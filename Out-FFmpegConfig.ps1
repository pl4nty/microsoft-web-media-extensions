#!/usr/bin/env powershell
#########################################################
# Copyright (C) Microsoft. All rights reserved.
# 
# Writes out all the setting files necessary for a
# FFmpeg-based codec pack, and builds FFmpeg.
#
# Env Variables:
# - BuildOS:        Target os, default "win10"
# - BuildPlatform:  Target platform, default "x64"
#                    may be comma-separated list.
#########################################################

[CmdletBinding()]
param(
    [Parameter(Mandatory=$True)]
    [string]$file
)

# Build Variables
if($Env:BuildOS) {
    $os = $Env:BuildOS
}else {
    $os = "win10"
}

if($Env:BuildPlatform) {
    $archs = $Env:BuildPlatform
}else {
    $archs = "x64"
}

# Add suffix to ffmpeg binary names
(gc FFmpegInterop\ffmpeg\configure) -replace '\.dll', '_ms.dll' | Out-File -encoding UTF8 FFmpegInterop\ffmpeg\configure
(gc FFmpegInterop\ffmpeg\configure) -replace '\.def', '_ms.def' | Out-File -encoding UTF8 FFmpegInterop\ffmpeg\configure

# Undo our change, if we've done it multiple times
(gc FFmpegInterop\ffmpeg\configure) -replace '_ms_ms\.dll', '_ms.dll' | Out-File -encoding UTF8 FFmpegInterop\ffmpeg\configure
(gc FFmpegInterop\ffmpeg\configure) -replace '_ms_ms\.def', '_ms.def' | Out-File -encoding UTF8 FFmpegInterop\ffmpeg\configure

# Build all platforms
foreach($arch in $archs){
    # Build FFmpeg
    cd FFmpegInterop
    $relativeFile = Join-Path .. $file
    cmd.exe /c "..\BuildFFmpeg.bat ${os} ${arch} -Settings ${relativeFile}"
    cd ..
}

# Run Preprocessing
& .\Write-FFmpegBuildPrestep.ps1 $file
