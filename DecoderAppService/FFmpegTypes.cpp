//Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "FFmpegTypes.h"

#include "FFmpegCodecs.h"
#include "DebugUtils.h"


using namespace FFmpegPack;

/**
 * Create a FFmpeg Packet from a MF sample.
 * Packet must already be initialized. Data must be freed by av_packet_free().
 */
HRESULT FFmpegTypes::PacketFromSample(
	_Out_ AVPacket *pPacket,
	_In_ IMFSample *pSample
) {
	HRESULT hr = (pPacket && pSample)? S_OK: E_POINTER;

	/* A intermediary media buffer with concat. buffers of the sample. */
	IMFMediaBuffer *pMediaBuffer = nullptr;

	/* The raw bytes of the sample, along with its total bytes count
	 and the currently used count. */
	BYTE *pbBuffer = nullptr;
	DWORD maxLength, curLength;

	/* An internal buffer used by the AVPacket */
	uint8_t * pFFBuffer = nullptr;
	

	if (SUCCEEDED(hr))
	{
		hr = pSample->ConvertToContiguousBuffer(&pMediaBuffer);
		if (!pMediaBuffer) return E_UNEXPECTED;
	}

	if (SUCCEEDED(hr))
		hr = pMediaBuffer->Lock(&pbBuffer, &maxLength, &curLength);

	if (SUCCEEDED(hr))
	{
		if (pPacket->data && pPacket->size == curLength)
		{
			// Reuse packet buffer
			pFFBuffer = pPacket->data;
		}
		else
		{
			// Free old packet and allocate new packet/buffer
			if (pPacket->data) av_packet_unref(pPacket);

			pFFBuffer = (uint8_t *)av_malloc(curLength + AV_INPUT_BUFFER_PADDING_SIZE);
			if (pFFBuffer == nullptr) hr = E_OUTOFMEMORY;
		}
	}

	// TODO memcpy could fail? How to test?
	if (SUCCEEDED(hr))
		RtlCopyMemory(pFFBuffer, pbBuffer, curLength);

	if (SUCCEEDED(hr) && pFFBuffer != pPacket->data)
	{
		// Attempt to set buffer info for packet
		if (av_packet_from_data(pPacket, pFFBuffer, curLength) != 0)
			hr = E_UNEXPECTED;
	}


	// Copy time and duration
	LONGLONG sampleDuration, sampleTime;
	if (SUCCEEDED(hr))
		hr = pSample->GetSampleDuration(&sampleDuration);

	if (SUCCEEDED(hr))
		hr = pSample->GetSampleTime(&sampleTime);

	if (SUCCEEDED(hr))
	{
		pPacket->duration = sampleDuration;
		pPacket->pts = sampleTime;
	}

	// Cleanup even on failure
	hr = pMediaBuffer->Unlock();
	pMediaBuffer->Release();

	return hr;
}

/**
 * Create a MF sample from a buffer. Sample must already be initialized.
 */
HRESULT FFmpegTypes::SampleFromBuffer(
	_Out_ IMFSample *pSample, 
	_In_ IBuffer^ hBuffer
) {
	HRESULT hr = (pSample && hBuffer)? S_OK : E_POINTER;

	IMFMediaBuffer *pMediaBuff = nullptr;
	int buffSize = hBuffer->Length;

	// Not needed...allocating own buffers with arbitrary(ish) size
	//_ASSERT(buffSize <= FFMPEG_OUTPUT_BUFFER_SIZE);

	if(SUCCEEDED(hr))
	{
		MFCreateMemoryBuffer(buffSize, &pMediaBuff);
		if (pMediaBuff == nullptr) hr = E_OUTOFMEMORY;
	}

	BYTE *pbBuffer = nullptr;
	DWORD buffMaxLength, buffCurrentLength;
	
	if(SUCCEEDED(hr))
	{
		hr = pMediaBuff->Lock(&pbBuffer, &buffMaxLength, &buffCurrentLength);
		if (pbBuffer == nullptr) hr = E_UNEXPECTED;
	}

	// Sanity check
	if (SUCCEEDED(hr))
		hr = (buffCurrentLength == 0) ? S_OK : E_UNEXPECTED;

	// This is what happens when you stray from C too much.
	// DataWriter -> IBuffer -> DataReader -> Array<BYTE> -> BYTE* -> IMFMediaBuffer

	DataReader ^reader = nullptr;
	if (SUCCEEDED(hr))
	{
		reader = DataReader::FromBuffer(hBuffer);
		if (reader == nullptr) hr = E_OUTOFMEMORY;
	}

	Platform::Array<BYTE>^ value = nullptr;
	if (SUCCEEDED(hr))
	{
		value = ref new Platform::Array<BYTE>(buffSize);
		if (value == nullptr) hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		reader->ReadBytes(value);
		RtlCopyMemory((void *)pbBuffer, (void *)value->Data, buffSize * sizeof(BYTE));
	}

	pMediaBuff->SetCurrentLength(buffSize);
	
	hr = pMediaBuff->Unlock();

	if (SUCCEEDED(hr))
		hr = pSample->AddBuffer(pMediaBuff);
	
	return hr;
}

/**
* Create a MF sample from a byte array. Sample must already be initialized.
*/
HRESULT FFmpegTypes::SampleFromArray(
	_Out_ IMFSample *pSample,
	_In_ Platform::Array<BYTE> ^hArray
) {
	HRESULT hr = (pSample && hArray) ? S_OK : E_POINTER;

	IMFMediaBuffer *pMediaBuff = nullptr;
	int buffSize = hArray->Length;

	if (SUCCEEDED(hr))
	{
		MFCreateMemoryBuffer(buffSize, &pMediaBuff);
		if (pMediaBuff == nullptr) hr = E_OUTOFMEMORY;
	}

	BYTE *pbBuffer = nullptr;
	DWORD buffMaxLength, buffCurrentLength;

	if (SUCCEEDED(hr))
	{
		hr = pMediaBuff->Lock(&pbBuffer, &buffMaxLength, &buffCurrentLength);
		if (pbBuffer == nullptr) hr = E_UNEXPECTED;
	}

	// Sanity check
	if (SUCCEEDED(hr))
		hr = (buffCurrentLength == 0) ? S_OK : E_UNEXPECTED;

	if (SUCCEEDED(hr))
		RtlCopyMemory((void *)pbBuffer, (void *)hArray->Data, buffSize * sizeof(BYTE));

	pMediaBuff->SetCurrentLength(buffSize);

	hr = pMediaBuff->Unlock();

	if (SUCCEEDED(hr))
	{
		hr = pSample->AddBuffer(pMediaBuff);
		pMediaBuff->Release();
	}

	return hr;
}

/**
 * Clones a sample into an already-allocated space.
 * Note: ONLY copies the FIRST buffer of the input sample.
 *
 * @param spSourceSample  the input sample.
 * @param spDestSample    the output sample, already allocated.
 *
 * @return S_OK on success, an error code on failure.
 */
HRESULT FFmpegTypes::CopySample(
	_In_  CComPtr<IMFSample> spSourceSample, 
	_Out_ CComPtr<IMFSample> spDestSample
) {
	HRESULT hr = S_OK;
	LONGLONG sampleTime, sampleDuration;

	if (SUCCEEDED(spSourceSample->GetSampleTime(&sampleTime)))
		spDestSample->SetSampleTime(sampleTime);

	if (SUCCEEDED(spSourceSample->GetSampleDuration(&sampleDuration)))
		spDestSample->SetSampleDuration(sampleDuration);

	
	CComPtr<IMFMediaBuffer> spSourceBuffer, spDestBuffer;
	BYTE *pbSourceRawBuffer = NULL;
	BYTE *pbDestRawBuffer = NULL;
	DWORD dwSourceLength, dwDestLength, dwMinLength;

	hr = spSourceSample->GetBufferByIndex(0, &spSourceBuffer);

	if (SUCCEEDED(hr))
		hr = spSourceBuffer->Lock(&pbSourceRawBuffer, &dwSourceLength, NULL);

	if (SUCCEEDED(hr))
		hr = spDestSample->GetBufferByIndex(0, &spDestBuffer);

	if (SUCCEEDED(hr))
		hr = spDestBuffer->Lock(&pbDestRawBuffer, &dwDestLength, NULL);

	dwMinLength = (dwDestLength > dwSourceLength)? dwSourceLength : dwDestLength;

	CopyMemory(pbDestRawBuffer, pbSourceRawBuffer, dwMinLength);
	spDestBuffer->SetCurrentLength(dwMinLength);

	if (spSourceBuffer) spSourceBuffer->Unlock();
	if (spDestBuffer) spDestBuffer->Unlock();

	return hr;
}

/**
 * Converts units of time in terms of samples to 100-nanosecond units
 * based on the current sample rate.
 */
LONGLONG FFmpegTypes::TimeToAVTime(
	LONGLONG time,
	AVRational timeBase
) {
	return LONGLONG(av_q2d(timeBase) * 10000000 * time);
}

/**
 * Creates FFmpeg codec parameters from a MF media type.
 * ONLY supports audio and video.
 */
HRESULT FFmpegTypes::CodecParamsFromMediaType(
	_Out_ AVCodecParameters *pCodecParams,
	_In_  IMFMediaType *pMediaTypeIn,
	_In_  IMFMediaType *pMediaTypeOut
) {
	HRESULT hr = (pCodecParams && pMediaTypeIn && pMediaTypeOut) ? S_OK : E_POINTER;
	GUID majorType, subType;

	// codec_type
	if (SUCCEEDED(hr))
		hr = pMediaTypeIn->GetGUID(MF_MT_MAJOR_TYPE, &majorType);

	if (SUCCEEDED(hr))
	{
		if (majorType == MFMediaType_Audio)
			pCodecParams->codec_type = AVMEDIA_TYPE_AUDIO;
		else if (majorType == MFMediaType_Video)
			pCodecParams->codec_type = AVMEDIA_TYPE_VIDEO;
		else
			hr = E_INVALIDARG;
	}

	// codec_id
	if (SUCCEEDED(hr))
		hr = pMediaTypeIn->GetGUID(MF_MT_SUBTYPE, &subType);

	AVCodecID inputCodecID = AV_CODEC_ID_NONE;
	if (SUCCEEDED(hr)) {
		if (majorType == MFMediaType_Audio)
		{
			ARRAYTRANSLATE(
				FFMPEG_MFTYPES_INPUT_AUDIO,
				FFMPEG_AVTYPES_INPUT_AUDIO,
				subType,
				inputCodecID
			);
		}
		else if (majorType == MFMediaType_Video)
		{
			ARRAYTRANSLATE(
				FFMPEG_MFTYPES_INPUT_VIDEO,
				FFMPEG_AVTYPES_INPUT_VIDEO,
				subType,
				inputCodecID
			);
		}
		else
		{
			hr = E_INVALIDARG;
		}
	}

	if (SUCCEEDED(hr) && inputCodecID == AV_CODEC_ID_NONE)
		hr = E_INVALIDARG;
	

	if (SUCCEEDED(hr))
		pCodecParams->codec_id = inputCodecID;
	
	
	// extradata and extradata_size
	UINT32 userDataSize = 0;
	UINT8 *userDataBuff = nullptr;
	UINT32 processedDataSize = 0;
	UINT8 *processedDataBuff = nullptr;

	if (SUCCEEDED(hr)) {
		hr = pMediaTypeIn->GetBlobSize(MF_MT_USER_DATA, &userDataSize);
		if (SUCCEEDED(hr))
		{
			userDataBuff = (UINT8 *)av_mallocz(userDataSize + AV_INPUT_BUFFER_PADDING_SIZE);
			if (userDataBuff == nullptr) hr = E_OUTOFMEMORY;

			if (SUCCEEDED(hr))
				hr = pMediaTypeIn->GetBlob(MF_MT_USER_DATA, userDataBuff, userDataSize, &userDataSize);
			
		}
		else
		{
			// Empty header...which is okay for certain formats
			userDataSize = 0;
			userDataBuff = nullptr;

			hr = S_OK;
		}
	}

	
	
	if (SUCCEEDED(hr))
	{
		// TODO make this less hard-codey
		if (subType == MFAudioFormat_Vorbis && userDataBuff != nullptr)
		{
#ifdef FFMPEG_CODEC_Vorbis
			hr = FFmpegVorbis::ProcessHeader(userDataBuff, userDataSize, &processedDataBuff, &processedDataSize);
#else
			hr = MF_E_INVALIDMEDIATYPE;
#endif
		}
		else 
		{
			processedDataBuff = userDataBuff;
			processedDataSize = userDataSize;
		}
	}

	if (SUCCEEDED(hr) && userDataBuff != processedDataBuff)
	{
		av_freep(&userDataBuff);
	}

	if (SUCCEEDED(hr)) 
	{
		pCodecParams->extradata = processedDataBuff;
		pCodecParams->extradata_size = processedDataSize;
	}

	// bits_per_coded_sample
	// bits_per_raw_sample
	

	// format-specific
	if (SUCCEEDED(hr))
	{
		if (majorType == MFMediaType_Audio)
			hr = _AudioCodecParams(pCodecParams, pMediaTypeIn, pMediaTypeOut);
		else
			hr = _VideoCodecParams(pCodecParams, pMediaTypeIn, pMediaTypeOut);
	}

	return hr;
}

/** Audio parameters from media type. */
HRESULT FFmpegTypes::_AudioCodecParams(
	_Inout_ AVCodecParameters *pCodecParams, 
	_In_ IMFMediaType *pMediaTypeIn, 
	_In_ IMFMediaType *pMediaTypeOut
) {
	HRESULT hr = (pCodecParams && pMediaTypeIn && pMediaTypeOut) ? S_OK : E_POINTER;

	// format
	//  - audio: the sample format, the value corresponds to enum AVSampleFormat.
	
	

	// bit_rate - average bytes per second
	UINT32 bitRate;
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bitRate);
		if (SUCCEEDED(hr))
			pCodecParams->bit_rate = bitRate;
		else
			hr = S_OK; // Optional
	}
	

	// channels
	UINT32 channels;
	if (SUCCEEDED(hr))
		hr = pMediaTypeIn->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);

	if (SUCCEEDED(hr))
		pCodecParams->channels = channels;

	// channel_layout
	UINT32 channelMask;
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeOut->GetUINT32(MF_MT_AUDIO_CHANNEL_MASK, &channelMask);

		if (SUCCEEDED(hr))
		{
			pCodecParams->channel_layout = channelMask;
		}
		else
		{
			pCodecParams->channel_layout = av_get_default_channel_layout(channels);
			hr = S_OK;
		}
	}
	
	// sample_rate - samples per second
	UINT32 sampleRate;
	if (SUCCEEDED(hr))
		hr = pMediaTypeIn->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);

	if (SUCCEEDED(hr))
		pCodecParams->sample_rate = sampleRate;

	// block_align
	UINT32 blockAlignment;
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeOut->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlignment);
		if (SUCCEEDED(hr)) pCodecParams->block_align = blockAlignment;
		else hr = S_OK; // Optional
	}
	

	// frame_size
	UINT32 frameSize;
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeOut->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_BLOCK, &frameSize);
		if (SUCCEEDED(hr)) pCodecParams->frame_size = frameSize;
		else hr = S_OK; // Optional
	}
		

	// initial_padding 0?
	// seek_preroll

	return hr;
}

/** Video parameters from media type. */
HRESULT FFmpegTypes::_VideoCodecParams(
	_Inout_ AVCodecParameters *pCodecParams, 
	_In_ IMFMediaType *pMediaTypeIn, 
	_In_ IMFMediaType *pMediaTypeOut
) {
	// TODO some parameters could be optional, must check
	HRESULT hr = (pCodecParams && pMediaTypeIn) ? S_OK : E_POINTER;

	

	// width and height
	UINT32 width, height;
	if (SUCCEEDED(hr))
		hr = MFGetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, &width, &height);

	if (SUCCEEDED(hr))
	{
		pCodecParams->width = width;
		pCodecParams->height = height;
	}

	// bit_rate - average bitrate
	UINT32 bitRate;
	if (SUCCEEDED(hr))
	{
		if (SUCCEEDED(pMediaTypeIn->GetUINT32(MF_MT_AVG_BITRATE, &bitRate)))
			pCodecParams->bit_rate = bitRate;
	}

	// sample_aspect_ratio
	UINT32 aspectNum, aspectDenom;
	if (SUCCEEDED(hr))
	{
		hr = MFGetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, &aspectNum, &aspectDenom);

		if (SUCCEEDED(hr))
		{
			AVRational aspect = av_make_q(aspectNum, aspectDenom);
			pCodecParams->sample_aspect_ratio = aspect;
		}
		else hr = S_OK; // Optional
	}

	// field_order - interlaced
	UINT32 interlaceMode;
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->GetUINT32(MF_MT_INTERLACE_MODE, &interlaceMode);

		AVFieldOrder fieldOrder = AV_FIELD_UNKNOWN;
		if (SUCCEEDED(hr))
		{
			ARRAYTRANSLATE(FFMPEG_MFTYPES_INTERLACE, FFMPEG_AVTYPES_INTERLACE, interlaceMode, fieldOrder);
			if (fieldOrder == AV_FIELD_UNKNOWN) hr = E_UNEXPECTED;
		}

		if (SUCCEEDED(hr))
			pCodecParams->field_order = fieldOrder;

		hr = S_OK; // Optional
	}

	//enum AVColorRange color_range;
	// color_primaries;
	MFVideoPrimaries videoPrimaries;

	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->GetUINT32(MF_MT_VIDEO_PRIMARIES, (UINT32 *)&videoPrimaries);

		AVColorPrimaries colorPrimaries = AVCOL_PRI_UNSPECIFIED;
		if (SUCCEEDED(hr))
		{
			ARRAYTRANSLATE(FFMPEG_MFTYPES_VIDEO_PRIMARIES, FFMPEG_AVTYPES_VIDEO_PRIMARIES, videoPrimaries, colorPrimaries);
			if (colorPrimaries == AVCOL_PRI_UNSPECIFIED) hr = E_UNEXPECTED;
		}

		if (SUCCEEDED(hr))
			pCodecParams->color_primaries = colorPrimaries;

		hr = S_OK; // Optional
	}

	// color_trc;
	MFVideoTransferFunction transferFunction;
	
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->GetUINT32(MF_MT_TRANSFER_FUNCTION, (UINT32 *)&transferFunction);

		AVColorTransferCharacteristic transferChr = AVCOL_TRC_UNSPECIFIED;
		if (SUCCEEDED(hr))
		{
			ARRAYTRANSLATE(FFMPEG_MFTYPES_VIDEO_TRANSFER, FFMPEG_AVTYPES_VIDEO_TRANSFER, transferFunction, transferChr);
			if (transferChr == AVCOL_TRC_UNSPECIFIED) hr = E_UNEXPECTED;
		}

		if (SUCCEEDED(hr))
			pCodecParams->color_trc = transferChr;

		hr = S_OK;
	}

	// color_space;
	MFVideoChromaSubsampling chromaSubsampling;

	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->GetUINT32(MF_MT_VIDEO_CHROMA_SITING, (UINT32 *)&chromaSubsampling);

		AVChromaLocation chromaLocation = AVCHROMA_LOC_UNSPECIFIED;
		if (SUCCEEDED(hr))
		{
			ARRAYTRANSLATE(FFMPEG_MFTYPES_VIDEO_CHROMA, FFMPEG_AVTYPES_VIDEO_CHROMA, chromaSubsampling, chromaLocation);
			if (chromaLocation == AVCHROMA_LOC_UNSPECIFIED) hr = E_UNEXPECTED;
		}

		if (SUCCEEDED(hr))
			pCodecParams->chroma_location = chromaLocation;

		hr = S_OK;
	}

	// chroma_location;
	MFVideoTransferMatrix transferMatrix;

	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->GetUINT32(MF_MT_YUV_MATRIX, (UINT32 *)&transferMatrix);

		AVColorSpace colorSpace = AVCOL_SPC_UNSPECIFIED;
		if (SUCCEEDED(hr))
		{
			ARRAYTRANSLATE(FFMPEG_MFTYPES_VIDEO_COLOR_SPACE, FFMPEG_AVTYPES_VIDEO_COLOR_SPACE, transferMatrix, colorSpace);
			if (colorSpace == AVCOL_SPC_UNSPECIFIED) hr = E_UNEXPECTED;
		}

		if (SUCCEEDED(hr))
			pCodecParams->color_space = colorSpace;

		hr = S_OK;
	}

	// video_delay

	// profile - MPEG2/H.264
	UINT32 profile;
	if (SUCCEEDED(pMediaTypeIn->GetUINT32(MF_MT_MPEG2_PROFILE, &profile)))
		pCodecParams->profile = profile;
		

	// level   - MPEG2/H.264
	UINT32 level;
	if (SUCCEEDED(pMediaTypeIn->GetUINT32(MF_MT_MPEG2_LEVEL, &level)))
		pCodecParams->level = level;

	// format
	//  - video: the pixel format, the value corresponds to enum AVPixelFormat.
	GUID typeOut;
	if (SUCCEEDED(pMediaTypeOut->GetGUID(MF_MT_SUBTYPE, &typeOut)))
	{
		// translate
		AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;

		ARRAYTRANSLATE(FFMPEG_MFTYPES_OUTPUT_VIDEO, FFMPEG_AVTYPES_OUTPUT_VIDEO, typeOut, pixelFormat);
		if (pixelFormat == AV_PIX_FMT_NONE) hr = E_UNEXPECTED;

		if (SUCCEEDED(hr))
		{
			pCodecParams->format = pixelFormat;
		}
	}

	// codec_tag - FOURCC, only on desktop
#ifdef MF_MT_ORIGINAL_4CC
	UINT32 codecTag;
	if (SUCCEEDED(hr))
		hr = pMediaType->GetUINT32(MF_MT_ORIGINAL_4CC, &codecTag);

	if (SUCCEEDED(hr))
		pCodecParams->codec_tag = codecTag;
#endif

	return hr;
}
