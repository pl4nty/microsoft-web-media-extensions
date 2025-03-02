//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

using namespace Windows::ApplicationModel::AppService; // AppServiceRequestReceivedEventArgs,
													   // AppServiceConnection,
using namespace Windows::ApplicationModel::Background; // IBackgroundTask

namespace DecoderAppService
{
	public ref class AppService sealed : public IBackgroundTask
	{
	public:
		AppService();
		virtual ~AppService();
		void virtual Run(IBackgroundTaskInstance^ taskInstance);

	private:
		void OnRequestReceived(AppServiceConnection^ sender, AppServiceRequestReceivedEventArgs^ args);
		void OnTaskCanceled(IBackgroundTaskInstance^ sender, BackgroundTaskCancellationReason reason);


		/** The deferral for async operation handling.  */
		BackgroundTaskDeferral^ m_backgroundTaskDeferral;

		/** The app endpoint exporting a MFTransform. */
		AppServiceConnection^ m_appServiceConnection;

		/** The FFmpeg MFT. */
		IMFTransform *m_pImfTransform;
	};
}
