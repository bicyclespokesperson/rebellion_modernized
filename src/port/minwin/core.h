// minwin: the fixed-width scalar types and the keyword macros every other minwin header builds on.
//
// minwin declares the subset of Win32 / COM / DirectX / WinMM / VfW that the reconstructed game uses (inventory:
// tools/minwin_inventory.txt). Where the target is Windows the types are spelled the way the Windows SDK spells them
// (`long`-based DWORD/LONG), so that mangled C++ names, overload resolution and generated code are the same as with the real
// headers; everywhere else they are explicit fixed widths, so that ILP32 and LP64 targets agree on every struct layout.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "core/reb_abi.h"

#if defined(_WIN32)
#define MINWIN_TARGET_WINDOWS 1
#else
#define MINWIN_TARGET_WINDOWS 0
#endif

// Calling conventions. WINAPI/CALLBACK are stdcall on 32-bit Windows (REB_STDCALL is empty elsewhere).
#define WINAPI REB_STDCALL
#define WINAPIV REB_CDECL
#define CALLBACK REB_STDCALL
#define APIENTRY REB_STDCALL
#define PASCAL REB_STDCALL
#define STDMETHODCALLTYPE REB_STDCALL
#define STDAPICALLTYPE REB_STDCALL

// Functions the game imports are always called through the import address table on Windows. The native implementation of
// minwin (compiled with MINWIN_IMPLEMENTATION) defines them instead.
#if defined(MINWIN_IMPLEMENTATION)
#define MINWIN_IMPORT
#else
#define MINWIN_IMPORT REB_DLLIMPORT
#endif

// Legacy qualifiers that old headers decorate prototypes with; they mean nothing on a flat 32/64-bit address space.
#define FAR
#define NEAR
#define CONST const
#define VOID void
#define IN
#define OUT
#define OPTIONAL
#define EXTERN_C extern "C"

// Scalar types.
using CHAR = char;
using UCHAR = unsigned char;
using BYTE = unsigned char;
using SHORT = short;
using USHORT = unsigned short;
using WORD = unsigned short;
using INT = int;
using UINT = unsigned int;
using BOOL = int;
using FLOAT = float;
using PCHAR = char*;
using PBYTE = unsigned char*;
using PINT = int*;
using PUINT = unsigned int*;
using PVOID = void*;

#if MINWIN_TARGET_WINDOWS
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
typedef __int64 LONGLONG;
typedef unsigned __int64 ULONGLONG;
#else
using LONG = int32_t;
using ULONG = uint32_t;
using DWORD = uint32_t;
using LONGLONG = int64_t;
using ULONGLONG = uint64_t;
#endif

// Pointer-sized integers: the 32-bit Windows spelling is `long`/`unsigned long` (INT_PTR is `int`).
#if defined(_WIN64)
typedef __int64 INT_PTR;
typedef unsigned __int64 UINT_PTR;
typedef __int64 LONG_PTR;
typedef unsigned __int64 ULONG_PTR;
#elif MINWIN_TARGET_WINDOWS
typedef int INT_PTR;
typedef unsigned int UINT_PTR;
typedef long LONG_PTR;
typedef unsigned long ULONG_PTR;
#else
using INT_PTR = intptr_t;
using UINT_PTR = uintptr_t;
using LONG_PTR = intptr_t;
using ULONG_PTR = uintptr_t;
#endif
using DWORD_PTR = ULONG_PTR;
using SIZE_T = ULONG_PTR;
using SSIZE_T = LONG_PTR;

#ifndef _WIN32
#define __int64 long long
#endif

#if MINWIN_TARGET_WINDOWS
#define AbnormalTermination() _abnormal_termination()
#endif

#define TRUE 1
#define FALSE 0
