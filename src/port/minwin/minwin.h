// minwin: the Win32 / COM / DirectX / WinMM / VfW declarations the reconstructed game uses, in place of <windows.h> and
// friends. Selected by platform/win32.h when REB_MINWIN is defined. One header per DLL:
//   core      scalar types and calling-convention macros      base      handles, GUID, RECT/POINT, IUnknown, HRESULT
//   kernel32  threads, files, resources, time                 gdi32     DCs, bitmaps, palettes, fonts, text
//   user32    windows, messages, input, painting              winmm     timers, MCI, WAVEFORMATEX
//   vfw       AVIFile                                         comctl32  tooltips
//   advapi32  registry, user name                             version   file version resources
//   ddraw     DirectDraw (mode switch only)                   dsound    DirectSound
// Declarations are written for ILP32 and LP64 alike (see core.h).
#pragma once
#include "port/minwin/advapi32.h"
#include "port/minwin/base.h"
#include "port/minwin/comctl32.h"
#include "port/minwin/core.h"
#include "port/minwin/ddraw.h"
#include "port/minwin/dsound.h"
#include "port/minwin/gdi32.h"
#include "port/minwin/kernel32.h"
#include "port/minwin/user32.h"
#include "port/minwin/version.h"
#include "port/minwin/vfw.h"
#include "port/minwin/winmm.h"
