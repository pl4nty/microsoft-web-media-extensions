//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "pch.h"

extern "C"
{
#include <libavformat/avformat.h> // AVPacket
}

#include "DecoderBase.h" // DecoderBase



using namespace Windows::Foundation::Collections; // IPropertySet

namespace FFmpegPack {
	/** Main MFT Implementation for FFmpeg-based decoding. */
	class FFmpegDecoderMFT final : public DecoderBase
	{
	public:
		static HRESULT CreateInstance(__deref_out IMFTransform ** ppFFmpegMFT);

		
		///////////////////////////////////////////////////////////
		//   IMFTransform Interface
		///////////////////////////////////////////////////////////
		virtual STDMETHODIMP ProcessInput(__in DWORD dwInputStreamID, __in IMFSample * pSample, __in DWORD dwFlags);
		virtual STDMETHODIMP ProcessMessage(__in MFT_MESSAGE_TYPE eMessage, __in ULONG_PTR ulParam);
		virtual STDMETHODIMP ProcessOutput(__in DWORD dwFlags, __in DWORD cOutputBufferCount, __inout_ecount(cOutputBufferCount) MFT_OUTPUT_DATA_BUFFER * pOutputSamples, __out DWORD *pdwStatus);

	private:
		FFmpegDecoderMFT();
		virtual ~FFmpegDecoderMFT();

		///////////////////////////////////////////////////////////
		//   Private properties
		///////////////////////////////////////////////////////////

		// State

		/** Whether MFT is currently streaming samples. */
		bool _bStreaming;

		/** Whether MFT is currently being drained. */
		bool _bDraining;

		/** Reused input media packet. */
		AVPacket *_pPacket;


		///////////////////////////////////////////////////////////
		//   Private decoder methods
		///////////////////////////////////////////////////////////
		HRESULT FFmpegDecoderMFT::StartStream();
	};
}
