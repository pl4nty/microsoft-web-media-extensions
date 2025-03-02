//Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "pch.h"

extern "C"
{
#include <libavformat/avformat.h> // AVPacket
}

#include "FFmpegTypes.h"
#include "FFmpegContext.h" // FFmpegContext



namespace FFmpegPack {
	/** A class with all the MFTransform bloat. See FFmpegDecoderMFT for main implementation. */
	class DecoderBase :
		public IMFTransform,
		public IMFMediaEventGenerator,
		public IMFShutdown,
		public ABI::Windows::Media::IMediaExtension
	{
	public:
		DecoderBase();
		~DecoderBase();

		///////////////////////////////////////////////////////////
		//   IUnknown Interface
		///////////////////////////////////////////////////////////

		virtual STDMETHODIMP QueryInterface(__in REFIID riid, __out void **outInterface)
		{
			if (outInterface == NULL)
				return E_POINTER;

			if (riid == IID_IUnknown || riid == IID_IMFTransform)
				*outInterface = reinterpret_cast<IMFTransform*>(this);

			else if (riid == IID_IMFMediaEventGenerator)
				*outInterface = reinterpret_cast<IMFMediaEventGenerator*>(this);

			else if (riid == IID_IMFShutdown)
				*outInterface = reinterpret_cast<IMFShutdown*>(this);

			else if (riid == IID_IInspectable
				|| riid == __uuidof(ABI::Windows::Media::IMediaExtension))
				*outInterface = reinterpret_cast<ABI::Windows::Media::IMediaExtension*>(this);
			else
			{
				*outInterface = NULL;
				return E_NOINTERFACE;
			}

			AddRef();

			return S_OK;
		}

		virtual STDMETHODIMP_(ULONG) AddRef()
		{
			InternalAddRef();
			return InterlockedIncrement(&_ulRefCount);
		}

		virtual STDMETHODIMP_(ULONG) Release()
		{
			ULONG ulRefCount = InterlockedDecrement(&_ulRefCount);
			if (ulRefCount == 0 && _bShutdownStarted == FALSE)
				Shutdown();

			InternalRelease();
			return ulRefCount;
		}

		///////////////////////////////////////////////////////////
		//   IMFMediaEventGenerator Interface
		///////////////////////////////////////////////////////////
		virtual STDMETHODIMP BeginGetEvent(__in IMFAsyncCallback * pCallback, __in IUnknown * punkState);
		virtual STDMETHODIMP EndGetEvent(__in IMFAsyncResult * pResult, __deref_out IMFMediaEvent ** ppEvent);
		virtual STDMETHODIMP GetEvent(__in DWORD dwFlags, __deref_out IMFMediaEvent ** ppEvent);
		virtual STDMETHODIMP QueueEvent(__in MediaEventType meType, __in REFGUID guidExtendedType, __in HRESULT hrStatus, __in_opt const PROPVARIANT * pvarValue);

		///////////////////////////////////////////////////////////
		//   IMFShutdown Interface
		///////////////////////////////////////////////////////////
		virtual STDMETHODIMP GetShutdownStatus(__out MFSHUTDOWN_STATUS * pStatus);
		virtual STDMETHODIMP Shutdown();

		///////////////////////////////////////////////////////////
		//   IMFTransform Interface
		///////////////////////////////////////////////////////////
		virtual STDMETHODIMP AddInputStreams(__in DWORD cStreams, __in_ecount(cStreams) DWORD * rgdwStreamIDs);
		virtual STDMETHODIMP DeleteInputStream(__in DWORD dwStreamID);
		virtual STDMETHODIMP GetAttributes(__deref_out IMFAttributes ** ppAttributes);
		virtual STDMETHODIMP GetInputAvailableType(__in DWORD dwInputStreamID, __in DWORD dwTypeIndex, __deref_out IMFMediaType ** ppType);
		virtual STDMETHODIMP GetInputCurrentType(__in DWORD dwInputStreamID, __deref_out IMFMediaType ** ppType);
		virtual STDMETHODIMP GetInputStatus(__in DWORD dwInputStreamID, __out DWORD * pdwFlags);
		virtual STDMETHODIMP GetInputStreamAttributes(__in DWORD dwInputStreamID, __deref_out IMFAttributes ** ppAttributes);
		virtual STDMETHODIMP GetInputStreamInfo(__in DWORD dwInputStreamID, __out MFT_INPUT_STREAM_INFO * pStreamInfo);
		virtual STDMETHODIMP GetOutputAvailableType(__in DWORD dwOutputStreamID, __in DWORD dwTypeIndex, __deref_out IMFMediaType ** ppType);
		virtual STDMETHODIMP GetOutputCurrentType(__in DWORD dwOutputStreamID, __deref_out IMFMediaType ** ppType);
		virtual STDMETHODIMP GetOutputStatus(__out DWORD * pdwFlags);
		virtual STDMETHODIMP GetOutputStreamAttributes(__in DWORD dwOutputStreamID, __deref_out IMFAttributes ** ppAttributes);
		virtual STDMETHODIMP GetOutputStreamInfo(__in DWORD dwOutputStreamID, __out MFT_OUTPUT_STREAM_INFO * pStreamInfo);
		virtual STDMETHODIMP GetStreamCount(__out DWORD * pcInputStreams, __out DWORD * pcOutputStreams);
		virtual STDMETHODIMP GetStreamIDs(__in DWORD dwInputIDArraySize, __inout_ecount(dwInputIDArraySize) DWORD * pdwInputIDs, __in DWORD dwOutputIDArraySize, __inout_ecount(dwInputIDArraySize) DWORD * pdwOutputIDs);
		virtual STDMETHODIMP GetStreamLimits(__out DWORD * pdwInputMinimum, __out DWORD * pdwInputMaximum, __out DWORD * pdwOutputMinimum, __out DWORD * pdwOutputMaximum);
		virtual STDMETHODIMP ProcessEvent(__in DWORD dwInputStreamID, __in IMFMediaEvent * pEvent);
		virtual STDMETHODIMP ProcessInput(__in DWORD dwInputStreamID, __in IMFSample * pSample, __in DWORD dwFlags);
		virtual STDMETHODIMP ProcessMessage(__in MFT_MESSAGE_TYPE eMessage, __in ULONG_PTR ulParam);
		virtual STDMETHODIMP ProcessOutput(__in DWORD dwFlags, __in DWORD cOutputBufferCount, __inout_ecount(cOutputBufferCount) MFT_OUTPUT_DATA_BUFFER * pOutputSamples, __out DWORD *pdwStatus);
		virtual STDMETHODIMP SetInputType(__in DWORD dwInputStreamID, __in IMFMediaType * pType, __in DWORD dwFlags);
		virtual STDMETHODIMP SetOutputBounds(__in LONGLONG hnsLowerBound, __in LONGLONG hnsUpperBound);
		virtual STDMETHODIMP SetOutputType(__in DWORD dwOutputStreamID, __in IMFMediaType * pType, __in DWORD dwFlags);

		///////////////////////////////////////////////////////////
		//   IMediaExtension Interface
		///////////////////////////////////////////////////////////
		virtual STDMETHODIMP SetProperties(__RPC__in_opt ABI::Windows::Foundation::Collections::IPropertySet * configuration);

		///////////////////////////////////////////////////////////
		//   IInspectable Interface
		///////////////////////////////////////////////////////////
		virtual STDMETHODIMP GetIids(__RPC__out ULONG *iidCount, __RPC__deref_out_ecount_full_opt(*iidCount) IID **iids);
		virtual STDMETHODIMP GetRuntimeClassName(__RPC__deref_out_opt HSTRING *className);
		virtual STDMETHODIMP GetTrustLevel(__RPC__out TrustLevel *trustLevel);

	protected:
		///////////////////////////////////////////////////////////
		//   Private properties
		///////////////////////////////////////////////////////////
		// IUnknown support vars
		volatile ULONG _ulRefCount;
		volatile ULONG _ulIntRefCount;

		// IShutdown support vars
		BOOL _bShutdownStarted;
		BOOL _bShutdown;

		// IMFMediaEventGenerator support vars
		/** Queue of internal events. */
		IMFMediaEventQueue* _spEventQueue;

		// Other
		/** A simple concurrency lock. */
		CRITICAL_SECTION  _pcsLock;

		/** The current output type. Raw audio/video in this case. */
		CComPtr<IMFMediaType> _spOutputType;

		/** The current input type. Compressed audio/video in this case. */
		CComPtr<IMFMediaType> _spInputType;

		/** Specific values to return on GetOutputAvailableType */
		CComPtr<IMFMediaType> _spSuggestedOutputType;

		/** The current major type, either audio or video. */
		GUID m_guidMajorType;

		/** The current FFmpeg library decoding context. */
		FFmpegContext^ _pContext;

		/** Signals format change that requires rebuilding of the FFmpeg Context. */
		boolean _bFormatChange;



		///////////////////////////////////////////////////////////
		//   IUnknown support methods
		///////////////////////////////////////////////////////////
		ULONG InternalAddRef()
		{
			return InterlockedIncrement(&_ulIntRefCount);
		}

		ULONG InternalRelease()
		{
			ULONG ulIntRefCount = InterlockedDecrement(&_ulIntRefCount);
			if (ulIntRefCount == 0)
				delete this;

			return ulIntRefCount;
		}


		///////////////////////////////////////////////////////////
		//   IShutdown support methods
		///////////////////////////////////////////////////////////
		HRESULT _CheckShutdown();

		///////////////////////////////////////////////////////////
		//   Non-interface dynamic encoding settings
		//   - used to be IDynamicEncoderSettingsChangeSubscriber
		//   - not a real interface in this project.
		///////////////////////////////////////////////////////////
		
		//STDMETHODIMP GetIsEncoding(__out VARIANT_BOOL * pvtbEncoding);
		STDMETHODIMP IsInputTypeSet(__out VARIANT_BOOL *pvtbInputSet);
		STDMETHODIMP IsOutputTypeSet(__out VARIANT_BOOL *pvtbOutputSet);


		///////////////////////////////////////////////////////////
		//   Other Methods
		///////////////////////////////////////////////////////////
		HRESULT ValidateInputType(IMFMediaType *pType);
		HRESULT ValidateOutputType(IMFMediaType *pType);

		HRESULT _SetOutputTypeAudioProperties(IMFMediaType * pType);
		HRESULT _SetOutputTypeVideoProperties(IMFMediaType * pType);
	};
}
