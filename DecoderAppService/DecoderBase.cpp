//Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "DecoderBase.h"

#ifdef DEBUG
#include "DebugUtils.h"
#endif

#include <winstring.h> // WindowsCreateString

using namespace FFmpegPack;

DecoderBase::DecoderBase()
{
	InitializeCriticalSection(&_pcsLock);
	MFCreateEventQueue(&_spEventQueue);

	_pContext = ref new FFmpegContext();
	_bFormatChange = false;

	AddRef();
}

DecoderBase::~DecoderBase()
{
	Shutdown();
	DeleteCriticalSection(&_pcsLock);
}


///////////////////////////////////////////////////////////
//   IMFMediaEventGenerator Interface
///////////////////////////////////////////////////////////

STDMETHODIMP DecoderBase::BeginGetEvent(
	__in IMFAsyncCallback * pCallback, 
	__in IUnknown * punkState
) {
	EnterCriticalSection(&_pcsLock);

	HRESULT hr = _CheckShutdown();
	if (SUCCEEDED(hr))
		hr = _spEventQueue->BeginGetEvent(pCallback, punkState);

	LeaveCriticalSection(&_pcsLock);
	return hr;
}

STDMETHODIMP DecoderBase::EndGetEvent(
	__in IMFAsyncResult * pResult,
	__deref_out IMFMediaEvent ** ppEvent
) {
	EnterCriticalSection(&_pcsLock);

	HRESULT hr = _CheckShutdown();
	if (SUCCEEDED(hr))
		hr = _spEventQueue->EndGetEvent(pResult, ppEvent);

	LeaveCriticalSection(&_pcsLock);
	return hr;
}

STDMETHODIMP DecoderBase::GetEvent(
	__in DWORD dwFlags,
	__deref_out IMFMediaEvent ** ppEvent
) {
	HRESULT hr = (ppEvent) ? S_OK : E_POINTER;
	CComPtr<IMFMediaEventQueue> spQueue = NULL;

	EnterCriticalSection(&_pcsLock);
	if (SUCCEEDED(hr))
		hr = _CheckShutdown();

	if (SUCCEEDED(hr))
		hr = _spEventQueue->GetEvent(dwFlags, ppEvent);

	LeaveCriticalSection(&_pcsLock);

	return hr;
}

STDMETHODIMP DecoderBase::QueueEvent(
	__in MediaEventType meType, 
	__in REFGUID guidExtendedType, 
	__in HRESULT hrStatus, 
	__in_opt const PROPVARIANT * pvarValue
) {
	EnterCriticalSection(&_pcsLock);

	HRESULT hr = _CheckShutdown();

	if (SUCCEEDED(hr)){
		hr = _spEventQueue->QueueEventParamVar(
			meType,
			guidExtendedType,
			hrStatus,
			pvarValue
		);
	}

	LeaveCriticalSection(&_pcsLock);

	return hr;
}


///////////////////////////////////////////////////////////
//   IMFShutdown Interface
///////////////////////////////////////////////////////////

STDMETHODIMP DecoderBase::GetShutdownStatus(
	__out MFSHUTDOWN_STATUS * pStatus
) {
	HRESULT hr = (pStatus) ? S_OK : E_POINTER;
	if (SUCCEEDED(hr))
	{
		EnterCriticalSection(&_pcsLock);

		hr = _CheckShutdown();
		if (SUCCEEDED(hr))
		{
			hr = MF_E_INVALIDREQUEST;
		}
		else
		{
			hr = S_OK;
			if (!_bShutdown && _bShutdownStarted)
				*pStatus = MFSHUTDOWN_INITIATED;
			else
				*pStatus = MFSHUTDOWN_COMPLETED;
		}

		LeaveCriticalSection(&_pcsLock);
	}
	return hr;
}
STDMETHODIMP DecoderBase::Shutdown() {
	HRESULT hr = S_OK;

	// Begin Check Shutdown
	EnterCriticalSection(&_pcsLock);

	hr = _CheckShutdown();
	if (SUCCEEDED(hr))
		_bShutdownStarted = TRUE;

	LeaveCriticalSection(&_pcsLock);
	// End Check Shutdown


	if (SUCCEEDED(hr))
	{
		// Perform Shutdown events here
		// TODO: Drain decoder

		// Because _FinalConstruct could fail and not get the Event Queue
		// created we must check for null here
		if (_spEventQueue)
		{
			_spEventQueue->Shutdown();
			_spEventQueue->Release();
		}

		EnterCriticalSection(&_pcsLock);
		_bShutdown = TRUE;
		LeaveCriticalSection(&_pcsLock);

		if (_spInputType)
			_spInputType.Release();

		if (_spOutputType)
			_spOutputType.Release();

		if (_spSuggestedOutputType)
			_spSuggestedOutputType.Release();

		if (_pContext)
			delete _pContext;
	}
	return hr;
}

///////////////////////////////////////////////////////////
//   IMFTransform Interface Methods
///////////////////////////////////////////////////////////

STDMETHODIMP DecoderBase::AddInputStreams(
	__in DWORD cStreams,
	__in_ecount(cStreams) DWORD * rgdwStreamIDs
) {
	return E_NOTIMPL;
}

STDMETHODIMP DecoderBase::DeleteInputStream(__in DWORD dwStreamID)
{
	return E_NOTIMPL;
}

STDMETHODIMP DecoderBase::GetAttributes(
	__deref_out IMFAttributes ** ppAttributes
) {
	return E_NOTIMPL;
}

STDMETHODIMP DecoderBase::GetInputAvailableType(
	__in DWORD dwInputStreamID,
	__in DWORD dwTypeIndex,
	__deref_out IMFMediaType ** ppType
) {
	HRESULT hr = (ppType) ? S_OK : E_POINTER;
	if (SUCCEEDED(hr))
	{
		if (dwInputStreamID != 0)
			hr = MF_E_INVALIDSTREAMNUMBER;
		else if (dwTypeIndex >= FFMPEG_INPUT_COUNT)
			hr = MF_E_NO_MORE_TYPES;
	}

	CComPtr<IMFMediaType> spType = NULL;
	if (SUCCEEDED(hr))
		hr = MFCreateMediaType(&spType);

	// Audio and then Video
	if (SUCCEEDED(hr)) {
		if (dwTypeIndex < FFMPEG_INPUT_AUDIO_COUNT)
			hr = spType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		else
			hr = spType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}

	if (SUCCEEDED(hr)) {
		if (dwTypeIndex < FFMPEG_INPUT_AUDIO_COUNT) {
			hr = spType->SetGUID(
				MF_MT_SUBTYPE,
				FFMPEG_MFTYPES_INPUT_AUDIO[dwTypeIndex]
			);
		}
		else
		{
			hr = spType->SetGUID(
				MF_MT_SUBTYPE,
				FFMPEG_MFTYPES_INPUT_VIDEO[dwTypeIndex - FFMPEG_INPUT_AUDIO_COUNT]
			);
		}
	}

	// TODO: Specific parameter requirements?

	if (SUCCEEDED(hr))
		*ppType = spType.Detach();

	return hr;
}

STDMETHODIMP DecoderBase::GetInputCurrentType(
	__in DWORD dwInputStreamID,
	__deref_out IMFMediaType ** ppType
) {
	HRESULT hr = (ppType) ? S_OK : E_POINTER;

	if (SUCCEEDED(hr))
	{
		*ppType = NULL;
		
		if (dwInputStreamID != 0)
			hr = MF_E_INVALIDSTREAMNUMBER;
		else if (!_spInputType)
			hr = MF_E_TRANSFORM_TYPE_NOT_SET;
	}

	CComPtr<IMFMediaType> spType;
	if (SUCCEEDED(hr))		
		hr = MFCreateMediaType(&spType);
		
	if (SUCCEEDED(hr))
		hr = _spInputType->CopyAllItems(spType);
	
	if (SUCCEEDED(hr))
		*ppType = spType.Detach();
	
	return hr;
}

STDMETHODIMP DecoderBase::GetInputStatus(
	__in DWORD dwInputStreamID,
	__out DWORD * pdwFlags
) {
	EnterCriticalSection(&_pcsLock);

	HRESULT hr = (pdwFlags) ? S_OK : E_POINTER;
	if (SUCCEEDED(hr))
	{
		VARIANT_BOOL vtbInputTypeSet = VARIANT_FALSE;
		if (dwInputStreamID != 0)
			hr = MF_E_INVALIDSTREAMNUMBER;
		
		else if (FAILED(IsInputTypeSet(&vtbInputTypeSet)) || vtbInputTypeSet == VARIANT_FALSE)
			hr = MF_E_TRANSFORM_TYPE_NOT_SET;

		
		// TODO: read input status from FFmpegContext
		if (SUCCEEDED(hr))
			*pdwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;
	}

	LeaveCriticalSection(&_pcsLock);

	return hr;
}

STDMETHODIMP DecoderBase::GetInputStreamAttributes(
	__in DWORD dwInputStreamID,
	__deref_out IMFAttributes ** ppAttributes
) {
	return E_NOTIMPL;
}

STDMETHODIMP DecoderBase::GetInputStreamInfo(
	__in DWORD dwInputStreamID,
	__out MFT_INPUT_STREAM_INFO * pStreamInfo
) {
	HRESULT hr = (pStreamInfo) ? S_OK : E_POINTER;

	if (SUCCEEDED(hr) && dwInputStreamID != 0)
		hr = MF_E_INVALIDSTREAMNUMBER;

	if (SUCCEEDED(hr))
	{
		pStreamInfo->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES
			| MFT_INPUT_STREAM_DOES_NOT_ADDREF;

		pStreamInfo->hnsMaxLatency = 0; // No buffering
		pStreamInfo->cbSize = 0; // No min buffer size?
		pStreamInfo->cbAlignment = 0; // TODO?
		pStreamInfo->cbMaxLookahead = 0; // No Lookahead
	}

	return hr;
}

STDMETHODIMP DecoderBase::GetOutputAvailableType(
	__in DWORD dwOutputStreamID,
	__in DWORD dwTypeIndex,
	__deref_out IMFMediaType ** ppType
) {
	HRESULT hr = (ppType) ? S_OK : E_POINTER;
	if (SUCCEEDED(hr))
	{
		if (dwOutputStreamID != 0)
			hr = MF_E_INVALIDSTREAMNUMBER;
		
		else if (!_spInputType)
			hr = MF_E_TRANSFORM_TYPE_NOT_SET;

		else if (m_guidMajorType == MFMediaType_Audio
			&& dwTypeIndex >= ARRAYSIZE(FFMPEG_MFTYPES_OUTPUT_AUDIO))
			hr = MF_E_NO_MORE_TYPES;

		else if (m_guidMajorType == MFMediaType_Video
			&& dwTypeIndex >= ARRAYSIZE(FFMPEG_MFTYPES_OUTPUT_VIDEO))
			hr = MF_E_NO_MORE_TYPES;
	}

	CComPtr<IMFMediaType> spType = NULL;
	if (SUCCEEDED(hr))
		hr = MFCreateMediaType(&spType);

	// Don't compute values when a suggested type has been set.
	if (_spSuggestedOutputType)
	{
		_spSuggestedOutputType->CopyAllItems(spType);
		*ppType = spType.Detach();
		return hr;
	}	

	if (SUCCEEDED(hr))
		hr = spType->SetGUID(MF_MT_MAJOR_TYPE, m_guidMajorType);

	if (SUCCEEDED(hr))
	{
		if (m_guidMajorType == MFMediaType_Audio)
		{
			hr = spType->SetGUID(MF_MT_SUBTYPE, FFMPEG_MFTYPES_OUTPUT_AUDIO[dwTypeIndex]);
			if (SUCCEEDED(hr)) hr = _SetOutputTypeAudioProperties(spType);
		}
		else
		{
			hr = spType->SetGUID(MF_MT_SUBTYPE, FFMPEG_MFTYPES_OUTPUT_VIDEO[dwTypeIndex]);
			if (SUCCEEDED(hr)) hr = _SetOutputTypeVideoProperties(spType);
		}
	}

	if (SUCCEEDED(hr))
		*ppType = spType.Detach();

	return hr;
}

STDMETHODIMP DecoderBase::GetOutputCurrentType(
	__in DWORD dwOutputStreamID,
	__deref_out IMFMediaType ** ppType
) {
	HRESULT hr = (ppType) ? S_OK : E_POINTER;
	if (SUCCEEDED(hr))
	{
		*ppType = NULL;
		if (dwOutputStreamID != 0)
			hr = MF_E_INVALIDSTREAMNUMBER;
		else if (!_spOutputType)
			hr = MF_E_TRANSFORM_TYPE_NOT_SET;
	}

	CComPtr<IMFMediaType> spType;
	if (SUCCEEDED(hr))
		hr = MFCreateMediaType(&spType);
	
	if (SUCCEEDED(hr))
		hr = _spOutputType->CopyAllItems(spType);

	if (SUCCEEDED(hr))
		*ppType = spType.Detach();

	return hr;
}

STDMETHODIMP DecoderBase::GetOutputStatus(__out DWORD * pdwFlags)
{
	EnterCriticalSection(&_pcsLock);

	HRESULT hr = (pdwFlags) ? S_OK : E_POINTER;
	if (SUCCEEDED(hr))
	{
		VARIANT_BOOL vtbOutputSet = VARIANT_FALSE;
		if (FAILED(IsOutputTypeSet(&vtbOutputSet)) || VARIANT_FALSE == vtbOutputSet)
			hr = MF_E_TRANSFORM_TYPE_NOT_SET;

		// TODO: get output status from FFmpegContext
		//if (SUCCEEDED(hr))
		//	*pdwFlags = (_dwOutputRequests) ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;
	}

	LeaveCriticalSection(&_pcsLock);

	return hr;
}

STDMETHODIMP DecoderBase::GetOutputStreamAttributes(
	__in DWORD dwOutputStreamID,
	__deref_out IMFAttributes ** ppAttributes
) {
	return E_NOTIMPL;
}

STDMETHODIMP DecoderBase::GetOutputStreamInfo(
	__in DWORD dwOutputStreamID,
	__out MFT_OUTPUT_STREAM_INFO * pStreamInfo
) {
	

	HRESULT hr = (pStreamInfo) ? S_OK : E_POINTER;
	if (SUCCEEDED(hr))
	{
		// TODO: Use RtlZeroMemory?
		memset(pStreamInfo, 0, sizeof(MFT_OUTPUT_STREAM_INFO));
		if (dwOutputStreamID != 0)
			hr = MF_E_INVALIDSTREAMNUMBER;
	}

	if (FAILED(hr)) return hr;
	
	EnterCriticalSection(&_pcsLock);

	// On video, get sample buffer from pipeline, avoids extra copying
	if (_spOutputType && m_guidMajorType == MFMediaType_Video)
	{
		// width and height
		UINT32 width, height;
		if (SUCCEEDED(hr))
			hr = MFGetAttributeSize(_spOutputType, MF_MT_FRAME_SIZE, &width, &height);

		if (SUCCEEDED(hr))
		{
			// ONLY WORKS FOR 12BPP - could change if needed.
			pStreamInfo->cbSize = (width * height * (FFMPEG_OUTPUT_BITS_PER_PIXEL / 8.0));
			pStreamInfo->dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES;
		}
	}
	else
	{
		// For audio and others, allocate own samples bc size is variable
		pStreamInfo->cbSize = FFMPEG_OUTPUT_BUFFER_SIZE;
		pStreamInfo->dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES
			//| MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
			//| MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES
			| MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
	}

	if (SUCCEEDED(hr))
	{
		pStreamInfo->cbAlignment = 0;
	}

	LeaveCriticalSection(&_pcsLock);
	return hr;
}

STDMETHODIMP DecoderBase::GetStreamCount(
	__out DWORD * pcInputStreams,
	__out DWORD * pcOutputStreams
) {
	HRESULT hr = (pcInputStreams && pcOutputStreams) ? S_OK : E_POINTER;
	
	if (SUCCEEDED(hr))
		*pcInputStreams = *pcOutputStreams = 1;
	
	return hr;
}

STDMETHODIMP DecoderBase::GetStreamIDs(
	__in DWORD dwInputIDArraySize,
	__inout_ecount(dwInputIDArraySize) DWORD * pdwInputIDs,
	__in DWORD dwOutputIDArraySize,
	__inout_ecount(dwInputIDArraySize) DWORD * pdwOutputIDs
) {
	return E_NOTIMPL;
}

STDMETHODIMP DecoderBase::GetStreamLimits(
	__out DWORD * pdwInputMinimum,
	__out DWORD * pdwInputMaximum,
	__out DWORD * pdwOutputMinimum,
	__out DWORD * pdwOutputMaximum
) {
	EnterCriticalSection(&_pcsLock);

	HRESULT hr = (
		pdwInputMaximum
		&& pdwInputMinimum 
		&& pdwOutputMaximum 
		&& pdwOutputMinimum
	) ? S_OK : E_POINTER;

	if (SUCCEEDED(hr))
	{
		*pdwInputMaximum = 1;
		*pdwInputMinimum = 1;
		*pdwOutputMaximum = 1;
		*pdwOutputMinimum = 1;
	}

	LeaveCriticalSection(&_pcsLock);
	return hr;
}

STDMETHODIMP DecoderBase::ProcessEvent(
	__in DWORD dwInputStreamID,
	__in IMFMediaEvent * pEvent
) {
	return E_NOTIMPL;
}


STDMETHODIMP DecoderBase::ProcessInput(
	__in DWORD dwInputStreamID,
	__in IMFSample * pSample,
	__in DWORD dwFlags
) {
	// Must be implemented - see FFmpegDecoderMFT
	return E_NOTIMPL;
}


STDMETHODIMP DecoderBase::ProcessMessage(
	__in MFT_MESSAGE_TYPE eMessage,
	__in ULONG_PTR ulParam
) {
	// Must be implemented - see FFmpegDecoderMFT
	return E_NOTIMPL;
}

STDMETHODIMP DecoderBase::ProcessOutput(
	__in DWORD dwFlags,
	__in DWORD cOutputBufferCount,
	__inout_ecount(cOutputBufferCount) MFT_OUTPUT_DATA_BUFFER * pOutputSamples,
	__out DWORD *pdwStatus
) {
	// Must be implemented - see FFmpegDecoderMFT
	return E_NOTIMPL;
}


STDMETHODIMP DecoderBase::SetInputType(
	__in DWORD dwInputStreamID,
	__in IMFMediaType * pType,
	__in DWORD dwFlags
) {
	HRESULT hr = S_OK;
	
	// TODO if stream started return error, cant change stream in middle
	if (dwInputStreamID != 0)
		hr = MF_E_INVALIDSTREAMNUMBER;
	
	if (FAILED(hr)) return hr;

	// Verify the type is valid
	if (pType == NULL)
	{
		if(_spInputType) _spInputType.Release();
		_spInputType = nullptr;
		return hr;
	}

	hr = ValidateInputType(pType);
	if (FAILED(hr)) return hr;

	// We're done checking, if not setting, end here
	if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
		return hr;

	// TODO extra properties?

	// Copy major type for easy access
	if (SUCCEEDED(hr))
		hr = pType->GetGUID(MF_MT_MAJOR_TYPE, &m_guidMajorType);

	if (SUCCEEDED(hr))
	{
		if (_spInputType) _spInputType.Release();
		_spInputType = nullptr;

		MFCreateMediaType(&_spInputType);
		pType->CopyAllItems(_spInputType);

		if (!_spInputType)
			hr = E_UNEXPECTED;

		if (SUCCEEDED(hr))
		{
			// Rebuild context - handles input type changes
			_bFormatChange = true;
		}

		//_UpdateOutputTypeProperties();
	}
	
	
	return hr;
}

STDMETHODIMP DecoderBase::SetOutputBounds(
	__in LONGLONG hnsLowerBound,
	__in LONGLONG hnsUpperBound
) {
	return E_NOTIMPL;
}

STDMETHODIMP DecoderBase::SetOutputType(
	__in DWORD dwOutputStreamID,
	__in IMFMediaType * pType,
	__in DWORD dwFlags
) {
	// Input type should be set first!
	HRESULT hr = S_OK;

	if (dwOutputStreamID != 0)
		hr = MF_E_INVALIDSTREAMNUMBER;

	else if (!_spInputType)
		hr = MF_E_TRANSFORM_TYPE_NOT_SET;

	if (FAILED(hr)) return hr;
	
	if (pType == NULL)
	{
		_spOutputType = NULL;
		return hr;
	}
	
	hr = ValidateOutputType(pType);
	
	if (FAILED(hr)) return hr;		
	
	// TODO - extra properties?
	//if (SUCCEEDED(hr))
	//	hr = _AddCodecPropertiesToOutputType(spNewOutputType);
					

	// Up to this point, only verification
	// Return if we want to only test if type is valid
	if (dwFlags & MFT_SET_TYPE_TEST_ONLY)
		return hr;

	
	if (SUCCEEDED(hr))
	{
		if (_spOutputType) _spOutputType.Release();
		_spOutputType = nullptr;

		if (_spSuggestedOutputType) _spSuggestedOutputType.Release();
		_spSuggestedOutputType = nullptr;

		MFCreateMediaType(&_spOutputType);
		pType->CopyAllItems(_spOutputType);

		if (SUCCEEDED(hr))
		{
			// Rebuild context - handles output type changes
			_bFormatChange = true;
		}

		//_UpdateInputTypeProperties();
	}
	
	return hr;
}

///////////////////////////////////////////////////////////
//   IMediaExtension Methods
///////////////////////////////////////////////////////////

STDMETHODIMP DecoderBase::SetProperties(
	__RPC__in_opt ABI::Windows::Foundation::Collections::IPropertySet * configuration
) {
	return S_OK;
}

///////////////////////////////////////////////////////////
//   IInspectable Interface
///////////////////////////////////////////////////////////
STDMETHODIMP DecoderBase::GetIids(
	__RPC__out ULONG *iidCount, 
	__RPC__deref_out_ecount_full_opt(*iidCount) IID **iids
) {
	HRESULT hr = (iidCount && iids) ? S_OK : E_POINTER;

	if(SUCCEEDED(hr))
	{
		*iidCount = 6;
		*iids = (IID *) CoTaskMemAlloc(sizeof(IID) * (*iidCount));
		if (iids == nullptr) hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		(*iids)[0] = IID_IUnknown;
		(*iids)[1] = IID_IMFTransform;
		(*iids)[2] = IID_IMFMediaEventGenerator;
		(*iids)[3] = IID_IMFShutdown;
		(*iids)[4] = IID_IInspectable;
		(*iids)[5] = __uuidof(ABI::Windows::Media::IMediaExtension);
	}

	return hr;
}
STDMETHODIMP DecoderBase::GetRuntimeClassName(__RPC__deref_out_opt HSTRING *className)
{
	if (!className) return E_POINTER;
	return WindowsCreateString(L"FFmpegPack.DecoderBase", 22, className);
}

STDMETHODIMP DecoderBase::GetTrustLevel(__RPC__out TrustLevel *trustLevel)
{
	if (!trustLevel) return E_POINTER;
	*trustLevel = BaseTrust;

	return S_OK;
}

///////////////////////////////////////////////////////////
//   Non Interface Methods
///////////////////////////////////////////////////////////

/**
* Checks whether the decoder has shutdown.
*
* @return Success if it has NOT shutdown, else MF_E_SHUTDOWN error.
*/
HRESULT DecoderBase::_CheckShutdown()
{
	return (_bShutdown ? MF_E_SHUTDOWN : S_OK);
}

/**
 * Checks whether an input type has been set.
 *
 * @param pvtbInputSet  set to true if it has been, else false.
 */
STDMETHODIMP DecoderBase::IsInputTypeSet(__out VARIANT_BOOL *pvtbInputSet)
{
	HRESULT hr = (pvtbInputSet) ? S_OK : E_POINTER;
	
	if (SUCCEEDED(hr))
		*pvtbInputSet = (_spInputType) ? VARIANT_TRUE : VARIANT_FALSE;
	
	return hr;
}

/**
 * Checks whether an output type has been set.
 *
 * @param pvtbInputSet  set to true if it has been, else false.
 */
STDMETHODIMP DecoderBase::IsOutputTypeSet(__out VARIANT_BOOL *pvtbOutputSet)
{
	HRESULT hr = (pvtbOutputSet) ? S_OK : E_POINTER;
	
	if (SUCCEEDED(hr))
		*pvtbOutputSet = (_spOutputType) ? VARIANT_TRUE : VARIANT_FALSE;
	
	return hr;
}


///////////////////////////////////////////////////////////
//   Other Methods
///////////////////////////////////////////////////////////
HRESULT DecoderBase::ValidateInputType(IMFMediaType *pType) {
	HRESULT hr = (pType) ? S_OK : E_POINTER;
	GUID newMajorType, newSubType;

	if (SUCCEEDED(hr))
		hr = pType->GetGUID(MF_MT_MAJOR_TYPE, &newMajorType);

	if (SUCCEEDED(hr))
		hr = pType->GetGUID(MF_MT_SUBTYPE, &newSubType);

	if (SUCCEEDED(hr))
	{
		hr = MF_E_INVALIDMEDIATYPE;
		int foundSubType;
		if (newMajorType == MFMediaType_Audio)
		{
			ARRAYFIND(FFMPEG_MFTYPES_INPUT_AUDIO, newSubType, &foundSubType);
			if (foundSubType >= 0) hr = S_OK;
		}
		else if (newMajorType == MFMediaType_Video)
		{
			ARRAYFIND(FFMPEG_MFTYPES_INPUT_VIDEO, newSubType, &foundSubType);
			if (foundSubType >= 0) hr = S_OK;
		}
	}

	// TODO: check other parameters?

	if (SUCCEEDED(hr))
	{
		if (newMajorType == MFMediaType_Audio)
		{
			// channels
			UINT32 channels;
			if (SUCCEEDED(hr))
				hr = pType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);

			
			// sample_rate - samples per second
			UINT32 sampleRate;
			if (SUCCEEDED(hr))
				hr = pType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);

		}
		else if (newMajorType == MFMediaType_Video)
		{
			// width and height
			UINT32 width, height;
			if (SUCCEEDED(hr))
				hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);			
		}
	}
	

	return hr;
}

HRESULT DecoderBase::ValidateOutputType(IMFMediaType *pType)
{
	// Precondition: input type is set
	HRESULT hr = (pType) ? S_OK : E_POINTER;
	GUID newMajorType, newSubType;

	if (SUCCEEDED(hr))
		hr = (_spInputType) ? S_OK : MF_E_TRANSFORM_TYPE_NOT_SET;

	if (SUCCEEDED(hr))
		hr = pType->GetGUID(MF_MT_MAJOR_TYPE, &newMajorType);

	if (SUCCEEDED(hr))
		hr = pType->GetGUID(MF_MT_SUBTYPE, &newSubType);

	if (SUCCEEDED(hr) && newMajorType != m_guidMajorType)
		hr = MF_E_INVALIDMEDIATYPE;

	// TODO: further parameter/format validation?
	
	return hr;
}

/**
 * Sets the output properties to uncompressed float wave audio,
 * based on the settings of the compressed input audio.
 */
HRESULT DecoderBase::_SetOutputTypeAudioProperties(IMFMediaType *pType)
{
	HRESULT hr = (pType) ? S_OK : E_POINTER;
	UINT32 sampleRate, channelCount;

	// Get input parameters
	
	if (SUCCEEDED(hr))
		hr = _spInputType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);

	if (SUCCEEDED(hr))
		hr = _spInputType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channelCount);

	// Set all the output parameters
	
	if (SUCCEEDED(hr))
		hr = pType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);

	if (SUCCEEDED(hr))
		hr = pType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channelCount);

	if (SUCCEEDED(hr))
		hr = pType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, sizeof(float) * 8);

	if (SUCCEEDED(hr))
		hr = pType->SetUINT32(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, sizeof(float) * 8);

	if (SUCCEEDED(hr))
	{
		// 5.1
		if(channelCount == 6)
			hr = pType->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, 0x6CC);

		else if (channelCount == 2)
			hr = pType->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, 0x3);

		// for mono channel mask should be 0x4 (CENTER)
		else if (channelCount == 1)
			hr = pType->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, 0x4);
	}
	

	if (SUCCEEDED(hr))
		hr = pType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, sizeof(float) * channelCount * sampleRate);

	if (SUCCEEDED(hr))
		hr = pType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, sizeof(float) * channelCount);

	if (SUCCEEDED(hr))
		hr = pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

	return hr;
}


HRESULT DecoderBase::_SetOutputTypeVideoProperties(IMFMediaType *pType)
{
	// TODO: not requiring any particular property...is that okay?
	HRESULT hr = (pType && _spInputType) ? S_OK : E_POINTER;

	UINT32 frameWidth, frameHeight, avgBitrate, videoRotation;
	//UINT32 ratioNum, ratioDenom;
	UINT64 frameRate;

	if (FAILED(hr)) return hr;
	
	// Transfer parameters to output
	if (SUCCEEDED(_spInputType->GetUINT32(MF_MT_AVG_BITRATE, &avgBitrate)))
		hr = pType->SetUINT32(MF_MT_AVG_BITRATE, avgBitrate);

	if (SUCCEEDED(_spInputType->GetUINT32(MF_MT_VIDEO_ROTATION, &videoRotation)))
		hr = pType->SetUINT32(MF_MT_VIDEO_ROTATION, videoRotation);

	if (SUCCEEDED(_spInputType->GetUINT64(MF_MT_FRAME_RATE, &frameRate)))
		hr = pType->SetUINT64(MF_MT_FRAME_RATE, frameRate);
		
	if (SUCCEEDED(MFGetAttributeSize(_spInputType, MF_MT_FRAME_SIZE, &frameWidth, &frameHeight)))
		hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, frameWidth, frameHeight);

	//if (SUCCEEDED(MFGetAttributeRatio(_spInputType, MF_MT_PIXEL_ASPECT_RATIO, &ratioNum, &ratioDenom)))
	//	hr = MFSetAttributeRatio(_spInputType, MF_MT_PIXEL_ASPECT_RATIO, ratioNum, ratioDenom);
	
		

	return hr;
}