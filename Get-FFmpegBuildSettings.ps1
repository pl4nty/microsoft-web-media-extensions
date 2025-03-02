#!/powershell
###################################################
# Copyright (C) Microsoft. All rights reserved.
# 
# Gets FFmpeg build settings.
###################################################

[CmdletBinding()]
param(
    [Parameter(Mandatory=$True)]
    [string]$file
)

$settings = [xml](Get-Content $file)
$settingsDir = Split-Path $file -Parent
$defPath = Join-Path $settingsDir $settings.FFmpegCodecPack.definitions

$definitions = [xml](Get-Content $defPath)

$buildString = "--disable-everything"

function Get-Codec([string]$codecName){
    echo $codecName
    foreach($codec in $definitions.codecs.ChildNodes){

        if($codec.name -eq $codecName){
            return $codec
        }
    }
    0
}

foreach($child in $settings.FFmpegCodecPack.ChildNodes){
    if($child.LocalName -ne "codec" -and $child.LocalName -ne "container"){
        continue
    }
    $codec = Get-Codec $child.InnerText
    $codecBuild = $codec.build
    $buildString = "$buildString $codecBuild"
}

Write-Output $buildString


