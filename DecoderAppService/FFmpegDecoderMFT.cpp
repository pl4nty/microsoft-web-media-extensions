//Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "FFmpegTypes.h"
#include "FFmpegDecoderMFT.h"

using namespace FFmpegPack;


FFmpegDecoderMFT::FFmpegDecoderMFT() :
	_bStreaming(false),
	_bDraining(false)
{
	
	_pPacket = av_packet_alloc();
}

FFmpegDecoderMFT::~FFmpegDecoderMFT()
{
	if(_pPacket)
		av_packet_free(&_pPacket);
}

HRESULT FFmpegDecoderMFT::CreateInstance(
	__deref_out IMFTransform ** ppFFmpegMFT
) {
	HRESULT hr = (ppFFmpegMFT) ? S_OK : E_POINTER;
	FFmpegDecoderMFT * decoder = nullptr;
	if (SUCCEEDED(hr))
	{
		decoder = new FFmpegDecoderMFT();
		hr = (decoder) ? S_OK : E_OUTOFMEMORY;
	}
		
	if (SUCCEEDED(hr))
	{
		decoder->QueryInterface(IID_IMFTransform, (void **)ppFFmpegMFT);
		decoder->Release();
	}

	return hr;
}


///////////////////////////////////////////////////////////
//   IMFTransform Interface Methods
///////////////////////////////////////////////////////////

/**
 * Process a compressed sample and queue its decoded output.
 *
 * @param dwInputStreamID  the stream of this transform getting a new sample.
 * @param pSample          the data of the sample, including time and duration.
 * @param dwFlags          input settings bitmask, MUST be 0.
 *
 * @return S_OK on success, an error code on failure:
 *		MF_E_INVALIDSTREAMNUMBER  on wrong stream number
 *      MF_E_NOTACCEPTING         if currently draining
 */
STDMETHODIMP FFmpegDecoderMFT::ProcessInput(
	__in DWORD dwInputStreamID, 
	__in IMFSample * pSample, 
	__in DWORD dwFlags
){
	HRESULT hr = (pSample) ? S_OK : E_POINTER;
	DWORD bufferCount;

	if (SUCCEEDED(hr))
	{
		if (dwInputStreamID != 0)
			hr = MF_E_INVALIDSTREAMNUMBER;

		else if (dwFlags != 0)
			hr = E_INVALIDARG;
	}

	if (SUCCEEDED(hr)) {
		hr = pSample->GetBufferCount(&bufferCount);
		if (SUCCEEDED(hr) && bufferCount == 0)
			return S_OK;
	}
	
	if (FAILED(hr)) return hr;

	EnterCriticalSection(&_pcsLock);
	hr = _CheckShutdown();
	
	// Check if can take in input
	if (SUCCEEDED(hr))
		hr = (_bDraining || _bFormatChange) ? MF_E_NOTACCEPTING : S_OK;

	LeaveCriticalSection(&_pcsLock);

	if (SUCCEEDED(hr))
	{
		if(!_pContext->HasInitialized())
			hr = _pContext->Initialize(_spInputType, _spOutputType);
	}

	if (SUCCEEDED(hr))
	{
		// Process input sample as AVPacket
		hr = FFmpegTypes::PacketFromSample(_pPacket, pSample);
		
		if (SUCCEEDED(hr))
			hr = _pContext->Decode(_pPacket);

	}
	return hr;
}

/**
 * Marks the start of a stream, makes sure the FFmpeg decoding context
 * is ready to receive input.
 */
HRESULT FFmpegDecoderMFT::StartStream() {
	// Its fine if stream has already started (ie seek)...just restart
	HRESULT hr = S_OK;

	hr = _pContext->Flush();

	if (SUCCEEDED(hr))
		_bStreaming = true;

	return hr;
}

/** Process a stream status message */
STDMETHODIMP FFmpegDecoderMFT::ProcessMessage(
	__in MFT_MESSAGE_TYPE eMessage, 
	__in ULONG_PTR ulParam
){

	EnterCriticalSection(&_pcsLock);

	HRESULT hr = S_OK;
	
	switch (eMessage)
	{
		case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
			hr = StartStream();
		break;
		case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
		{
			// Stop asking for input, but do not reset the encoder as they can flush or drain
			if (ulParam == 0)
			{	
				if (_bStreaming)  _bStreaming = false;
			} else {
				hr = MF_E_INVALIDSTREAMNUMBER;
			}

		}
		break;

		// Stop accepting input until all possible output is queued
		case MFT_MESSAGE_COMMAND_DRAIN:
		{
			if(_pContext->HasInitialized())
			{
				hr = _pContext->FlushInput();
				_bDraining = true;
			}
		}
		break;

		// Drop all current samples - must be ready for new ones right away
		case MFT_MESSAGE_COMMAND_FLUSH:
			if (_pContext->HasInitialized())
			{
				hr = _pContext->Flush();
			}
			
		break;
		
		default:
			// We do not handle this type of message so pass it downstream
			hr = S_OK;
			break;
	}

	LeaveCriticalSection(&_pcsLock);

	return hr;
}

/**
* Retrieve a decoded media frame.
*
* @param dwInputStreamID     the stream of this transform.
* @param cOutputBufferCount  the number of buffers in the input.
* @param pOutputSamples      a pointer to the output samples, allocated by caller.
*
* @return S_OK on success, an error code on failure.
*/
STDMETHODIMP FFmpegDecoderMFT::ProcessOutput(
	__in DWORD dwFlags, 
	__in DWORD cOutputBufferCount, 
	__inout_ecount(cOutputBufferCount) MFT_OUTPUT_DATA_BUFFER * pOutputSamples, 
	__out DWORD *pdwStatus
){
	HRESULT hr = (pOutputSamples && pdwStatus) ? S_OK : E_POINTER;
	if (SUCCEEDED(hr))
	{
		*pdwStatus = 0;

		// We need one buffer and an allocated sample
		if (cOutputBufferCount < 1)
			hr = E_INVALIDARG;

		else if (pOutputSamples[0].dwStreamID != 0)
			hr = MF_E_INVALIDSTREAMNUMBER;
	}

	if (SUCCEEDED(hr) && _pContext->HasFormatChange())
	{
		hr = MFCreateMediaType(&_spSuggestedOutputType);
		
		if (SUCCEEDED(hr))
		{
			_spOutputType->CopyAllItems(_spSuggestedOutputType);
			_spOutputType.Release();
			pOutputSamples[0].dwStatus |= MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE;

			return MF_E_TRANSFORM_STREAM_CHANGE;
		}
	}

	if (SUCCEEDED(hr))
	{
		EnterCriticalSection(&_pcsLock);

		CComPtr<IMFSample> spSample;

		// TODO : Dont need markers for FFmpeg, RIGHT?
		if (SUCCEEDED(hr))
			hr = _pContext->GetNextSample(&spSample);

		if (SUCCEEDED(hr))
		{
			// We allocate our own samples
			if (pOutputSamples[0].pSample == NULL)
				hr = spSample.CopyTo(&(pOutputSamples[0].pSample));

			// Pipeline allocates sample
			else
			{
				hr = FFmpegTypes::CopySample(spSample, pOutputSamples[0].pSample);
				spSample->RemoveAllBuffers();
			}

			spSample.Release();
		}


		if (_bDraining && !_pContext->HasNextSample())
			_bDraining = false;

		if (_bFormatChange && !_pContext->HasNextSample())
		{
			// Rebuild context - handles output type changes
			// We don't want to drop samples in between type changes!
			delete _pContext;
			_pContext = ref new FFmpegContext();
			_bFormatChange = false;
		}

		LeaveCriticalSection(&_pcsLock);
	}

	return hr;
}

