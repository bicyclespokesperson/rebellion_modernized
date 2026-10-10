// minwin: DSOUND -- DirectSound: the device, buffers, 3D listener and position notification used by the sound mixer.
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/base.h"
#include "port/minwin/winmm.h"

struct _D3DVECTOR;
struct _DSCAPS;
struct _DSBCAPS;
struct _DSBUFFERDESC;
struct _DS3DLISTENER;
typedef float D3DVALUE;
typedef struct _D3DVECTOR D3DVECTOR;
typedef struct IDirectSound *LPDIRECTSOUND;
typedef struct IDirectSoundBuffer *LPDIRECTSOUNDBUFFER;
typedef struct _DSCAPS *LPDSCAPS;
typedef struct _DSBCAPS *LPDSBCAPS;
typedef struct _DSBUFFERDESC DSBUFFERDESC;
typedef const DSBUFFERDESC *LPCDSBUFFERDESC;
struct _DSBUFFERDESC1 {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
};
typedef struct _DSBUFFERDESC1 DSBUFFERDESC1;
typedef struct _DS3DLISTENER DS3DLISTENER;
typedef struct _DS3DLISTENER *LPDS3DLISTENER;
typedef const DS3DLISTENER *LPCDS3DLISTENER;
struct _DSBPOSITIONNOTIFY {
    DWORD dwOffset;
    HANDLE hEventNotify;
};
typedef struct _DSBPOSITIONNOTIFY DSBPOSITIONNOTIFY;
typedef const DSBPOSITIONNOTIFY *LPCDSBPOSITIONNOTIFY;
struct IDirectSound : public IUnknown {
    virtual HRESULT REB_STDCALL QueryInterface(const IID &, LPVOID *) = 0;
    virtual ULONG REB_STDCALL AddRef() = 0;
    virtual ULONG REB_STDCALL Release() = 0;
    virtual HRESULT REB_STDCALL CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter) = 0;
    virtual HRESULT REB_STDCALL GetCaps(LPDSCAPS pDSCaps) = 0;
    virtual HRESULT REB_STDCALL DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER pDSBufferOriginal, LPDIRECTSOUNDBUFFER *ppDSBufferDuplicate) = 0;
    virtual HRESULT REB_STDCALL SetCooperativeLevel(HWND hwnd, DWORD dwLevel) = 0;
    virtual HRESULT REB_STDCALL Compact() = 0;
    virtual HRESULT REB_STDCALL GetSpeakerConfig(LPDWORD pdwSpeakerConfig) = 0;
    virtual HRESULT REB_STDCALL SetSpeakerConfig(DWORD dwSpeakerConfig) = 0;
    virtual HRESULT REB_STDCALL Initialize(LPCGUID pcGuidDevice) = 0;
};
struct IDirectSoundBuffer : public IUnknown {
    virtual HRESULT REB_STDCALL QueryInterface(const IID &, LPVOID *) = 0;
    virtual ULONG REB_STDCALL AddRef() = 0;
    virtual ULONG REB_STDCALL Release() = 0;
    virtual HRESULT REB_STDCALL GetCaps(LPDSBCAPS pDSBufferCaps) = 0;
    virtual HRESULT REB_STDCALL GetCurrentPosition(LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor) = 0;
    virtual HRESULT REB_STDCALL GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten) = 0;
    virtual HRESULT REB_STDCALL GetVolume(LPLONG plVolume) = 0;
    virtual HRESULT REB_STDCALL GetPan(LPLONG plPan) = 0;
    virtual HRESULT REB_STDCALL GetFrequency(LPDWORD pdwFrequency) = 0;
    virtual HRESULT REB_STDCALL GetStatus(LPDWORD pdwStatus) = 0;
    virtual HRESULT REB_STDCALL Initialize(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc) = 0;
    virtual HRESULT REB_STDCALL Lock(DWORD dwOffset, DWORD dwBytes, LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID *ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL SetCurrentPosition(DWORD dwNewPosition) = 0;
    virtual HRESULT REB_STDCALL SetFormat(LPCWAVEFORMATEX pcfxFormat) = 0;
    virtual HRESULT REB_STDCALL SetVolume(LONG lVolume) = 0;
    virtual HRESULT REB_STDCALL SetPan(LONG lPan) = 0;
    virtual HRESULT REB_STDCALL SetFrequency(DWORD dwFrequency) = 0;
    virtual HRESULT REB_STDCALL Stop() = 0;
    virtual HRESULT REB_STDCALL Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2) = 0;
    virtual HRESULT REB_STDCALL Restore() = 0;
};
extern "C" const GUID IID_IDirectSound3DListener;
struct IDirectSound3DListener : public IUnknown {
    virtual HRESULT REB_STDCALL QueryInterface(const IID &, LPVOID *) = 0;
    virtual ULONG REB_STDCALL AddRef() = 0;
    virtual ULONG REB_STDCALL Release() = 0;
    virtual HRESULT REB_STDCALL GetAllParameters(LPDS3DLISTENER pListener) = 0;
    virtual HRESULT REB_STDCALL GetDistanceFactor(D3DVALUE *pflDistanceFactor) = 0;
    virtual HRESULT REB_STDCALL GetDopplerFactor(D3DVALUE *pflDopplerFactor) = 0;
    virtual HRESULT REB_STDCALL GetOrientation(D3DVECTOR *pvOrientFront, D3DVECTOR *pvOrientTop) = 0;
    virtual HRESULT REB_STDCALL GetPosition(D3DVECTOR *pvPosition) = 0;
    virtual HRESULT REB_STDCALL GetRolloffFactor(D3DVALUE *pflRolloffFactor) = 0;
    virtual HRESULT REB_STDCALL GetVelocity(D3DVECTOR *pvVelocity) = 0;
    virtual HRESULT REB_STDCALL SetAllParameters(LPCDS3DLISTENER pcListener, DWORD dwApply) = 0;
    virtual HRESULT REB_STDCALL SetDistanceFactor(D3DVALUE flDistanceFactor, DWORD dwApply) = 0;
    virtual HRESULT REB_STDCALL SetDopplerFactor(D3DVALUE flDopplerFactor, DWORD dwApply) = 0;
    virtual HRESULT REB_STDCALL SetOrientation(D3DVALUE xFront, D3DVALUE yFront, D3DVALUE zFront, D3DVALUE xTop, D3DVALUE yTop, D3DVALUE zTop, DWORD dwApply) = 0;
    virtual HRESULT REB_STDCALL SetPosition(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD dwApply) = 0;
    virtual HRESULT REB_STDCALL SetRolloffFactor(D3DVALUE flRolloffFactor, DWORD dwApply) = 0;
    virtual HRESULT REB_STDCALL SetVelocity(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD dwApply) = 0;
    virtual HRESULT REB_STDCALL CommitDeferredSettings() = 0;
};
extern "C" const GUID IID_IDirectSoundNotify;
struct IDirectSoundNotify : public IUnknown {
    virtual HRESULT REB_STDCALL QueryInterface(const IID &, LPVOID *) = 0;
    virtual ULONG REB_STDCALL AddRef() = 0;
    virtual ULONG REB_STDCALL Release() = 0;
    virtual HRESULT REB_STDCALL SetNotificationPositions(DWORD dwPositionNotifies, LPCDSBPOSITIONNOTIFY pcPositionNotifies) = 0;
};

extern "C" {
MINWIN_IMPORT HRESULT REB_STDCALL DirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
}

#define DS_OK ((LONG)0)
#define DSSCL_EXCLUSIVE 3
#define DSBCAPS_PRIMARYBUFFER 1
#define DSBCAPS_CTRL3D 0x10
#define DSBCAPS_CTRLVOLUME 0x80
#define DSBCAPS_GLOBALFOCUS 0x8000
#define DSBPLAY_LOOPING 1
