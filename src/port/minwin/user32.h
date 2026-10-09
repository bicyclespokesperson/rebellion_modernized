// minwin: USER32 -- window classes, messages, input, painting, hooks, timers and the window-message/style constants.
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/base.h"
#include "port/minwin/gdi32.h"

typedef LRESULT (REB_STDCALL *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void (REB_STDCALL *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
typedef LRESULT (REB_STDCALL *HOOKPROC)(int, WPARAM, LPARAM);
struct tagMOUSEHOOKSTRUCT {
    POINT pt;
    HWND hwnd;
    UINT wHitTestCode;
    ULONG_PTR dwExtraInfo;
};
typedef struct tagMOUSEHOOKSTRUCT MOUSEHOOKSTRUCT;
struct tagWNDCLASSA {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
};
typedef struct tagWNDCLASSA WNDCLASSA;
typedef struct tagWNDCLASSA *LPWNDCLASSA;
struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
};
typedef struct tagMSG MSG;
typedef struct tagMSG *LPMSG;
struct tagPAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
};
typedef struct tagPAINTSTRUCT PAINTSTRUCT;
typedef struct tagPAINTSTRUCT *LPPAINTSTRUCT;
struct tagCREATESTRUCTA {
    LPVOID lpCreateParams;
    HINSTANCE hInstance;
    HMENU hMenu;
    HWND hwndParent;
    int cy;
    int cx;
    int y;
    int x;
    LONG style;
    LPCSTR lpszName;
    LPCSTR lpszClass;
    DWORD dwExStyle;
};
typedef struct tagCREATESTRUCTA CREATESTRUCTA;
struct tagDRAWITEMSTRUCT {
    UINT CtlType;
    UINT CtlID;
    UINT itemID;
    UINT itemAction;
    UINT itemState;
    HWND hwndItem;
    HDC hDC;
    RECT rcItem;
    ULONG_PTR itemData;
};
typedef struct tagDRAWITEMSTRUCT DRAWITEMSTRUCT;

extern "C" {
MINWIN_IMPORT int REB_STDCALL LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax);
MINWIN_IMPORT int REB_CDECL wsprintfA(LPSTR, LPCSTR, ...);
MINWIN_IMPORT BOOL REB_STDCALL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
MINWIN_IMPORT BOOL REB_STDCALL TranslateMessage(const MSG *lpMsg);
MINWIN_IMPORT LRESULT REB_STDCALL DispatchMessageA(const MSG *lpMsg);
MINWIN_IMPORT BOOL REB_STDCALL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
MINWIN_IMPORT DWORD REB_STDCALL GetMessagePos(void);
MINWIN_IMPORT LRESULT REB_STDCALL SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
MINWIN_IMPORT BOOL REB_STDCALL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
MINWIN_IMPORT LRESULT REB_STDCALL DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
MINWIN_IMPORT void REB_STDCALL PostQuitMessage(int nExitCode);
MINWIN_IMPORT ATOM REB_STDCALL RegisterClassA(const WNDCLASSA *lpWndClass);
MINWIN_IMPORT BOOL REB_STDCALL GetClassInfoA(HINSTANCE hInstance, LPCSTR lpClassName, LPWNDCLASSA lpWndClass);
MINWIN_IMPORT HWND REB_STDCALL CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
MINWIN_IMPORT BOOL REB_STDCALL IsWindow(HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL IsChild(HWND hWndParent, HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL DestroyWindow(HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL ShowWindow(HWND hWnd, int nCmdShow);
MINWIN_IMPORT BOOL REB_STDCALL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
MINWIN_IMPORT BOOL REB_STDCALL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
MINWIN_IMPORT BOOL REB_STDCALL IsWindowVisible(HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL IsIconic(HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL BringWindowToTop(HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL IsZoomed(HWND hWnd);
MINWIN_IMPORT HWND REB_STDCALL SetFocus(HWND hWnd);
MINWIN_IMPORT HWND REB_STDCALL GetActiveWindow(void);
MINWIN_IMPORT HWND REB_STDCALL GetFocus(void);
MINWIN_IMPORT SHORT REB_STDCALL GetKeyState(int nVirtKey);
MINWIN_IMPORT SHORT REB_STDCALL GetAsyncKeyState(int vKey);
MINWIN_IMPORT HWND REB_STDCALL GetCapture(void);
MINWIN_IMPORT HWND REB_STDCALL SetCapture(HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL ReleaseCapture(void);
MINWIN_IMPORT UINT_PTR REB_STDCALL SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);
MINWIN_IMPORT BOOL REB_STDCALL KillTimer(HWND hWnd, UINT_PTR uIDEvent);
MINWIN_IMPORT BOOL REB_STDCALL EnableWindow(HWND hWnd, BOOL bEnable);
MINWIN_IMPORT BOOL REB_STDCALL IsWindowEnabled(HWND hWnd);
MINWIN_IMPORT HACCEL REB_STDCALL LoadAcceleratorsA(HINSTANCE hInstance, LPCSTR lpTableName);
MINWIN_IMPORT int REB_STDCALL TranslateAcceleratorA(HWND hWnd, HACCEL hAccTable, LPMSG lpMsg);
MINWIN_IMPORT int REB_STDCALL GetSystemMetrics(int nIndex);
MINWIN_IMPORT HMENU REB_STDCALL GetMenu(HWND hWnd);
MINWIN_IMPORT int REB_STDCALL DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format);
MINWIN_IMPORT BOOL REB_STDCALL UpdateWindow(HWND hWnd);
MINWIN_IMPORT HDC REB_STDCALL GetDC(HWND hWnd);
MINWIN_IMPORT HDC REB_STDCALL GetWindowDC(HWND hWnd);
MINWIN_IMPORT int REB_STDCALL ReleaseDC(HWND hWnd, HDC hDC);
MINWIN_IMPORT HDC REB_STDCALL BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint);
MINWIN_IMPORT BOOL REB_STDCALL EndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint);
MINWIN_IMPORT BOOL REB_STDCALL GetUpdateRect(HWND hWnd, LPRECT lpRect, BOOL bErase);
MINWIN_IMPORT BOOL REB_STDCALL InvalidateRect(HWND hWnd, const RECT *lpRect, BOOL bErase);
MINWIN_IMPORT BOOL REB_STDCALL SetWindowTextA(HWND hWnd, LPCSTR lpString);
MINWIN_IMPORT BOOL REB_STDCALL GetClientRect(HWND hWnd, LPRECT lpRect);
MINWIN_IMPORT BOOL REB_STDCALL GetWindowRect(HWND hWnd, LPRECT lpRect);
MINWIN_IMPORT BOOL REB_STDCALL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);
MINWIN_IMPORT int REB_STDCALL MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
MINWIN_IMPORT int REB_STDCALL MessageBoxExA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType, WORD wLanguageId);
MINWIN_IMPORT BOOL REB_STDCALL MessageBeep(UINT uType);
MINWIN_IMPORT int REB_STDCALL ShowCursor(BOOL bShow);
MINWIN_IMPORT HCURSOR REB_STDCALL SetCursor(HCURSOR hCursor);
MINWIN_IMPORT BOOL REB_STDCALL GetCursorPos(LPPOINT lpPoint);
MINWIN_IMPORT BOOL REB_STDCALL CreateCaret(HWND hWnd, HBITMAP hBitmap, int nWidth, int nHeight);
MINWIN_IMPORT BOOL REB_STDCALL DestroyCaret(void);
MINWIN_IMPORT BOOL REB_STDCALL HideCaret(HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL ShowCaret(HWND hWnd);
MINWIN_IMPORT BOOL REB_STDCALL SetCaretPos(int X, int Y);
MINWIN_IMPORT BOOL REB_STDCALL GetCaretPos(LPPOINT lpPoint);
MINWIN_IMPORT BOOL REB_STDCALL ClientToScreen(HWND hWnd, LPPOINT lpPoint);
MINWIN_IMPORT BOOL REB_STDCALL ScreenToClient(HWND hWnd, LPPOINT lpPoint);
MINWIN_IMPORT int REB_STDCALL MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints);
MINWIN_IMPORT HWND REB_STDCALL WindowFromPoint(POINT Point);
MINWIN_IMPORT HWND REB_STDCALL ChildWindowFromPointEx(HWND hwnd, POINT pt, UINT flags);
MINWIN_IMPORT int REB_STDCALL FillRect(HDC hDC, const RECT *lprc, HBRUSH hbr);
MINWIN_IMPORT BOOL REB_STDCALL SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom);
MINWIN_IMPORT BOOL REB_STDCALL SetRectEmpty(LPRECT lprc);
MINWIN_IMPORT BOOL REB_STDCALL CopyRect(LPRECT lprcDst, const RECT *lprcSrc);
MINWIN_IMPORT BOOL REB_STDCALL IntersectRect(LPRECT lprcDst, const RECT *lprcSrc1, const RECT *lprcSrc2);
MINWIN_IMPORT BOOL REB_STDCALL UnionRect(LPRECT lprcDst, const RECT *lprcSrc1, const RECT *lprcSrc2);
MINWIN_IMPORT BOOL REB_STDCALL IsRectEmpty(const RECT *lprc);
MINWIN_IMPORT BOOL REB_STDCALL PtInRect(const RECT *lprc, POINT pt);
MINWIN_IMPORT LONG REB_STDCALL GetWindowLongA(HWND hWnd, int nIndex);
MINWIN_IMPORT LONG REB_STDCALL SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong);
#if MINWIN_TARGET_WINDOWS && !defined(_WIN64)
// 32-bit Windows has no *Ptr entry points: the SDK maps them to the 32-bit ones.
#define GetWindowLongPtrA GetWindowLongA
#define SetWindowLongPtrA SetWindowLongA
#else
MINWIN_IMPORT LONG_PTR REB_STDCALL GetWindowLongPtrA(HWND hWnd, int nIndex);
MINWIN_IMPORT LONG_PTR REB_STDCALL SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
#endif
MINWIN_IMPORT HWND REB_STDCALL GetParent(HWND hWnd);
MINWIN_IMPORT HWND REB_STDCALL SetParent(HWND hWndChild, HWND hWndNewParent);
MINWIN_IMPORT int REB_STDCALL GetClassNameA(HWND hWnd, LPSTR lpClassName, int nMaxCount);
MINWIN_IMPORT HWND REB_STDCALL GetTopWindow(HWND hWnd);
MINWIN_IMPORT HWND REB_STDCALL GetWindow(HWND hWnd, UINT uCmd);
MINWIN_IMPORT HHOOK REB_STDCALL SetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId);
MINWIN_IMPORT BOOL REB_STDCALL UnhookWindowsHookEx(HHOOK hhk);
MINWIN_IMPORT LRESULT REB_STDCALL CallNextHookEx(HHOOK hhk, int nCode, WPARAM wParam, LPARAM lParam);
MINWIN_IMPORT HBITMAP REB_STDCALL LoadBitmapA(HINSTANCE hInstance, LPCSTR lpBitmapName);
MINWIN_IMPORT HCURSOR REB_STDCALL LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
MINWIN_IMPORT HICON REB_STDCALL LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
MINWIN_IMPORT BOOL REB_STDCALL WinHelpA(HWND hWndMain, LPCSTR lpszHelp, UINT uCommand, ULONG_PTR dwData);
MINWIN_IMPORT BOOL REB_STDCALL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);
}

#if defined(REB_MSG_TRACE_SHIM)
// src/standalone/msg_trace_shim.cpp: registers a logging wrapper around each window procedure (tools/msg_trace_compare.py).
extern "C" ATOM REB_STDCALL RebTracedRegisterClassA(const WNDCLASSA *lpWndClass);
#define RegisterClassA RebTracedRegisterClassA
#endif

#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEA
#define RT_RCDATA MAKEINTRESOURCE(10)
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define VK_LBUTTON 1
#define VK_BACK 8
#define VK_TAB 9
#define VK_RETURN 0xd
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_ESCAPE 0x1b
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2d
#define VK_DELETE 0x2e
#define VK_ADD 0x6b
#define VK_SUBTRACT 0x6d
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define WH_KEYBOARD 2
#define WH_GETMESSAGE 3
#define WH_MOUSE 7
#define HC_NOREMOVE 3
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define GWL_STYLE -16
#define GWL_EXSTYLE -20
#define WM_CREATE 1
#define WM_DESTROY 2
#define WM_MOVE 3
#define WM_SIZE 5
#define WM_ACTIVATE 6
#define WM_SETFOCUS 7
#define WM_KILLFOCUS 8
#define WM_PAINT 0xf
#define WM_CLOSE 0x10
#define WM_QUIT 0x12
#define WM_ERASEBKGND 0x14
#define WM_SHOWWINDOW 0x18
#define WM_ACTIVATEAPP 0x1c
#define WM_MOUSEACTIVATE 0x21
#define WM_DRAWITEM 0x2b
#define WM_NCCREATE 0x81
#define WM_NCDESTROY 0x82
#define WM_NCLBUTTONDBLCLK 0xa3
#define WM_KEYDOWN 0x100
#define WM_KEYUP 0x101
#define WM_CHAR 0x102
#define WM_SYSKEYDOWN 0x104
#define WM_SYSKEYUP 0x105
#define WM_COMMAND 0x111
#define WM_SYSCOMMAND 0x112
#define WM_TIMER 0x113
#define WM_CTLCOLORBTN 0x135
#define WM_MOUSEMOVE 0x200
#define WM_LBUTTONDOWN 0x201
#define WM_LBUTTONUP 0x202
#define WM_LBUTTONDBLCLK 0x203
#define WM_RBUTTONDOWN 0x204
#define WM_RBUTTONUP 0x205
#define WM_CAPTURECHANGED 0x215
#define WM_MDICREATE 0x220
#define WM_QUERYNEWPALETTE 0x30f
#define WM_PALETTECHANGED 0x311
#define WM_USER 0x400
#define HTCAPTION 2
#define MA_ACTIVATE 1
#define MK_LBUTTON 1
#define MK_SHIFT 4
#define MK_CONTROL 8
#define WS_POPUP ((DWORD)0x80000000)
#define WS_CHILD ((LONG)0x40000000)
#define WS_VISIBLE ((LONG)0x10000000)
#define WS_CLIPSIBLINGS ((LONG)0x4000000)
#define WS_CLIPCHILDREN ((LONG)0x2000000)
#define WS_BORDER ((LONG)0x800000)
#define WS_DLGFRAME ((LONG)0x400000)
#define WS_TABSTOP ((LONG)0x10000)
#define WS_EX_TOPMOST ((LONG)8)
#define CS_VREDRAW 1
#define CS_HREDRAW 2
#define CS_DBLCLKS 8
#define PM_NOREMOVE 0
#define PM_REMOVE 1
#define SendMessage SendMessageA
#define CW_USEDEFAULT ((int)0x80000000)
#define SWP_NOSIZE 1
#define SWP_NOMOVE 2
#define SWP_NOZORDER 4
#define SWP_NOACTIVATE 0x10
#define SWP_SHOWWINDOW 0x40
#define SWP_NOOWNERZORDER 0x200
#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_NOTOPMOST ((HWND)-2)
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CYCAPTION 4
#define SM_CXBORDER 5
#define SM_CYBORDER 6
#define SM_CXDLGFRAME 7
#define SM_CYDLGFRAME 8
#define SM_CXFULLSCREEN 0x10
#define SM_CYFULLSCREEN 0x11
#define SM_MOUSEPRESENT 0x13
#define DT_LEFT 0
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_BOTTOM 8
#define DT_WORDBREAK 0x10
#define DT_SINGLELINE 0x20
#define DT_NOCLIP 0x100
#define DT_CALCRECT 0x400
#define DT_NOPREFIX 0x800
#define DT_EDITCONTROL 0x2000
#define MB_ICONHAND ((LONG)0x10)
#define MB_SYSTEMMODAL ((LONG)0x1000)
#define MB_SETFOREGROUND ((LONG)0x10000)
#define MB_TOPMOST ((LONG)0x40000)
#define GetWindowLongPtr GetWindowLongPtrA
#define SetWindowLongPtr SetWindowLongPtrA
#define GW_HWNDNEXT 2
#define GW_HWNDPREV 3
#define GW_CHILD 5
#define SC_SIZE 0xf000
#define SC_MOVE 0xf010
#define SC_MINIMIZE 0xf020
#define SC_MAXIMIZE 0xf030
#define SC_RESTORE 0xf120
#define LoadBitmap LoadBitmapA
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define HELP_QUIT ((LONG)2)
#define HELP_FINDER 0xb
#define SPI_GETWORKAREA 0x30
