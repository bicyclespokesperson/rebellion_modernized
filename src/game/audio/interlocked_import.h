#pragma once
#include "platform/win32.h"

// The original calls kernel32's InterlockedExchange through its import table; the SDK headers would replace the call
// with an inline instruction, so the import is declared again here.
#ifdef InterlockedExchange
#undef InterlockedExchange
#endif
extern "C" LONG InterlockedExchange(LONG volatile* target, LONG value);
