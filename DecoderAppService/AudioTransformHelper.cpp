//Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "AudioTransformHelper.h"

#include "FFmpegTypes.h"

using namespace FFmpegPack;

/** Destroy the helper and free all resources. */
AudioTransformHelper::~AudioTransformHelper()
{
	if (m_pResampleContext)
		swr_free(&m_pResampleContext);

	m_pCodecContext = nullptr;

	if (m_resampledData)
		av_freep(&m_resampledData);
}

/** Initialize the helper and allocate all resources. */
HRESULT AudioTransformHelper::Initialize(
	_In_ AVCodecContext *pCodecContext, 
	_In_ IMFMediaType *inputType, 
	_In_ IMFMediaType *outputType
) {
	HRESULT hr = S_OK;
	int64 inChannelLayout, outChannelLayout;

	m_pCodecContext = pCodecContext;
	
	// Set default channel layout when the value is unknown (0)
	if(m_pCodecContext->channel_layout)
		inChannelLayout = m_pCodecContext->channel_layout;
	else
		inChannelLayout = av_get_default_channel_layout(m_pCodecContext->channels);

	outChannelLayout = av_get_default_channel_layout(m_pCodecContext->channels);

	GUID outputMFType;
	m_outputFormat = AV_SAMPLE_FMT_NONE;
	if (SUCCEEDED(outputType->GetGUID(MF_MT_SUBTYPE, &outputMFType)))
	{
		ARRAYTRANSLATE(FFMPEG_MFTYPES_OUTPUT_AUDIO, FFMPEG_AVTYPES_OUTPUT_AUDIO, outputMFType, m_outputFormat);
		if(m_outputFormat == AV_SAMPLE_FMT_NONE) return E_FAIL;
	}

	// Set up resampler to convert any PCM format to the selected output format.
	// TODO: Skip conversion if already in desired output format.
	m_pResampleContext = swr_alloc_set_opts(
		NULL,
		outChannelLayout,
		m_outputFormat,
		m_pCodecContext->sample_rate,
		inChannelLayout,
		m_pCodecContext->sample_fmt,
		m_pCodecContext->sample_rate,
		0,
		NULL
	);

	if (!m_pResampleContext)
		hr = E_OUTOFMEMORY;

	if (SUCCEEDED(hr) && swr_init(m_pResampleContext) < 0)
		hr = E_FAIL;

	if (SUCCEEDED(hr))
	{
		m_samples = 0;
		m_channels = 0;
		m_resampledData = nullptr;
	}

	return hr;
}

/** Write an uncompressed frame to a stream in float format. */
HRESULT AudioTransformHelper::ProcessDecodedFrame(
	_In_  AVFrame *pFrame,
	_Out_ Platform::Array<BYTE>^& phBuffer
) {
	// Resample uncompressed frame to AV_SAMPLE_FMT_FLT float format

	// m_resampledDataSize ends up = channels * nb_samples * sample size
	// Reuse m_resampledData buffer whenever size is the same
	if (!m_resampledData
		|| pFrame->channels != m_channels
		|| pFrame->nb_samples != m_samples)
	{
		if(m_resampledData) av_freep(&m_resampledData);

		m_resampledDataSize = av_samples_alloc(
			&m_resampledData,
			NULL,
			pFrame->channels,
			pFrame->nb_samples,
			m_outputFormat,
			0
		);
		m_channels = pFrame->channels;
		m_samples = pFrame->nb_samples;
	}

	
	int resampledSamplesCount = swr_convert(
		m_pResampleContext,
		&m_resampledData,
		m_resampledDataSize,
		(const BYTE **)pFrame->extended_data,
		pFrame->nb_samples
	);

	unsigned int samplesByteSize = (
		resampledSamplesCount * pFrame->channels * av_get_bytes_per_sample(m_outputFormat)
	);

	// Avoid extra copy here by using array reference instead of array.
	Platform::ArrayReference<BYTE> aBuffer(
		m_resampledData, 
		min(m_resampledDataSize, samplesByteSize)
	);
	
	phBuffer = aBuffer;

	return S_OK;
}