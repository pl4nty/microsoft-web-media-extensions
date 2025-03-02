//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once
#include "FFmpegTransformHelper.h"

extern "C"
{
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}

namespace FFmpegPack {
	ref class AudioTransformHelper :
		FFmpegTransformHelper
	{

	public:
		virtual ~AudioTransformHelper();

	internal:
		AudioTransformHelper() {};

		///////////////////////////////////////////////////////////
		//   FFmpegTransformHelper Methods
		///////////////////////////////////////////////////////////
		HRESULT Initialize(_In_ AVCodecContext *pCodecContext, _In_ IMFMediaType *inputType, _In_ IMFMediaType *outputType) override;
		HRESULT ProcessDecodedFrame(_In_ AVFrame *pFrame, _Out_ Platform::Array<BYTE>^& phBuffer) override;

	private:
		/** The codec context passed down from the FFmpegContext. */
		AVCodecContext *m_pCodecContext;

		/** The output float raw audio resampling context. */
		SwrContext* m_pResampleContext;

		/** The output uncompressed sample format. */
		AVSampleFormat m_outputFormat;

		/** Byte buffer of the output resampled data. */
		BYTE *m_resampledData;
		unsigned int m_resampledDataSize;

		// Some stream info so we can reuse memory
		int m_channels;
		int m_samples;
	};
};

