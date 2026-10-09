// minwin: COMCTL32 -- the tooltip control (InitCommonControls and the TTM_* messages).
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/base.h"
#include "port/minwin/user32.h"

struct tagTOOLINFOA {
    UINT cbSize;
    UINT uFlags;
    HWND hwnd;
    UINT_PTR uId;
    RECT rect;
    HINSTANCE hinst;
    LPSTR lpszText;
    LPARAM lParam;
    void *lpReserved;
};
typedef struct tagTOOLINFOA TTTOOLINFOA;

extern "C" {
MINWIN_IMPORT void REB_STDCALL InitCommonControls(void);
}

#define TTS_ALWAYSTIP 1
#define TTF_IDISHWND 1
#define TTM_ACTIVATE 0x401
#define TTM_SETDELAYTIME 0x403
#define TTM_ADDTOOLA 0x404
#define TTM_DELTOOLA 0x405
#define TTM_RELAYEVENT 0x407
#define TTM_UPDATETIPTEXTA 0x40c
