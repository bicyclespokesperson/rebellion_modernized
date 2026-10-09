#pragma once

// clang deletes `atexit(f)` when f is provably empty (an atexit thunk of an object with a trivial destructor), but the
// original registers the thunk and the registration is visible in the CRT's onexit table. A barrier keeps the body.
#define REB_KEEP_ATEXIT_TARGET() __asm__ volatile("" : : : "memory")
