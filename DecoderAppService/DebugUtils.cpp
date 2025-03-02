//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "pch.h"
#include "DebugUtils.h"

using namespace FFmpegPack;

void FFmpegPack::PrintProcessInfo() {
	_RPT1(_CRT_WARN, "[%X-", GetCurrentProcessId());
	_RPT1(_CRT_WARN, "%X] ", GetCurrentThreadId());
}

void FFmpegPack::PrintGUID(GUID id) {
	if (id == MF_MT_AUDIO_AVG_BYTES_PER_SECOND)
		return _RPT0(_CRT_WARN, "MF_MT_AUDIO_AVG_BYTES_PER_SECOND");
	else if (id == MF_MT_AUDIO_NUM_CHANNELS)
		return _RPT0(_CRT_WARN, "MF_MT_AUDIO_NUM_CHANNELS");
	else if (id == MF_MT_COMPRESSED)
		return _RPT0(_CRT_WARN, "MF_MT_COMPRESSED");
	else if (id == MF_MT_MAJOR_TYPE)
		return _RPT0(_CRT_WARN, "MF_MT_MAJOR_TYPE");
	else if (id == MF_MT_SUBTYPE)
		return _RPT0(_CRT_WARN, "MF_MT_SUBTYPE");
	else if (id == MF_MT_USER_DATA)
		return _RPT0(_CRT_WARN, "MF_MT_USER_DATA");
	else if (id == MF_MT_AUDIO_SAMPLES_PER_SECOND)
		return _RPT0(_CRT_WARN, "MF_MT_AUDIO_SAMPLES_PER_SECOND");
	else if (id == MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND)
		return _RPT0(_CRT_WARN, "MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND");
	else if (id == MFMediaType_Video)
		return _RPT0(_CRT_WARN, "MFMediaType_Video");
	else if (id == MFMediaType_Audio)
		return _RPT0(_CRT_WARN, "MFMediaType_Audio");
	else if (id == MFAudioFormat_Vorbis)
		return _RPT0(_CRT_WARN, "MFAudioFormat_Vorbis");
	else if (id == MFAudioFormat_Float)
		return _RPT0(_CRT_WARN, "MFAudioFormat_Float");
	else if (id == MF_MT_AUDIO_BLOCK_ALIGNMENT)
		return _RPT0(_CRT_WARN, "MF_MT_AUDIO_BLOCK_ALIGNMENT");
	else if (id == MF_MT_AUDIO_CHANNEL_MASK)
		return _RPT0(_CRT_WARN, "MF_MT_AUDIO_CHANNEL_MASK");
	else if (id == MF_MT_AUDIO_VALID_BITS_PER_SAMPLE)
		return _RPT0(_CRT_WARN, "MF_MT_AUDIO_VALID_BITS_PER_SAMPLE");
	else if (id == MF_MT_AUDIO_BITS_PER_SAMPLE)
		return _RPT0(_CRT_WARN, "MF_MT_AUDIO_BITS_PER_SAMPLE");
	//else if (id == MF_MT_CONTAINER_SUBTYPE_NAME)


	_RPTN(_CRT_WARN, "{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
		id.Data1, id.Data2, id.Data3,
		id.Data4[0], id.Data4[1], id.Data4[2], id.Data4[3],
		id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
}

void FFmpegPack::PrintAllAttributes(IMFAttributes *atts) {
	int i = 0;

	GUID guid;
	PROPVARIANT val;
	//OLECHAR *guidStr;

	UINT32 int32Val;
	double doubleVal;
	GUID guidVal;

	WCHAR stringVal[256] = { 0 };

	_RPT0(_CRT_WARN, "\nAttributes\n");
	while (SUCCEEDED(atts->GetItemByIndex(i, &guid, &val))) {

		_RPT1(_CRT_WARN, "[%d] ", i);
		PrintGUID(guid);
		_RPT0(_CRT_WARN, "   -   ");

		if (SUCCEEDED(atts->GetUINT32(guid, &int32Val)))
			_RPT1(_CRT_WARN, "%d", int32Val);
		else if (SUCCEEDED(atts->GetDouble(guid, &doubleVal)))
			_RPT1(_CRT_WARN, "%f", doubleVal);
		else if (SUCCEEDED(atts->GetGUID(guid, &guidVal)))
			PrintGUID(guidVal);
		else if (SUCCEEDED(atts->GetString(guid, stringVal, 256, &int32Val)))
			_RPT1(_CRT_WARN, "%s", stringVal);
		else
			_RPT0(_CRT_WARN, "?");

		_RPT0(_CRT_WARN, "\n");
		i++;
	}
	_RPT0(_CRT_WARN, "END Attributes\n\n");
}

void FFmpegPack::PrintUHArray(UINT8 *arr, int n)
{
	for (int i = 0; i < n; i++) {
		_RPT1(_CRT_WARN, "%02hhX ", arr[i]);
	}
	_RPT0(_CRT_WARN, "\n");
}

void FFmpegPack::PrintMessage(char *msg) {
	PrintProcessInfo();
	_RPT1(_CRT_WARN, " %s", msg);
	_RPT0(_CRT_WARN, "\n");
}