// Force-included into every game translation unit of the native build (cmake/native.cmake).
//
// The game's own versions of a few C library functions (src/game/crt: rand/srand, qsort, atoi/atol: their results are game
// behaviour) are named reb_* there and the game's calls are redirected here, after the libc and libstdc++ headers that must
// keep the real names have been read. The MSVC-only CRT functions the game calls come from port/native/crt/msvc_crt.h.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>

#include "msvc_crt.h"

extern "C" {
int reb_rand(void);
void reb_srand(unsigned int seed);
void reb_qsort(void* base, size_t count, size_t size, int (*compare)(const void*, const void*));
int reb_atoi(const char* text);
FILE* reb_fopen(const char* name, const char* mode);
long reb_time(long* result);
long reb_atol(const char* text);
}

#define rand reb_rand
#define srand reb_srand
#define qsort reb_qsort
#define fopen reb_fopen
#define time(result) reb_time(result)
#define atoi reb_atoi
#define atol reb_atol

namespace std {
using ::reb_atoi;
using ::reb_atol;
using ::reb_qsort;
using ::reb_rand;
using ::reb_srand;
}  // namespace std
