// Allocation and harness hooks of the native executable (the counterpart of src/standalone and src/port/heap_probe.cpp).
//
// Transitional zero-filling allocation: retain it until the A1 member-initialization audit is complete.
// Allocation failure follows the ordinary throwing-new contract.
#include <stddef.h>
#include <new>
#include <stdlib.h>

namespace {
void* AllocateZeroed(size_t size) {
    if (void* memory = calloc(size ? size : 1, 1)) return memory;
    throw std::bad_alloc();
}
}

void* operator new(size_t size) { return AllocateZeroed(size); }
void* operator new[](size_t size) { return AllocateZeroed(size); }
void operator delete(void* memory) noexcept { free(memory); }
void operator delete[](void* memory) noexcept { free(memory); }
void operator delete(void* memory, size_t) noexcept { free(memory); }
void operator delete[](void* memory, size_t) noexcept { free(memory); }

int RebHeapProbeMode() { return 0; }

void* operator new(size_t size, const std::nothrow_t&) noexcept { return calloc(size ? size : 1, 1); }
void* operator new[](size_t size, const std::nothrow_t&) noexcept { return calloc(size ? size : 1, 1); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { free(memory); }
