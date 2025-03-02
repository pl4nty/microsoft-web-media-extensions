//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "pch.h"

namespace FFmpegPack {
	void PrintProcessInfo();

	void PrintGUID(GUID id);

	void PrintAllAttributes(IMFAttributes *atts);

	void PrintUHArray(UINT8 *arr, int n);

	void PrintMessage(char *msg);
}