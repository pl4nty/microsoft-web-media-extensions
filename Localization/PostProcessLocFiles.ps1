param(
# The localization config file.
[Parameter(Mandatory = $true)]
[ValidateScript({ Test-Path $_ })]
[string] $locConfigFile,

# The location where localized resource files downloaded from Touchdown Build is available.
[Parameter(Mandatory = $true)]
[ValidateScript({ Test-Path $_ })]
[string] $locOutput,

# The module name in the localization config file.
[Parameter(Mandatory = $true)]
[string] $locModuleName,

# The file which will contain a list of localized resource files processed by the script for further consumption later.
[Parameter(Mandatory = $true)]
[string] $locFileList,

# The localization type.
[Parameter(Mandatory = $true)]
[ValidateSet("LocalizeResx", "LocalizeApp")]
[string] $locType,

# Indicates whether the "bin" subdirectory exists or not.
[switch] $noBinSubDir
)

trap
{
    $error[0] | Out-Host
    exit 1
}

Set-StrictMode -Version 3
$ErrorActionPreference = "Stop"

Write-Host "Post-processing localized resource files under '$locOutput' by parsing '$locConfigFile'..."

if(Test-Path $locFileList)
{
    Remove-Item $locFileList
}

$locConfigXml = [xml] $(Get-Content $locConfigFile)
$modules = $locConfigXml.Modules.Module

foreach($module in $modules)
{
    if($module.name -ne $locModuleName)
    {
        continue
    }

    $files = $module.File

    foreach($file in $files)
    {
        $location = $file.location
        $fullPath = [Environment]::ExpandEnvironmentVariables($file.path)
        $baseTgtFileDir = Split-Path $fullPath -Parent
        $baseTgtFileDir = Convert-Path $baseTgtFileDir
        $fileName = Split-Path $fullPath -Leaf
        $fileNameNoExt = [System.IO.Path]::GetFileNameWithoutExtension($fileName)
        $ext = [System.IO.Path]::GetExtension($fileName)

        if($locType -eq "LocalizeApp")
        {
            if(($ext -ne ".resw") -and ($ext -ne ".resjson"))
            {
                continue
            }
        }
        else
        {
            if($ext -ne ".resx")
            {
                continue
            }
        }

        $cultures = Get-ChildItem $locOutput -Exclude @("en", "en-us") | Where-Object { $_.PSIsContainer -eq $true } | ForEach-Object { $_.Name }

        foreach($culture in $cultures)
        {
            if($noBinSubDir.IsPresent)
            {
                $baseSrcFileDir = [System.IO.Path]::Combine($locOutput, $culture, $location)
            }
            else
            {
                $baseSrcFileDir = [System.IO.Path]::Combine($locOutput, $culture, "bin", $location)
            }

            if(-not (Test-Path $baseSrcFileDir))
            {
               continue
            }

            # Culture code can be appended to RESX filename like Resource1.ko-kr.resx
            $srcFilePaths = @(Get-ChildItem "$baseSrcFileDir\*" -Include "$fileName", "$fileNameNoExt.*$ext")

            if($srcFilePaths.Count -eq 0)
            {
                continue
            }

            $srcFilePath = $srcFilePaths[0]
            $locFileName = Split-Path $srcFilePath -Leaf

            if($locType -eq "LocalizeApp")
            {
                $tgtFileDir = Split-Path $baseTgtFileDir -Parent
                $tgtFileDir = [System.IO.Path]::Combine($tgtFileDir, $culture)
            }
            else
            {
                $tgtFileDir = $baseTgtFileDir
            }

            $tgtFilePath = [System.IO.Path]::Combine($tgtFileDir, $locFileName)

            if(-not (Test-Path $tgtFileDir))
            {
               [void] [System.IO.Directory]::CreateDirectory($tgtFileDir)
            }

            [System.IO.File]::Copy($srcFilePath, $tgtFilePath, $true)
            Write-Host "'$srcFilePath' ->`n  '$tgtFilePath'."

            # Accumulate a list of localized resource files for consumption later
            Add-Content $locFileList -Value $tgtFilePath
        }
    }
}

exit 0
