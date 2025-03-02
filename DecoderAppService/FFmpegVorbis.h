//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

extern "C"
{
#include <libavformat/avformat.h> // AVPacket, AVFrame
}

namespace FFmpegPack{
	/** Vorbis header format, exported by Matroska/WEBM MF Source. */
	struct VORBISFORMAT
	{
		DWORD channels;
		DWORD samplesPerSec;
		DWORD bitsPerSample;
		DWORD headerSize[3];
	};

	/** A simple vorbis header transformation. */
	class FFmpegVorbis
	{
	public:
		static HRESULT ProcessHeader(__in UINT8* pHeaderIn, UINT32 inSize, __out UINT8** ppHeaderOut, __out UINT32* outSize);

	private:
		static void _WriteHeaderSize(__inout UINT8** pos, UINT32 size);
	};

}