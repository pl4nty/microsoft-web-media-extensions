//Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include "FFmpegTypes.h"

#ifdef FFMPEG_CODEC_Vorbis
#include "FFmpegVorbis.h"
#endif

namespace FFmpegPack
{
	/**
	 * A function pointer to a header processor.
	 *
	 * @param pHeaderIn   A pointer to the byte buffer holding the header.
	 * @param inSize      The size of the input header buffer.
	 * @param ppHeaderOut A pointer to the output header buffer.
	 * @param outSize     A pointer to the output header size.
	 * @return S_OK on success, an error code on failure.
	 */
	typedef HRESULT (*FFmpegHeaderProcessor)(__in UINT8* pHeaderIn, UINT32 inSize, __out UINT8** ppHeaderOut, __out UINT32* outSize);
}
