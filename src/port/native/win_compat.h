// The few Win32 calls the headless simulation harness (src/port/sim_harness.cpp) makes beyond minwin, as thin POSIX wrappers
// (win_compat.cpp). Used instead of <windows.h> by the native build.
#pragma once
#include "platform/win32.h"

#define MAX_PATH 260
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1u
#define FILE_APPEND_DATA 4u
#define CREATE_ALWAYS 2u
#define OPEN_ALWAYS 4u
#define FILE_ATTRIBUTE_NORMAL 0x80u

DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size);
HANDLE CreateFileA(LPCSTR path,
                   DWORD access,
                   DWORD share,
                   void* security,
                   DWORD disposition,
                   DWORD attributes,
                   HANDLE template_);
BOOL WriteFile(HANDLE file, const void* data, DWORD bytes, DWORD* written, void* overlapped);
[[noreturn]] void ExitProcess(UINT code);
