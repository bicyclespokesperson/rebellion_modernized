#pragma once

// Calling-convention keywords of the 32-bit MSVC ABI the original uses (members are __thiscall, free functions __cdecl, Win32
// callbacks __stdcall). They exist only on i386; every other target has one calling convention, so the macros are empty.
// Source outside platform/ must use these instead of the keywords.
#if defined(__i386__) || defined(_M_IX86)
#define REB_THISCALL __thiscall
#define REB_STDCALL __stdcall
#define REB_CDECL __cdecl
#else
#define REB_THISCALL
#define REB_STDCALL
#define REB_CDECL
#endif

// Functions imported from a DLL are called through the import address table on Windows; elsewhere they are ordinary functions.
#if defined(_WIN32)
#define REB_DLLIMPORT __declspec(dllimport)
#else
#define REB_DLLIMPORT
#endif

// Structured exception handling (`__try`/`__finally`), used once, by the sound refill thread, to leave the thread when the
// refill raises an exception. Off Windows there are no structured exceptions: the body just runs and the handler is the
// ordinary cleanup.
#if defined(_WIN32)
#define REB_TRY __try
#define REB_FINALLY __finally
#define REB_ABNORMAL_TERMINATION() AbnormalTermination()
#else
#define REB_TRY
#define REB_FINALLY
#define REB_ABNORMAL_TERMINATION() 0
#endif
