//Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "VideoTransformHelper.h"

#include "FFmpegTypes.h"

using namespace FFmpegPack;

VideoTransformHelper::VideoTransformHelper() {
	
}

VideoTransformHelper::~VideoTransformHelper() 
{
	if (m_VideoBufferData)
		av_freep(m_VideoBufferData);

	if (m_pCodecContext)
		m_pCodecContext = nullptr;

	if (m_pScaleContext)
		sws_freeContext(m_pScaleContext);
}

HRESULT VideoTransformHelper::Initialize(
	_In_ AVCodecContext *pCodecContext, 
	_In_ IMFMediaType *inputType, 
	_In_ IMFMediaType *outputType
) {
	HRESULT hr = (pCodecContext)? S_OK: E_POINTER;

	if (FAILED(hr)) return hr;

	m_pCodecContext = pCodecContext;

	GUID outputMFType;
	m_outputFormat = AV_PIX_FMT_NONE;
	if (SUCCEEDED(outputType->GetGUID(MF_MT_SUBTYPE, &outputMFType)))
	{
		ARRAYTRANSLATE(FFMPEG_MFTYPES_OUTPUT_VIDEO, FFMPEG_AVTYPES_OUTPUT_VIDEO, outputMFType, m_outputFormat);
		if(m_outputFormat == AV_SAMPLE_FMT_NONE) return E_FAIL;
	}

	// Only NV12 supported right now, change ProcessDecodedFrame and remove
	_ASSERT(m_outputFormat == AV_PIX_FMT_NV12);

	if (SUCCEEDED(hr))
		hr = _CreateScaleContext();

	return hr;
}

HRESULT VideoTransformHelper::_CreateScaleContext(void)
{
	HRESULT hr = (m_pCodecContext && m_outputFormat)? S_OK: E_POINTER;

	// Setup software scaler to convert any decoder pixel format (e.g. YUV420P)
	// to the selected output format.
	m_pScaleContext = sws_getContext(
		m_pCodecContext->width,
		m_pCodecContext->height,
		m_pCodecContext->pix_fmt,
		m_pCodecContext->width,
		m_pCodecContext->height,
		m_outputFormat,
		SWS_BICUBIC,
		NULL,
		NULL,
		NULL
	);

	if (m_pScaleContext == nullptr)
		hr = E_OUTOFMEMORY;


	if (SUCCEEDED(hr))
	{
		m_inputFormat = m_pCodecContext->pix_fmt;
		int allocResult = av_image_alloc(
			m_VideoBufferData,
			m_VideoBufferLineSizes,
			m_pCodecContext->width,
			m_pCodecContext->height,
			m_outputFormat,
			1
		);

		if (allocResult < 0)
			hr = E_FAIL;
	}

	return hr;
}


HRESULT VideoTransformHelper::ProcessDecodedFrame(
	_In_  AVFrame *pFrame,
	_Out_ Platform::Array<BYTE>^& phBuffer
) {
	HRESULT hr = S_OK;

	if (m_inputFormat != m_pCodecContext->pix_fmt) {
		// Decoder found better format match, need to recreate context.
		if (m_VideoBufferData)
			av_freep(m_VideoBufferData);

		if (m_pScaleContext)
			sws_freeContext(m_pScaleContext);

		hr = _CreateScaleContext();
		if (FAILED(hr)) return hr;
	}

	// Convert decoded video pixel format to NV12 using FFmpeg software scaler
	int scaleResult = sws_scale(
		m_pScaleContext,
		(const BYTE **)(pFrame->data),
		pFrame->linesize,
		0,
		m_pCodecContext->height,
		m_VideoBufferData,
		m_VideoBufferLineSizes
	);

	if (scaleResult < 0)
		hr = E_FAIL;

	if (SUCCEEDED(hr))
	{
		int YSize = m_VideoBufferLineSizes[0] * m_pCodecContext->height;
		int UVSize = m_VideoBufferLineSizes[1] * m_pCodecContext->height / 2;
		auto outBuffer = ref new Platform::Array<BYTE>(YSize + UVSize);

		RtlCopyMemory(outBuffer->Data, m_VideoBufferData[0], YSize);
		RtlCopyMemory(outBuffer->Data + YSize, m_VideoBufferData[1], UVSize);
		
		phBuffer = outBuffer;
	}

	// Don't set a timestamp on S_FALSE
	// Try to get the best effort timestamp for the frame.
	if (hr == S_OK)
		pFrame->pts = pFrame->best_effort_timestamp;

	return S_OK;
}
