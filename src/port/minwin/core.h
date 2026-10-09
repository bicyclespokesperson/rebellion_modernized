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
typedef char CHAR;
typedef unsigned char UCHAR;
typedef unsigned char BYTE;
typedef short SHORT;
typedef unsigned short USHORT;
typedef unsigned short WORD;
typedef int INT;
typedef unsigned int UINT;
typedef int BOOL;
typedef float FLOAT;
typedef char* PCHAR;
typedef unsigned char* PBYTE;
typedef int* PINT;
typedef unsigned int* PUINT;
typedef void* PVOID;

#if MINWIN_TARGET_WINDOWS
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
typedef __int64 LONGLONG;
typedef unsigned __int64 ULONGLONG;
#else
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef uint32_t DWORD;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
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
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
#endif
typedef ULONG_PTR DWORD_PTR;
typedef ULONG_PTR SIZE_T;
typedef LONG_PTR SSIZE_T;

#ifndef _WIN32
#define __int64 long long
#endif

#if MINWIN_TARGET_WINDOWS
#define AbnormalTermination() _abnormal_termination()
#endif

#define TRUE 1
#define FALSE 0
