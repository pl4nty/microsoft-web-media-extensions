//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "pch.h"

extern "C"
{
#include <libavformat/avformat.h> // AVPacket, AVFrame, AVCodecContext
}

using namespace Windows::Storage::Streams;

namespace FFmpegPack {
	ref class FFmpegTransformHelper abstract {
			
	internal:
		
		/**
		 * Allocates all necessary resources for processing packets and frames.
		 * Anything that can fail should go here instead of constructor.
		 *
		 * @return S_OK on success, an error code on failure.
		 */
		virtual HRESULT Initialize(_In_ AVCodecContext *pCodecContext, _In_ IMFMediaType *inputType, _In_ IMFMediaType *outputType) = 0;

		/**
		 * Writes a frame into a data stream, optionally processing it first.
		 * If nothing is written to stream here there will be NO output.
		 *
		 * @param pFrame  (AVFrame)    the decoded frame produced by FFmpeg.
		 * @param sStream (DataWriter) a writeable output stream.
		 *
		 * @return S_OK on success, an error code on failure.
		 */
		virtual HRESULT ProcessDecodedFrame(_In_ AVFrame *pFrame, _Out_ Platform::Array<BYTE>^& phBuffer) = 0;

		/** Process packet before decoding - if needed. */
		//virtual HRESULT ProcessEncodedPacket(AVPacket *pPacket) = 0;
	};
}