# Media Extensions

This is a set of Universal Windows Platform apps that export decoders as Media Extensions. This package is powered by the open source project FFmpeg. This project aims to promote customer choice and allow consumers to use any codec with their preferred player app.


## Dependencies

### FFmpegInterop

The FFmpegInterop project, currently supported by Gilles Khouzam <gilles.khouzam@microsoft.com>, is a public Microsoft-driven effort to port the FFmpeg libraries into the MSS platform. The repository of this project can be found [here](https://github.com/Microsoft/FFmpegInterop).

This project wraps around the FFmpeg libraries, providing decoding from containers to raw audio/video (and optionally compressed media as well). It is important to note that this will be the source of the FFmpeg project itself, as the repository includes a submodule pointing to the official source.


### FFmpeg

You can find instructions on how to [build](https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT), and [develop](https://www.ffmpeg.org/doxygen/trunk/index.html) using FFmpeg. There are scripts that automatically build the FFmpeg source based on some provided settings, which will be discussed in detail on the Build section.


## Project Organization

These are the main folders of the project:

* `AppServiceCPP/` a Visual Studio project that provides the Media Extension service for FFmpegInterop.
* `BuildUtils/` a Powershell script and other required files to install and self-host a codec pack.
* `FFmpegInterop/` a git submodule pointing to the public FFmpegInterop code.
* `FFmpegCodecPack/` the main Visual Studio project, which provides the appxmanifest and bundles up the FFmpegInterop source and the FFmpeg MFT.
* `FFmpegMFT/` a custom MFT wrapper for FFmpeg, which allows for decoding in the pipeline.

## Builds

Significant work has been done to make building codec packs easy. The general building process involves building FFmpeg binaries, outputting a custom appxmanifest, and then building a Visual Studio solution that includes FFmpeg.


### Config XMLs
As part of the build system, there are two types of XML files that define several settings of the codec pack to be created.

**Metadata Definitions:** this is a XML file that contains all the necessary metadata of video & audio codecs, video & audio output types, and containers. It defines things like the GUID of each type, the macro identifications and the FFmpeg codec IDs, so the pack can declare them correclty in its appxmanifest and pass them correclty to the underlying FFmpeg decoders. Under most circumstances, only one file of this type would be present in the project. An example of a *Metadata* file in the project is `FFmpegCodecs.xml`.

**Build Settings:** this is a simple XML file that contains a list of codecs, output types and containers to be included in a specific build. This file contains a pointer to a *Metadata* file in the `definitions` attribute, which must define all of the items listed. Different codec packs would use different *Build Settings* files. An example of a file of this type in the project is `PlatformPack.xml`.


### Appxmanifest
The manifest needs to be generated automatically by the build system, given that it is different for each codec pack. Thus, we have a template located at `./FFmpegMFT/Package.template.xml`. In this template, the following are replaced by automatically generated metadata:

* `<!--%AudioInputTypes%-->` is replaced by the corresponding audio `<InputType>`s and `<OutputType>`s.
* `<!--%VideoInputTypes%-->` is replaced by the corresponding video `<InputType>`s and `<OutputType>`s.
* `<!--%SupportedFileTypes%-->` is replaced by a list of `<uap5:FileType>`s per the Build Settings.
* `<!--%SupportedContentTypes%-->` is replaced by a list of `<uap5:ContentType>`s per the Build Settings.

Other than those, the template file contents are left as-is.


### Scripts

The following are utility scripts that automate different steps of the building process.

* `Out-FFmpegConfig.ps1` aggregates other steps, performing a full build of FFmpeg binaries.
* `BuildFFmpeg.bat` builds the FFmpeg binaries given a platform, architecture and a settings file.
* `FFmpegConfig.sh` a shell file that configures and makes FFmpeg in a Unix-like environment (in our case Msys2).
* `Get-FFmpegBuildSettings.ps1` reads a config XML and outputs a string with build flags for the configure step of building FFmpeg.
* `Write-FFmpegBuildPrestep.ps1` reads a config XML and outputs the appxmanifest and header files required to build a codec pack with those settings using Visual Studio.

Here is a quick way to perform all the build steps necessary *prior* to building the `FFmpegWin10.sln` with Visual Studio 2015:

```
powershell -ExecutionPolicy Bypass .\Out-FFmpegConfig.ps1 .\BuildConfig.xml
```

You can additionally set up the environment variables `BuildOS` and `BuildPlatform` to specify the Windows target version and the architecture target, respectively.


### TFS Build

A Visual Studio Teams build definition was created to build non-production codec packs, and can be found [here](https://microsoft.visualstudio.com/Apps/_build/index?context=mine&path=%5C&definitionId=16059). It uses a custom agent in PackageES called `PKGESCODEPACK04` and is able to build the FFmpeg binaries and subsequently a multi-architecture UWP package bundle signed with a certificate in the repo.

A *Build Settings* file can be specified for each build, so the same build definition can be used for multiple packs.


### Outputs
The following are the outputs of the build steps:

* `./FFmpegMFT/FFmpegBuildCodecs.g.h` a header file used by the MFT, containing libav and Media Foundation constants that specify the allowed inputs and outputs.
* `./FFmpegCodecPack/Package.appxmanifest` a generated appx manifest that defines the accepted MIME types and file extensions for the FFmpegInterop Media Source, and the accepted input and output media types for the MFT.
* `./FFmpegInterop/ffmpeg/Build` the FFmpeg built DLLs, header files, and libraries.
* `./FFmpegInterop/ffmpeg/Output` the full FFmpeg build output, including PDB symbol files and object files.

Additional files, such as a `*.appxbundle` file are also generated by Visual Studio when the `FFmpegWin10.sln` solution is built (which depends on all the outputs just described).

## Self Hosting

The Visual Studio Teams build we created outputs codec packs to [\\apollon\public\davidmeb\FFMPEG\Latest](file://apollon\public\davidmeb\FFMPEG\Latest). In this folder, there will be an `.appxbundle`, along a certificate and the script `Add-AppDevPackage.ps1`, which should be executed in order to install the pack. The codec package can easily be uninstalled using the Apps & features page in Settings.


## Media Foundation Transform

The MFT is the main wrapper implemented in this project. There are a few key Classes that provide all the functionality needed by the pipeline to make use of FFmpeg. Here are the most important ones:

### DecoderBase

This class provides all the MFT state management code, which handles things like input and output type changes, startup and shutdown, state changes, and stream properties. It contains many essential functionality used by the topology resolver and MFT proxy.


### FFmpegDecoderMFT

This class extends from `DecoderBase`, and implements only the stream lifecycle and decoding methods. This is where the methods that accept input and provide output are implemented. It was separated from the rest of the management code for organization.


### FFmpegContext

This class interfaces between FFmpeg and the MFT. It internally does most of the decoding, converts media types between libav formats and Media Foundation formats, and handles all the state-dependent FFmpeg contexts.

### Transform Helpers

These are video or audio-specific, and provide resampling and massaging of uncompressed output frames. They essentially make sure that if the output of the FFmpeg decoder is not in the desired format, it gets converted correctly into the expected output format for the pipeline.
