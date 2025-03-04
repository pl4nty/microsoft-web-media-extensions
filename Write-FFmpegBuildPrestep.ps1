#!/usr/bin/env powershell
###################################################
# Copyright (C) Microsoft. All rights reserved.
# 
# Writes various build files given a settings file.
###################################################

[CmdletBinding()]
param(
    [Parameter(Mandatory=$True)]
    [string]$file
)

$n = [System.Environment]::NewLine

# Constants
$BUILD_HEADER_FILE="./FFmpegMFT/FFmpegBuildCodecs.g.h"
$MANIFEST_TEMPLATE_FILE="./FFmpegCodecPack/Package.template.xml"
$MANIFEST_RENDERED_FILE="./FFmpegCodecPack/Package.appxmanifest"
$GENERATED_WARNING="<!-- CONTENT GENERATED BY PREBUILD, SEE BuildConfig.xml and Package.template.xml -->${n}"

$XML_AUDIO_WRAPPER="<uap4:Extension Category=`"windows.mediaCodec`">${n}<uap4:MediaCodec DisplayName=`"FFmpegCodec`" Description=`"FFmpeg Audio MFT`" Category=`"audioDecoder`" AppServiceName=`"com.microsoft.codecs.audio`">${n}<uap4:MediaEncodingProperties>${n}"
$XML_VIDEO_WRAPPER="<uap4:Extension Category=`"windows.mediaCodec`">${n}<uap4:MediaCodec DisplayName=`"FFmpegCodec`" Description=`"FFmpeg Video MFT`" Category=`"videoDecoder`" AppServiceName=`"com.microsoft.codecs.video`">${n}<uap4:MediaEncodingProperties>${n}"
$XML_AUDIO_END="</uap4:MediaEncodingProperties>${n}</uap4:MediaCodec>${n}</uap4:Extension>${n}"
$XML_VIDEO_END="</uap4:MediaEncodingProperties>${n}</uap4:MediaCodec>${n}</uap4:Extension>${n}"



# Local Vars
$settings = [xml](Get-Content $file)
$definitions = [xml](Get-Content $settings.FFmpegCodecPack.definitions)
[System.Xml.XmlNode[]] $audioCodecs = @()
[System.Xml.XmlNode[]] $audioOutputs = @()
[System.Xml.XmlNode[]] $videoCodecs = @()
[System.Xml.XmlNode[]] $videoOutputs = @()

[System.Xml.XmlNode[]] $containers = @()

# Global Vars
$global:audioCount = 0
$global:videoCount = 0

$global:headerDefines = "// Codecs in this pack:${n}"

function Get-Codec([string]$codecName) {

    foreach($codec in $definitions.codecs.ChildNodes){

        if($codec.name -eq $codecName){
            return [System.Xml.XmlNode] $codec
        }
    }
    return 0
}

function Out-BuildHeaderFile() {
    # Make sure we overwrite always
    #Remove-Item $BUILD_HEADER_FILE

    # First is single > to create file
    "// Generated by pre build step. See BuildConfig.xml.${n}" > $BUILD_HEADER_FILE
    
    "#pragma once${n}" >> $BUILD_HEADER_FILE

    "extern `"C`"" >> $BUILD_HEADER_FILE
    "{" >> $BUILD_HEADER_FILE
    "#include <libavformat/avformat.h> // AVPacket" >> $BUILD_HEADER_FILE
    "}${n}" >> $BUILD_HEADER_FILE

    "namespace FFmpegPack {${n}" >> $BUILD_HEADER_FILE
    $headerDefines >> $BUILD_HEADER_FILE

    # INPUT AUDIO
    "const AVCodecID FFMPEG_AVTYPES_INPUT_AUDIO[] = {" >> $BUILD_HEADER_FILE
    if($audioCount -ne 0){
        foreach($audioCodec in $audioCodecs){
            "`t$($audioCodec.AVCodecID)," >> $BUILD_HEADER_FILE
        }
    }else{
        "`t// No Audio Codecs" >> $BUILD_HEADER_FILE
        "`tAV_CODEC_ID_NONE" >> $BUILD_HEADER_FILE
    }
    "};${n}" >> $BUILD_HEADER_FILE

    "const GUID FFMPEG_MFTYPES_INPUT_AUDIO[] = {" >> $BUILD_HEADER_FILE
    if($audioCount -ne 0){
        foreach($audioCodec in $audioCodecs){
            "`t$($audioCodec.MFType)," >> $BUILD_HEADER_FILE
        }
    }else{
        "`t// No Audio Codecs" >> $BUILD_HEADER_FILE
        "`t0" >> $BUILD_HEADER_FILE
    }
    "};${n}" >> $BUILD_HEADER_FILE

    # INPUT VIDEO
    "const AVCodecID FFMPEG_AVTYPES_INPUT_VIDEO[] = {" >> $BUILD_HEADER_FILE
    if($videoCount -ne 0){
        foreach($videoCodec in $videoCodecs){
            "`t$($videoCodec.AVCodecID)," >> $BUILD_HEADER_FILE
        }
    }else{
        "`t// No Video Codecs" >> $BUILD_HEADER_FILE
        "`tAV_CODEC_ID_NONE" >> $BUILD_HEADER_FILE
    }
    "};${n}" >> $BUILD_HEADER_FILE

    "const GUID FFMPEG_MFTYPES_INPUT_VIDEO[] = {" >> $BUILD_HEADER_FILE
    if($videoCount -ne 0){
        foreach($videoCodec in $videoCodecs){
            "`t$($videoCodec.MFType)," >> $BUILD_HEADER_FILE
        }
    }else{
        "`t// No Video Codecs" >> $BUILD_HEADER_FILE
        "`t0" >> $BUILD_HEADER_FILE
    }
    "};${n}" >> $BUILD_HEADER_FILE

    # OUTPUT AUDIO
    "const AVSampleFormat FFMPEG_AVTYPES_OUTPUT_AUDIO[] = {" >> $BUILD_HEADER_FILE
    if($audioCount -ne 0){
        foreach($audioOut in $audioOutputs){
            "`t$($audioOut.AVFormat)," >> $BUILD_HEADER_FILE
        }
    }else{
        "`t// No Audio Codecs" >> $BUILD_HEADER_FILE
        "`tAV_SAMPLE_FMT_NONE" >> $BUILD_HEADER_FILE
    }
    "};${n}" >> $BUILD_HEADER_FILE

    "const GUID FFMPEG_MFTYPES_OUTPUT_AUDIO[] = {" >> $BUILD_HEADER_FILE
    if($audioCount -ne 0){
        foreach($audioOut in $audioOutputs){
            "`t$($audioOut.MFType)," >> $BUILD_HEADER_FILE
        }
    }else{
        "`t// No Audio Codecs" >> $BUILD_HEADER_FILE
        "`t0" >> $BUILD_HEADER_FILE
    }
    "};${n}" >> $BUILD_HEADER_FILE

    # OUTPUT VIDEO
    "const AVPixelFormat FFMPEG_AVTYPES_OUTPUT_VIDEO[] = {" >> $BUILD_HEADER_FILE
    if($videoCount -ne 0){
        foreach($videoOutput in $videoOutputs){
            "`t$($videoOutput.AVFormat)," >> $BUILD_HEADER_FILE
        }
    }else{
        "`t// No Video Codecs" >> $BUILD_HEADER_FILE
        "`tAV_PIX_FMT_NONE" >> $BUILD_HEADER_FILE
    }
    "};${n}" >> $BUILD_HEADER_FILE

    "const GUID FFMPEG_MFTYPES_OUTPUT_VIDEO[] = {" >> $BUILD_HEADER_FILE
    if($videoCount -ne 0){
        foreach($videoOutput in $videoOutputs){
            "`t$($videoOutput.MFType)," >> $BUILD_HEADER_FILE
        }
    }else{
        "`t// No Video Codecs" >> $BUILD_HEADER_FILE
        "`t0" >> $BUILD_HEADER_FILE
    }
    "};${n}" >> $BUILD_HEADER_FILE

    "} // namespace FFmpeg${n}"  >> $BUILD_HEADER_FILE
}

function Write-CodecInHeader([Object]$codec) {
    $global:headerDefines = "${headerDefines}#define FFMPEG_CODEC_$($codec.name)${n}"

    if($codec.type -eq "Audio"){
        $global:audioCount = $audioCount + 1
    }else{
        $global:videoCount = $videoCount + 1
    }
}

function Write-ContainerInHeader([Object]$codec) {
    $global:headerDefines = "${headerDefines}#define FFMPEG_CONTAINER_$($codec.name)${n}"
}

# AudioInputTypes
# <!-- Name -->\n<uap4:InputType SubType="${guid}" />
function Get-AudioInputTypes() {
    if($audioCount -eq 0){
        return ""
    }

    $result = $GENERATED_WARNING
    $result = "${result}${XML_AUDIO_WRAPPER}"
    $result = "${result}<uap4:InputTypes>${n}"
    
    foreach($codec in $audioCodecs) {
        $result = "${result}<!-- $($codec.name) -->${n}<uap4:InputType SubType=`"$($codec.guid)`" />${n}"
    }

    $result = "${result}</uap4:InputTypes>${n}"
    $result = "${result}<uap4:OutputTypes>${n}"

    foreach($rawtype in $audioOutputs){
        $result = "${result}<!-- Uncompressed $($rawtype.name) -->${n}<uap4:OutputType SubType=`"$($rawtype.guid)`" />${n}"
    }

    $result = "${result}</uap4:OutputTypes>${n}"
    $result = "${result}${XML_AUDIO_END}"

    return $result
}

# VideoInputTypes
# <!-- Name -->\n<uap4:InputType SubType="${guid}" />
function Get-VideoInputTypes(){
    if($videoCount -eq 0){
        return ""
    }

    $result = $GENERATED_WARNING
    $result = "${result}${XML_VIDEO_WRAPPER}"
    $result = "${result}<uap4:InputTypes>${n}"
    
    foreach($codec in $videoCodecs) {
        if($codec.MkvCodecId){
            $result = "${result}<!-- $($codec.name) -->${n}<uap4:InputType SubType=`"$($codec.guid)`" uap5:MkvCodecId=`"$($codec.MkvCodecId)`" />${n}"
        }else{
            $result = "${result}<!-- $($codec.name) -->${n}<uap4:InputType SubType=`"$($codec.guid)`" />${n}"
        }
    }

    $result = "${result}</uap4:InputTypes>${n}"
    $result = "${result}<uap4:OutputTypes>${n}"

    foreach($rawtype in $videoOutputs){
        $result = "${result}<!-- Uncompressed $($rawtype.name) -->${n}<uap4:OutputType SubType=`"$($rawtype.guid)`" />${n}"
    }

    $result = "${result}</uap4:OutputTypes>${n}"
    $result = "${result}${XML_VIDEO_END}"

    return $result
}

# SupportedFileTypes
# <uap5:FileType>${type}</uap5:FileType>
function Get-SupportedFileTypes(){
    $result = $GENERATED_WARNING

    foreach($container in $containers) {
        foreach($fileType in $container.extension){
            $result = "${result}<uap5:FileType>${fileType}</uap5:FileType>${n}"
        }
    }
    return $result
}

# SupportedContentTypes
# <uap5:ContentType SubType="${type}" />
function Get-SupportedContentTypes(){
    $result = $GENERATED_WARNING

    foreach($container in $containers) {
        foreach($contentType in $container.mime) {
            $result = "${result}<uap5:ContentType SubType=`"${contentType}`" />${n}"
        }
    }
    return $result
}

function Out-RenderedManifest() {
    $template = [System.IO.File]::ReadAllText($MANIFEST_TEMPLATE_FILE)

    $audioInputTypes    = Get-AudioInputTypes
    $videoInputTypes    = Get-VideoInputTypes
    $supportedFileTypes = Get-SupportedFileTypes
    $supportedContTypes = Get-SupportedContentTypes

    $template = $template.Replace("<!--%AudioInputTypes%-->",       $audioInputTypes);
    $template = $template.Replace("<!--%VideoInputTypes%-->",       $videoInputTypes);
    $template = $template.Replace("<!--%SupportedFileTypes%-->",    $supportedFileTypes);
    $template = $template.Replace("<!--%SupportedContentTypes%-->", $supportedContTypes);

    [System.IO.File]::WriteAllText($MANIFEST_RENDERED_FILE, $template)
}




foreach($child in $settings.FFmpegCodecPack.ChildNodes){
    if($child.LocalName -ne "codec" -and $child.LocalName -ne "rawtype" -and $child.LocalName -ne "container"){
        continue
    }

    [System.Xml.XmlNode] $codec = Get-Codec $child.InnerText

    if($child.LocalName -eq "codec"){
        if($child.sourceOnly){
            continue
        }

        Write-CodecInHeader $codec

        if($codec.type -eq "Audio"){
            echo "Building Audio Codec '$($codec.name)'"
            $audioCodecs += $codec
        }else{
            echo "Building Video Codec '$($codec.name)'"
            $videoCodecs += $codec
        }
                
    }elseif($child.LocalName -eq "rawtype"){

        if($codec.type -eq "Audio"){
            echo "Building Audio Output '$($codec.name)'"
            $audioOutputs += $codec
        }else{
            echo "Building Video Output '$($codec.name)'"
            $videoOutputs += $codec
        }
        
    }else{
        echo "Building Container '$($codec.name)'"
        Write-ContainerInHeader $codec
        $containers +=$codec
    }
}


Out-BuildHeaderFile
Out-RenderedManifest
echo "FFmpeg Prebuild Step Succeeded"
