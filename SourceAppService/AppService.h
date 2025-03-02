//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

using namespace Windows::ApplicationModel::AppService;
using namespace Windows::ApplicationModel::Background;
using namespace Windows::Foundation::Collections;

namespace SourceAppService
{
	public ref class AppService sealed : public Windows::ApplicationModel::Background::IBackgroundTask
	{
	public:
		AppService();

		// Inherited via IBackgroundTask
		virtual void Run(Windows::ApplicationModel::Background::IBackgroundTaskInstance ^taskInstance);

	private:

		BackgroundTaskDeferral^ backgroundTaskDeferral;
		AppServiceConnection^ appServiceConnection;

		FFmpegInterop::FFmpegInteropMSS^ FFmpegMSS;

		bool fCanceled = false;

		void InitializeMediaStreamSourceRequestedEventHandler(
			Windows::Media::Core::MediaSourceAppServiceConnection^ sender,
			Windows::Media::Core::InitializeMediaStreamSourceRequestedEventArgs^ args);

		void TaskCanceledEventHandler(
			Windows::ApplicationModel::Background::IBackgroundTaskInstance^ sender,
			Windows::ApplicationModel::Background::BackgroundTaskCancellationReason resons);
	};
}

