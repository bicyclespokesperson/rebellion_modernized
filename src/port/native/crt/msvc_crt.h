// MSVC CRT names the game calls (non-standard functions of <string.h>, <direct.h>, <io.h>) on top of POSIX.
#pragma once
#include <strings.h>
#include <string.h>
#include <stddef.h>

static inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
static inline int _strnicmp(const char* a, const char* b, size_t n) { return strncasecmp(a, b, n); }
