//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "pch.h"

extern "C"
{
#include <libavformat/avformat.h> // AVPacket, AVFrame
}

#include "FFmpegTransformHelper.h"
#include "FFmpegTypes.h"

// basically a wrapper for the actual FFmpeg
// context as in:
//    - IO Context
//    - Format Context
//    - Codec Context
namespace FFmpegPack {
	ref class FFmpegContext sealed
	{
	public:
		FFmpegContext();
		virtual ~FFmpegContext();

	internal:

		HRESULT Initialize(_In_ IMFMediaType *inputType, _Inout_ IMFMediaType *outputType);
		bool HasInitialized(void);

		HRESULT Decode(_In_ AVPacket *pPacket);

		HRESULT GetNextSample(_Deref_out_ IMFSample **ppSample);
		bool HasNextSample(void);

		HRESULT FlushInput(void);
		HRESULT Flush(void);

		bool HasFormatChange(void);

	private:
		// MFT
		/** The current output type. Raw audio/video in this case. */
		IMFMediaType* m_pOutputType;

		/** The current input type. Compressed audio/video in this case. */
		IMFMediaType* m_pInputType;

		// FFmpeg
		AVCodec* m_Codec;
		AVCodecContext * m_CodecContext;
		AVCodecParameters *m_pCodecParams;

		AVFrame *m_pFrame;
		

		// Others
		CAtlList<IMFSample*> m_OutputQueue;
		FFmpegTransformHelper^ m_TransformHelper;
		bool m_bFormatChange;

		// Methods
		HRESULT QueueOutput(_In_ IMFSample *pSample);
		HRESULT _CreateCodecContext(void);

		void QueueFormatChange(void);

		bool _VideoUpdateOutput(_Inout_ IMFMediaType *outputType);
	};
};
