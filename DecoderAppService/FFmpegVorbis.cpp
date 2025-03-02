//Copyright (c) Microsoft Corporation. All rights reserved.
#include "pch.h"
#include "FFmpegVorbis.h"

using namespace FFmpegPack;

/**
 * Modifies a Xiph Vorbis header exported by the MF Matroska
 * demuxer source, outputting a Vorbis format header for FFmpeg.
 *
 * @param pHeaderIn    the input header buffer.
 * @param inSize       the input header size in bytes.
 * @param ppHeaderOut  the output header. In this case same memory as input.
 * @param outSize      the output header size.
 */
HRESULT FFmpegVorbis::ProcessHeader(
	__in UINT8* pHeaderIn,
	UINT32 inSize,
	__out UINT8** ppHeaderOut,
	__out UINT32* outSize
) {
	// CANNOT do it in place bc memory must be aligned!
	
	VORBISFORMAT *pInVorbisData = (VORBISFORMAT*)pHeaderIn;
	DWORD headerSize1 = pInVorbisData->headerSize[0];
	DWORD headerSize2 = pInVorbisData->headerSize[1];

	*ppHeaderOut = (UINT8 *)av_mallocz(inSize);
	UINT8 *pos = *ppHeaderOut;

	// 8 bytes of header sizes + 1 byte 0x2
	*pos = 0x02; // Vorbis header type
	pos++;
	_WriteHeaderSize(&pos, headerSize1);
	_WriteHeaderSize(&pos, headerSize2);
	
	RtlCopyMemory(pos, ((VORBISFORMAT*)pHeaderIn) + 1, inSize - sizeof(VORBISFORMAT));
	
	*outSize = inSize;

	return S_OK;
}

/**
 * Writes a unsigned 32-bit integer into a bitstream in big-endian format.
 * Moves DOWN the memory. Modifies the bitstream position, such that when it
 * returns, pos points to a byte AFTER the written integer.
 *
 * @param pos   a pointer to where the function will start writing.
 *		mutated to point to the memory position right after where the 
 *      data was written.
 * @param size  the 32-bit integer to write.
 */
void FFmpegVorbis::_WriteHeaderSize(__inout UINT8** pos, UINT32 size)
{	
	UINT8 *pSize = (UINT8 *)&size;

	if (pSize[3] == 0xff)
	{
		**pos = 0xff;
		(*pos)++;
	}

	if (pSize[2] == 0xff)
	{
		**pos = 0xff;
		(*pos)++;
	}

	if (pSize[1] == 0xff)
	{
		**pos = 0xff;
		(*pos)++;
	}

	**pos = pSize[0];
	(*pos)++;
}
