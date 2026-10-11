#pragma once

// Calling-convention keywords of the 32-bit MSVC ABI used by platform APIs.
#if defined(__i386__) || defined(_M_IX86)
#define REB_THISCALL __thiscall
#define REB_STDCALL __stdcall
#define REB_CDECL __cdecl
#else
#define REB_THISCALL
#define REB_STDCALL
#define REB_CDECL
#endif

// Imported DLL functions use the Windows import address table.
#if defined(_WIN32)
#define REB_DLLIMPORT __declspec(dllimport)
#else
#define REB_DLLIMPORT
#endif
