// minwin: KERNEL32 -- threads, events, critical sections, file and resource access, timing.
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/base.h"

struct _SECURITY_ATTRIBUTES;
typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR cFileName[260];
    CHAR cAlternateFileName[14];
};
typedef struct _WIN32_FIND_DATAA WIN32_FIND_DATAA;
typedef struct _WIN32_FIND_DATAA *LPWIN32_FIND_DATAA;
typedef RTL_CRITICAL_SECTION CRITICAL_SECTION;
typedef PRTL_CRITICAL_SECTION LPCRITICAL_SECTION;
typedef DWORD (REB_STDCALL *PTHREAD_START_ROUTINE)(LPVOID);
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

extern "C" {
MINWIN_IMPORT BOOL REB_STDCALL DeleteFileA(LPCSTR lpFileName);
MINWIN_IMPORT BOOL REB_STDCALL FindClose(HANDLE hFindFile);
MINWIN_IMPORT HANDLE REB_STDCALL FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
MINWIN_IMPORT UINT REB_STDCALL GetDriveTypeA(LPCSTR lpRootPathName);
MINWIN_IMPORT void REB_STDCALL OutputDebugStringA(LPCSTR lpOutputString);
MINWIN_IMPORT BOOL REB_STDCALL CloseHandle(HANDLE hObject);
MINWIN_IMPORT DWORD REB_STDCALL GetLastError(void);
MINWIN_IMPORT BOOL REB_STDCALL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
MINWIN_IMPORT BOOL REB_STDCALL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
MINWIN_IMPORT void REB_STDCALL InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
MINWIN_IMPORT void REB_STDCALL EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
MINWIN_IMPORT void REB_STDCALL LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
MINWIN_IMPORT void REB_STDCALL DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
MINWIN_IMPORT BOOL REB_STDCALL SetEvent(HANDLE hEvent);
MINWIN_IMPORT DWORD REB_STDCALL WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
MINWIN_IMPORT DWORD REB_STDCALL SleepEx(DWORD dwMilliseconds, BOOL bAlertable);
MINWIN_IMPORT HANDLE REB_STDCALL CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
MINWIN_IMPORT void REB_STDCALL Sleep(DWORD dwMilliseconds);
MINWIN_IMPORT DWORD REB_STDCALL WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds);
MINWIN_IMPORT HANDLE REB_STDCALL CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);
MINWIN_IMPORT DWORD REB_STDCALL GetCurrentThreadId(void);
MINWIN_IMPORT BOOL REB_STDCALL SetThreadPriority(HANDLE hThread, int nPriority);
[[noreturn]] MINWIN_IMPORT void REB_STDCALL ExitThread(DWORD dwExitCode);
MINWIN_IMPORT BOOL REB_STDCALL TerminateThread(HANDLE hThread, DWORD dwExitCode);
MINWIN_IMPORT DWORD REB_STDCALL GetTickCount(void);
MINWIN_IMPORT BOOL REB_STDCALL FreeLibrary(HMODULE hLibModule);
MINWIN_IMPORT BOOL REB_STDCALL FreeResource(HGLOBAL hResData);
MINWIN_IMPORT HMODULE REB_STDCALL GetModuleHandleA(LPCSTR lpModuleName);
MINWIN_IMPORT HGLOBAL REB_STDCALL LoadResource(HMODULE hModule, HRSRC hResInfo);
MINWIN_IMPORT LPVOID REB_STDCALL LockResource(HGLOBAL hResData);
MINWIN_IMPORT HMODULE REB_STDCALL LoadLibraryA(LPCSTR lpLibFileName);
MINWIN_IMPORT HFILE REB_STDCALL _lopen(LPCSTR lpPathName, int iReadWrite);
MINWIN_IMPORT UINT REB_STDCALL _lread(HFILE hFile, LPVOID lpBuffer, UINT uBytes);
MINWIN_IMPORT HFILE REB_STDCALL _lclose(HFILE hFile);
MINWIN_IMPORT HANDLE REB_STDCALL CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
MINWIN_IMPORT HRSRC REB_STDCALL FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
MINWIN_IMPORT BOOL REB_STDCALL GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize);
MINWIN_IMPORT DWORD REB_STDCALL VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD cchLang);
}

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define WAIT_FAILED ((DWORD)0xffffffff)
#define WAIT_OBJECT_0 ((DWORD)0)
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define DRIVE_FIXED 3
#define INFINITE 0xffffffffu
#define OF_SHARE_DENY_WRITE 0x20
