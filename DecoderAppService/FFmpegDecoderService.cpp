//Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "FFmpegDecoderMFT.h"

#include "FFmpegDecoderService.h"


using namespace DecoderAppService; // AppService
using namespace FFmpegPack; // FFmpegDecoderMFT

using namespace Windows::Media; // IMediaExtension, MediaExtensionManager
using namespace Windows::Foundation::Collections; // ValueSet

AppService::AppService()
{

}

AppService::~AppService()
{
	if (m_pImfTransform)
		m_pImfTransform->Release();
}

void AppService::Run(IBackgroundTaskInstance ^ taskInstance)
{
	auto details = (AppServiceTriggerDetails^)taskInstance->TriggerDetails;

	if (details != nullptr)
	{
		// Get a deferral so that the service isn't terminated.
		m_backgroundTaskDeferral = taskInstance->GetDeferral();

		taskInstance->Canceled += ref new BackgroundTaskCanceledEventHandler(
			this,
			&AppService::OnTaskCanceled
		);

		m_appServiceConnection = details->AppServiceConnection;

		m_pImfTransform = nullptr;
		FFmpegDecoderMFT::CreateInstance(&m_pImfTransform);

		// Export MFTransform as MediaExtension. Pipeline knows how to use it,
		// and will recast it into an IMFTransform as needed.
		IMediaExtension^ ext = reinterpret_cast<IMediaExtension^>(m_pImfTransform);
		MediaExtensionManager^ manager = ref new MediaExtensionManager();
		manager->RegisterMediaExtensionForAppService(ext, m_appServiceConnection);
	}
}

void AppService::OnRequestReceived(
	AppServiceConnection ^ sender,
	AppServiceRequestReceivedEventArgs ^ args
) {
	// Doesn't really do anything. Needed for IBackgroundTask Interface.
	AppServiceDeferral^ messageDeferral = args->GetDeferral();

	ValueSet^ message = args->Request->Message;
	ValueSet^ returnData = ref new ValueSet();

	try
	{
		args->Request->SendResponseAsync(returnData);
		messageDeferral->Complete();
	}
	catch (Platform::Exception^ e)
	{
		throw (e);
	}
}

void AppService::OnTaskCanceled(
	IBackgroundTaskInstance ^ sender,
	BackgroundTaskCancellationReason reason
) {
	if (m_backgroundTaskDeferral != nullptr)
	{
		m_backgroundTaskDeferral->Complete();
	}
}
