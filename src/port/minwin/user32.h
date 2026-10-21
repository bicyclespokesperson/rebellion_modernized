// minwin: USER32 -- window classes, messages, input, painting, hooks, timers and the window-message/style constants.
// Written from the API surface (names, signatures, layouts, values) of the Windows SDK.
#pragma once
#include "port/minwin/base.h"
#include "port/minwin/gdi32.h"

using WNDPROC = LRESULT(REB_STDCALL*)(HWND, UINT, WPARAM, LPARAM);
using TIMERPROC = void(REB_STDCALL*)(HWND, UINT, UINT_PTR, DWORD);
using HOOKPROC = LRESULT(REB_STDCALL*)(int, WPARAM, LPARAM);
struct tagMOUSEHOOKSTRUCT {
  POINT pt;
  HWND hwnd;
  UINT wHitTestCode;
  ULONG_PTR dwExtraInfo;
};
using MOUSEHOOKSTRUCT = struct tagMOUSEHOOKSTRUCT;
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
using WNDCLASSA = struct tagWNDCLASSA;
using LPWNDCLASSA = struct tagWNDCLASSA*;
struct tagMSG {
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
};
using MSG = struct tagMSG;
using LPMSG = struct tagMSG*;
struct tagPAINTSTRUCT {
  HDC hdc;
  BOOL fErase;
  RECT rcPaint;
  BOOL fRestore;
  BOOL fIncUpdate;
  BYTE rgbReserved[32];
};
using PAINTSTRUCT = struct tagPAINTSTRUCT;
using LPPAINTSTRUCT = struct tagPAINTSTRUCT*;
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
using CREATESTRUCTA = struct tagCREATESTRUCTA;
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
using DRAWITEMSTRUCT = struct tagDRAWITEMSTRUCT;

namespace reb::compat
{
int REB_STDCALL LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax);
int REB_CDECL wsprintfA(LPSTR, LPCSTR, ...);
BOOL REB_STDCALL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
BOOL REB_STDCALL TranslateMessage(const MSG* lpMsg);
LRESULT REB_STDCALL DispatchMessageA(const MSG* lpMsg);
BOOL REB_STDCALL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax,
                                            UINT wRemoveMsg);
DWORD REB_STDCALL GetMessagePos(void);
LRESULT REB_STDCALL SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL REB_STDCALL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT REB_STDCALL DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
void REB_STDCALL PostQuitMessage(int nExitCode);
ATOM REB_STDCALL RegisterClassA(const WNDCLASSA* lpWndClass);
BOOL REB_STDCALL GetClassInfoA(HINSTANCE hInstance, LPCSTR lpClassName, LPWNDCLASSA lpWndClass);
HWND REB_STDCALL CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle,
                                               int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu,
                                               HINSTANCE hInstance, LPVOID lpParam);
BOOL REB_STDCALL IsWindow(HWND hWnd);
BOOL REB_STDCALL IsChild(HWND hWndParent, HWND hWnd);
BOOL REB_STDCALL DestroyWindow(HWND hWnd);
BOOL REB_STDCALL ShowWindow(HWND hWnd, int nCmdShow);
BOOL REB_STDCALL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
BOOL REB_STDCALL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
BOOL REB_STDCALL IsWindowVisible(HWND hWnd);
BOOL REB_STDCALL IsIconic(HWND hWnd);
BOOL REB_STDCALL BringWindowToTop(HWND hWnd);
BOOL REB_STDCALL IsZoomed(HWND hWnd);
HWND REB_STDCALL SetFocus(HWND hWnd);
HWND REB_STDCALL GetActiveWindow(void);
HWND REB_STDCALL GetFocus(void);
SHORT REB_STDCALL GetKeyState(int nVirtKey);
SHORT REB_STDCALL GetAsyncKeyState(int vKey);
HWND REB_STDCALL GetCapture(void);
HWND REB_STDCALL SetCapture(HWND hWnd);
BOOL REB_STDCALL ReleaseCapture(void);
UINT_PTR REB_STDCALL SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);
BOOL REB_STDCALL KillTimer(HWND hWnd, UINT_PTR uIDEvent);
BOOL REB_STDCALL EnableWindow(HWND hWnd, BOOL bEnable);
BOOL REB_STDCALL IsWindowEnabled(HWND hWnd);
HACCEL REB_STDCALL LoadAcceleratorsA(HINSTANCE hInstance, LPCSTR lpTableName);
int REB_STDCALL TranslateAcceleratorA(HWND hWnd, HACCEL hAccTable, LPMSG lpMsg);
int REB_STDCALL GetSystemMetrics(int nIndex);
HMENU REB_STDCALL GetMenu(HWND hWnd);
int REB_STDCALL DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format);
BOOL REB_STDCALL UpdateWindow(HWND hWnd);
HDC REB_STDCALL GetDC(HWND hWnd);
HDC REB_STDCALL GetWindowDC(HWND hWnd);
int REB_STDCALL ReleaseDC(HWND hWnd, HDC hDC);
HDC REB_STDCALL BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint);
BOOL REB_STDCALL EndPaint(HWND hWnd, const PAINTSTRUCT* lpPaint);
BOOL REB_STDCALL GetUpdateRect(HWND hWnd, LPRECT lpRect, BOOL bErase);
BOOL REB_STDCALL InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
BOOL REB_STDCALL SetWindowTextA(HWND hWnd, LPCSTR lpString);
BOOL REB_STDCALL GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL REB_STDCALL GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL REB_STDCALL AdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);
int REB_STDCALL MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
int REB_STDCALL MessageBoxExA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType, WORD wLanguageId);
BOOL REB_STDCALL MessageBeep(UINT uType);
int REB_STDCALL ShowCursor(BOOL bShow);
HCURSOR REB_STDCALL SetCursor(HCURSOR hCursor);
BOOL REB_STDCALL GetCursorPos(LPPOINT lpPoint);
BOOL REB_STDCALL CreateCaret(HWND hWnd, HBITMAP hBitmap, int nWidth, int nHeight);
BOOL REB_STDCALL DestroyCaret(void);
BOOL REB_STDCALL HideCaret(HWND hWnd);
BOOL REB_STDCALL ShowCaret(HWND hWnd);
BOOL REB_STDCALL SetCaretPos(int X, int Y);
BOOL REB_STDCALL GetCaretPos(LPPOINT lpPoint);
BOOL REB_STDCALL ClientToScreen(HWND hWnd, LPPOINT lpPoint);
BOOL REB_STDCALL ScreenToClient(HWND hWnd, LPPOINT lpPoint);
int REB_STDCALL MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints);
HWND REB_STDCALL WindowFromPoint(POINT Point);
HWND REB_STDCALL ChildWindowFromPointEx(HWND hwnd, POINT pt, UINT flags);
int REB_STDCALL FillRect(HDC hDC, const RECT* lprc, HBRUSH hbr);
BOOL REB_STDCALL SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom);
BOOL REB_STDCALL SetRectEmpty(LPRECT lprc);
BOOL REB_STDCALL CopyRect(LPRECT lprcDst, const RECT* lprcSrc);
BOOL REB_STDCALL IntersectRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);
BOOL REB_STDCALL UnionRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);
BOOL REB_STDCALL IsRectEmpty(const RECT* lprc);
BOOL REB_STDCALL PtInRect(const RECT* lprc, POINT pt);
LONG REB_STDCALL GetWindowLongA(HWND hWnd, int nIndex);
LONG REB_STDCALL SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong);
LONG_PTR REB_STDCALL GetWindowLongPtrA(HWND hWnd, int nIndex);
LONG_PTR REB_STDCALL SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
HWND REB_STDCALL GetParent(HWND hWnd);
HWND REB_STDCALL SetParent(HWND hWndChild, HWND hWndNewParent);
int REB_STDCALL GetClassNameA(HWND hWnd, LPSTR lpClassName, int nMaxCount);
HWND REB_STDCALL GetTopWindow(HWND hWnd);
HWND REB_STDCALL GetWindow(HWND hWnd, UINT uCmd);
HHOOK REB_STDCALL SetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId);
BOOL REB_STDCALL UnhookWindowsHookEx(HHOOK hhk);
LRESULT REB_STDCALL CallNextHookEx(HHOOK hhk, int nCode, WPARAM wParam, LPARAM lParam);
HBITMAP REB_STDCALL LoadBitmapA(HINSTANCE hInstance, LPCSTR lpBitmapName);
HCURSOR REB_STDCALL LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
HICON REB_STDCALL LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
BOOL REB_STDCALL WinHelpA(HWND hWndMain, LPCSTR lpszHelp, UINT uCommand, ULONG_PTR dwData);
BOOL REB_STDCALL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);
}

#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEA
#define RT_RCDATA MAKEINTRESOURCE(10)
#define SW_SHOW 5
#define SW_RESTORE 9
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
#define CW_USEDEFAULT ((int)0x80000000)
#define SWP_NOSIZE 1
#define SWP_NOMOVE 2
#define SWP_NOZORDER 4
#define SWP_NOACTIVATE 0x10
#define SWP_SHOWWINDOW 0x40
#define SWP_NOOWNERZORDER 0x200
#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_NOTOPMOST ((HWND) - 2)
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
#define GW_HWNDNEXT 2
#define GW_HWNDPREV 3
#define GW_CHILD 5
#define SC_SIZE 0xf000
#define SC_MOVE 0xf010
#define SC_MINIMIZE 0xf020
#define SC_MAXIMIZE 0xf030
#define SC_RESTORE 0xf120
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define HELP_QUIT ((LONG)2)
#define HELP_FINDER 0xb
#define SPI_GETWORKAREA 0x30
