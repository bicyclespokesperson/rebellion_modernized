// minwin: WINMM -- multimedia timers, the MCI command interface (CD audio) and WAVEFORMATEX.
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/base.h"

struct HDRVR__;
using MMRESULT = UINT;
using HDRVR = struct HDRVR__*;
using MCIERROR = DWORD;
using MCIDEVICEID = UINT;
struct tagMCI_GENERIC_PARMS
{
  DWORD_PTR dwCallback;
};
using MCI_GENERIC_PARMS = struct tagMCI_GENERIC_PARMS;
struct tagMCI_OPEN_PARMSA
{
  DWORD_PTR dwCallback;
  MCIDEVICEID wDeviceID;
  LPCSTR lpstrDeviceType;
  LPCSTR lpstrElementName;
  LPCSTR lpstrAlias;
};
using MCI_OPEN_PARMSA = struct tagMCI_OPEN_PARMSA;
using MCI_OPEN_PARMS = MCI_OPEN_PARMSA;
struct tagMCI_PLAY_PARMS
{
  DWORD_PTR dwCallback;
  DWORD dwFrom;
  DWORD dwTo;
};
using MCI_PLAY_PARMS = struct tagMCI_PLAY_PARMS;
struct tagMCI_STATUS_PARMS
{
  DWORD_PTR dwCallback;
  DWORD_PTR dwReturn;
  DWORD dwItem;
  DWORD dwTrack;
};
using MCI_STATUS_PARMS = struct tagMCI_STATUS_PARMS;
struct tagMCI_SET_PARMS
{
  DWORD_PTR dwCallback;
  DWORD dwTimeFormat;
  DWORD dwAudio;
};
using MCI_SET_PARMS = struct tagMCI_SET_PARMS;
using TIMECALLBACK = void REB_STDCALL (UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
using LPTIMECALLBACK = TIMECALLBACK*;
#pragma pack(push, 1)
struct tWAVEFORMATEX
{
  WORD wFormatTag;
  WORD nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD nBlockAlign;
  WORD wBitsPerSample;
  WORD cbSize;
};
#pragma pack(pop)
using WAVEFORMATEX = struct tWAVEFORMATEX;
using LPWAVEFORMATEX = struct tWAVEFORMATEX*;
using LPCWAVEFORMATEX = const WAVEFORMATEX*;
struct timecaps_tag
{
  UINT wPeriodMin;
  UINT wPeriodMax;
};
using TIMECAPS = struct timecaps_tag;
using LPTIMECAPS = struct timecaps_tag*;

extern "C" {
MINWIN_IMPORT MCIERROR REB_STDCALL mciSendCommandA(MCIDEVICEID mciId,
                                                   UINT uMsg,
                                                   DWORD_PTR dwParam1,
                                                   DWORD_PTR dwParam2);
MINWIN_IMPORT MCIDEVICEID REB_STDCALL mciGetDeviceIDA(LPCSTR pszDevice);
MINWIN_IMPORT HDRVR REB_STDCALL OpenDriver(LPCWSTR szDriverName, LPCWSTR szSectionName, LPARAM lParam2);
MINWIN_IMPORT MMRESULT REB_STDCALL
timeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK fptc, DWORD_PTR dwUser, UINT fuEvent);
MINWIN_IMPORT MMRESULT REB_STDCALL timeKillEvent(UINT uTimerID);
MINWIN_IMPORT DWORD REB_STDCALL timeGetTime(void);
MINWIN_IMPORT MMRESULT REB_STDCALL timeGetDevCaps(LPTIMECAPS ptc, UINT cbtc);
MINWIN_IMPORT MMRESULT REB_STDCALL timeBeginPeriod(UINT uPeriod);
MINWIN_IMPORT MMRESULT REB_STDCALL timeEndPeriod(UINT uPeriod);
}

#define MM_MCINOTIFY 0x3b9
#define MCI_OPEN 0x803
#define MCI_CLOSE 0x804
#define MCI_PLAY 0x806
#define MCI_STOP 0x808
#define MCI_SET 0x80d
#define MCI_STATUS 0x814
#define MCI_DEVTYPE_CD_AUDIO 0x204
#define MCI_FORMAT_TMSF 0xa
#define MCI_NOTIFY ((LONG)1)
#define MCI_WAIT ((LONG)2)
#define MCI_OPEN_ELEMENT ((LONG)0x200)
#define MCI_OPEN_TYPE_ID ((LONG)0x1000)
#define MCI_OPEN_TYPE ((LONG)0x2000)
#define MCI_STATUS_ITEM ((LONG)0x100)
#define MCI_SET_TIME_FORMAT ((LONG)0x400)
#define SEEK_SET 0
#define SEEK_END 2
#define TIME_PERIODIC 1
#define WAVE_FORMAT_PCM 1
#define TIMERR_NOERROR 0
