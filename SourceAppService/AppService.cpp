//Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "AppService.h"

using namespace SourceAppService;
using namespace FFmpegInterop;

AppService::AppService()
{
}

void AppService::Run(Windows::ApplicationModel::Background::IBackgroundTaskInstance^ taskInstance)
{
	AppServiceTriggerDetails^ details = (AppServiceTriggerDetails^)taskInstance->TriggerDetails;

	if (details != nullptr)
	{
		this->backgroundTaskDeferral = taskInstance->GetDeferral();
		taskInstance->Canceled += ref new BackgroundTaskCanceledEventHandler(this, &AppService::TaskCanceledEventHandler);

		this->appServiceConnection = details->AppServiceConnection;

		Windows::Media::Core::MediaSourceAppServiceConnection^ mediaSourceConnection = ref new Windows::Media::Core::MediaSourceAppServiceConnection(appServiceConnection);
		mediaSourceConnection->InitializeMediaStreamSourceRequested +=
			ref new Windows::Foundation::TypedEventHandler<Windows::Media::Core::MediaSourceAppServiceConnection^, Windows::Media::Core::InitializeMediaStreamSourceRequestedEventArgs^>(
				this, &AppService::InitializeMediaStreamSourceRequestedEventHandler);
		mediaSourceConnection->Start();
	}
}


void AppService::InitializeMediaStreamSourceRequestedEventHandler(Windows::Media::Core::MediaSourceAppServiceConnection^ sender, Windows::Media::Core::InitializeMediaStreamSourceRequestedEventArgs^ args)
{
	bool fForceDecodeAudio = false;
	bool fForceDecodeVideo = false;
	
	try
	{
		FFmpegMSS = FFmpegInteropMSS::CreateFFmpegInteropMSSFromStream(args->RandomAccessStream, fForceDecodeAudio, fForceDecodeVideo, nullptr, args->Source);
	}
	catch (Platform::Exception^ ex)
	{
		// We failed during initialization, likely because the MSS we were provided no longer exists.
		// We will terminate app service.
		if (backgroundTaskDeferral != nullptr)
		{
			this->backgroundTaskDeferral->Complete();
			backgroundTaskDeferral = nullptr;
		}
	}
}

void AppService::TaskCanceledEventHandler(Windows::ApplicationModel::Background::IBackgroundTaskInstance ^ sender, Windows::ApplicationModel::Background::BackgroundTaskCancellationReason reason)
{
	if (backgroundTaskDeferral != nullptr)
	{
		this->backgroundTaskDeferral->Complete();
		backgroundTaskDeferral = nullptr;
	}
}
