#pragma once
#include "FFmpegTransformHelper.h"

extern "C"
{
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace FFmpegPack{
	ref class VideoTransformHelper :
		FFmpegTransformHelper
	{
		
	public:
		virtual ~VideoTransformHelper();

	internal:
		VideoTransformHelper();

		///////////////////////////////////////////////////////////
		//   FFmpegTransformHelper Methods
		///////////////////////////////////////////////////////////
		HRESULT Initialize(_In_ AVCodecContext *pCodecContext, _In_ IMFMediaType *inputType, _In_ IMFMediaType *outputType) override;
		HRESULT ProcessDecodedFrame(_In_ AVFrame *pFrame, _Out_ Platform::Array<BYTE>^& phBuffer) override;
	
	private:
		SwsContext* m_pScaleContext;

		int m_VideoBufferLineSizes[4];
		uint8_t* m_VideoBufferData[4];

		AVCodecContext *m_pCodecContext;

		/** The output uncompressed sample format. */
		AVPixelFormat m_outputFormat;

		/** The input (from decoder) uncompressed sample format. */
		AVPixelFormat m_inputFormat;


		//// Methods
		HRESULT _CreateScaleContext(void);
	};
};
