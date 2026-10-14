// MSVC CRT names the game calls (non-standard functions of <string.h>, <direct.h>, <io.h>) on POSIX hosts.
#pragma once
#include <stddef.h>
#include <string.h>
#if !defined(_WIN32)
#include <strings.h>

static inline int _stricmp(const char* a, const char* b)
{
  return strcasecmp(a, b);
}
static inline int _strnicmp(const char* a, const char* b, size_t n)
{
  return strncasecmp(a, b, n);
}
#endif
