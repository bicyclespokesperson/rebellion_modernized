// Allocation and harness hooks of the native executable (the counterpart of src/standalone and src/port/heap_probe.cpp).
//
// operator new returns zeroed memory, as in the standalone build: the original's heap handed out whatever the previous use left,
// and the reconstruction relies on neither (game state is initialised before use), but zero keeps runs reproducible. It returns
// null on failure, as the original's does (the game checks).
#include <stddef.h>
#include <new>
#include <stdlib.h>

void* operator new(size_t size) { return calloc(size ? size : 1, 1); }
void* operator new[](size_t size) { return calloc(size ? size : 1, 1); }
void operator delete(void* memory) noexcept { free(memory); }
void operator delete[](void* memory) noexcept { free(memory); }
void operator delete(void* memory, size_t) noexcept { free(memory); }
void operator delete[](void* memory, size_t) noexcept { free(memory); }

int RebHeapProbeMode() { return 0; }

void* operator new(size_t size, const std::nothrow_t&) noexcept { return calloc(size ? size : 1, 1); }
void* operator new[](size_t size, const std::nothrow_t&) noexcept { return calloc(size ? size : 1, 1); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { free(memory); }
