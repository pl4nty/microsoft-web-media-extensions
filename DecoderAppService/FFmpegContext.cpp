//Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include <mferror.h> // MF_E_*

#include "FFmpegContext.h"
#include "AudioTransformHelper.h"
#include "VideoTransformHelper.h"

using namespace FFmpegPack;

FFmpegContext::FFmpegContext() :
	m_Codec(nullptr),
	m_CodecContext(nullptr),
	m_bFormatChange(false)
{


}

FFmpegContext::~FFmpegContext()
{
	if (m_pCodecParams)
		avcodec_parameters_free(&m_pCodecParams);

	if (m_CodecContext)
		avcodec_free_context(&m_CodecContext);

	if (m_pFrame)
	{
		av_frame_unref(m_pFrame);
		av_freep(m_pFrame);
	}

	if (m_TransformHelper)
		delete m_TransformHelper;

	IMFSample *pSample = nullptr;
	while (!m_OutputQueue.IsEmpty())
	{
		 pSample = m_OutputQueue.RemoveHead();
		 pSample->Release();
	}
}

/**
 * Allocate all resources for decoding based on the set input
 * and output types.
 */
HRESULT FFmpegContext::Initialize(
	_In_    IMFMediaType *inputType, 
	_Inout_ IMFMediaType *outputType
) {
	HRESULT hr = (inputType && outputType) ? S_OK : E_POINTER;

	m_pInputType = inputType;
	m_pOutputType = outputType;
	
	// Find codec
	GUID inputMajorType, inputSubType;

	if (SUCCEEDED(hr))
		hr = inputType->GetGUID(MF_MT_MAJOR_TYPE, &inputMajorType);

	if (SUCCEEDED(hr))
		hr = inputType->GetGUID(MF_MT_SUBTYPE, &inputSubType);

	if (FAILED(hr)) return hr;


	if (SUCCEEDED(hr))
	{
		m_pCodecParams = avcodec_parameters_alloc();
		if (!m_pCodecParams) hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
		hr = FFmpegTypes::CodecParamsFromMediaType(m_pCodecParams, inputType, outputType);

	if(SUCCEEDED(hr))
	{
		AVCodecID inputCodecID = m_pCodecParams->codec_id;
		m_Codec = avcodec_find_decoder(inputCodecID);

		if (!m_Codec) hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
		hr = _CreateCodecContext();

	if (SUCCEEDED(hr))
	{
		if (inputMajorType == MFMediaType_Video && _VideoUpdateOutput(outputType))
			QueueFormatChange();
	}

	if (SUCCEEDED(hr))
	{
		if (inputMajorType == MFMediaType_Audio)
			m_TransformHelper = ref new AudioTransformHelper();
		else
			m_TransformHelper = ref new VideoTransformHelper();

		hr = m_TransformHelper->Initialize(m_CodecContext, inputType, outputType);
	}

	if (SUCCEEDED(hr))
	{
		m_pFrame = av_frame_alloc();
		if (!m_pFrame) hr = E_OUTOFMEMORY;
	}

	return hr;
}

/** Returns true if this context has been initialized. */
bool FFmpegContext::HasInitialized()
{
	return (m_pCodecParams && m_Codec && m_TransformHelper);
}

/** Decodes a compressed frame and queues the raw output. */
HRESULT FFmpegContext::Decode(_In_ AVPacket *pPacket) {
	HRESULT hr = (pPacket) ? S_OK : E_POINTER;

	// Format change must be fulfilled before new packets can be decoded.
	if (m_bFormatChange)
		return MF_E_NOTACCEPTING;

	if (SUCCEEDED(hr))
	{
		int sendPacketResult = avcodec_send_packet(m_CodecContext, pPacket);
		if (sendPacketResult == AVERROR(EAGAIN))
		{
			// The decoder should have been drained and always ready to access input
			_ASSERT(FALSE);
			hr = E_UNEXPECTED;
		}
		else if (sendPacketResult < 0)
		{
			// We failed to send the packet
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		// Try to get a frame from the decoder.
		int decodeFrame = avcodec_receive_frame(m_CodecContext, m_pFrame);

		// The decoder is empty, send a packet to it.
		if (decodeFrame == AVERROR(EAGAIN))
		{
			// The decoder doesn't have enough data to produce a frame,
			// return S_FALSE to indicate a partial frame
			return S_FALSE;
		}
		else if (decodeFrame < 0)
		{
			hr = E_FAIL;
		}

		if (SUCCEEDED(hr) && m_pFrame->nb_samples)
			m_pFrame->pkt_duration = FFmpegTypes::TimeToAVTime(m_pFrame->nb_samples, m_CodecContext->time_base);

		Platform::Array<BYTE>^ hBuffer;
		IMFSample *pSample = nullptr;
		if (SUCCEEDED(hr)) {
			hr = MFCreateSample(&pSample);
			if (pSample == nullptr) hr = E_OUTOFMEMORY;
		}

		if(SUCCEEDED(hr))
			hr = m_TransformHelper->ProcessDecodedFrame(m_pFrame, hBuffer);
		
		if (SUCCEEDED(hr))
		{
			hr = FFmpegTypes::SampleFromArray(pSample, hBuffer);
			delete hBuffer;
		}


		if (SUCCEEDED(hr))
		{
			if (m_pFrame->pts != AV_NOPTS_VALUE)
				hr = pSample->SetSampleTime(m_pFrame->pts);
			else
				hr = pSample->SetSampleTime(pPacket->pts);

			if (SUCCEEDED(hr)) {
				if(pPacket->duration > 0)
					hr = pSample->SetSampleDuration(pPacket->duration);
				else
					hr = pSample->SetSampleDuration(m_pFrame->pkt_duration);
			}
		}

		if (SUCCEEDED(hr))
			hr = QueueOutput(pSample);
	}

	return hr;
}

/**
 * Retrieves the next decoded sample.
 * 
 * @param ppSample  a pointer to the next sample.
 * @return S_OK on success, an error code on failure:
 *		MF_E_TRANSFORM_NEED_MORE_INPUT  if no sample is available.
 */
HRESULT FFmpegContext::GetNextSample(_Deref_out_ IMFSample **ppSample)
{
	HRESULT hr = (ppSample) ? S_OK : E_POINTER;
	
	if (m_OutputQueue.IsEmpty())
		hr = MF_E_TRANSFORM_NEED_MORE_INPUT;

	if (SUCCEEDED(hr))
	{
		IMFSample *pSample = m_OutputQueue.RemoveTail();
		hr = pSample->QueryInterface<IMFSample>(ppSample);
		pSample->Release();
		if (*ppSample == NULL) hr = E_UNEXPECTED;
	}

	return hr;
}

/** Returns true if there is a uncompressed output frame queued */
bool FFmpegContext::HasNextSample() {
	return (!m_OutputQueue.IsEmpty());
}

/** Resets the context. */
HRESULT FFmpegContext::FlushInput()
{
	HRESULT hr = S_OK;

	avcodec_free_context(&m_CodecContext);
	hr = _CreateCodecContext();

	GUID inputMajorType;

	if (SUCCEEDED(hr))
	{
		if (m_pInputType == nullptr)
			hr = MF_E_TRANSFORM_TYPE_NOT_SET;
		else
			hr = m_pInputType->GetGUID(MF_MT_MAJOR_TYPE, &inputMajorType);
	}

	if (SUCCEEDED(hr))
	{
		delete m_TransformHelper;

		if (inputMajorType == MFMediaType_Audio)
			m_TransformHelper = ref new AudioTransformHelper();
		else
			m_TransformHelper = ref new VideoTransformHelper();

		hr = m_TransformHelper->Initialize(m_CodecContext, m_pInputType, m_pOutputType);
	}

	return hr;
}

/**
 * Flushes current state of decoder. Is immediately ready for a new stream.
 *
 * @return S_OK on success, an error code on failure.
 */
HRESULT FFmpegContext::Flush()
{
	HRESULT hr = S_OK;
	IMFSample *pSample = nullptr;
	while (!m_OutputQueue.IsEmpty())
	{
		pSample = m_OutputQueue.RemoveHead();
		pSample->Release();
	}

	FlushInput();
	return hr;
}

/** Queues sample frame. */
HRESULT FFmpegContext::QueueOutput(IMFSample *pSample)
{
	m_OutputQueue.AddHead(pSample);
	return S_OK;
}

/** Creates the FFmpeg codec context. */
HRESULT FFmpegContext::_CreateCodecContext()
{
	HRESULT hr = (m_Codec)? S_OK : E_POINTER;

	if (SUCCEEDED(hr))
	{
		m_CodecContext = avcodec_alloc_context3(m_Codec);
		if (!m_CodecContext) hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		int codecParamsResult = avcodec_parameters_to_context(
			m_CodecContext,
			m_pCodecParams
		);
		if (codecParamsResult < 0) hr = E_FAIL;
	}

	// TODO : this is not thread safe - use locks?
	if (SUCCEEDED(hr))
	{
		int codecOpenResult = avcodec_open2(m_CodecContext, m_Codec, NULL);
		if(codecOpenResult < 0)
		{
			m_CodecContext = nullptr;
			hr = E_FAIL;
		}
	}

	return hr;
}

void FFmpegContext::QueueFormatChange(void)
{
	m_bFormatChange = true;
}

bool FFmpegContext::HasFormatChange(void)
{
	bool result = m_bFormatChange;
	m_bFormatChange = false;
	return result;
}

bool FFmpegContext::_VideoUpdateOutput(_Inout_ IMFMediaType *outputType)
{
	HRESULT hr = (outputType) ? S_OK : E_POINTER;
	bool result = false;

	// width and height
	UINT32 width, height;
	if (SUCCEEDED(hr))
		hr = MFGetAttributeSize(outputType, MF_MT_FRAME_SIZE, &width, &height);

	if (SUCCEEDED(hr))
	{
		if (m_CodecContext->width != width || m_CodecContext->height != height)
		{
			hr = MFSetAttributeSize(outputType, MF_MT_FRAME_SIZE, m_CodecContext->width, m_CodecContext->height);
			result = true;
		}
	}

	// Calls in this function are NOT expected to fail.
	_ASSERT(SUCCEEDED(hr)); 

	return result;
}
