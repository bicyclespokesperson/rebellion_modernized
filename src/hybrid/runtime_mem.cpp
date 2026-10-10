// memcpy/memset/strlen for the CRT-less runtime (the compiler emits calls to them for copies/zeroing).
// Compiled with -fno-builtin so these are not turned back into calls to themselves.
#include <cstddef>

extern "C" void* memcpy(void* dst, const void* src, size_t n) {
    auto* d = static_cast<unsigned char*>(dst);
    const auto* s = static_cast<const unsigned char*>(src);
    while (n--) *d++ = *s++;
    return dst;
}

extern "C" void* memset(void* dst, int value, size_t n) {
    auto* d = static_cast<unsigned char*>(dst);
    while (n--) *d++ = static_cast<unsigned char>(value);
    return dst;
}

extern "C" size_t strlen(const char* s) {
    const char* p = s;
    while (*p) ++p;
    return static_cast<size_t>(p - s);
}
