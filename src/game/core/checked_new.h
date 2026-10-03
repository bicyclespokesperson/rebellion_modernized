#pragma once
#include <new>

// The game's operator new returns null on failure (it never throws) and callers skip construction in that
// case; `new T` would not check.
template <class T, class... Args>
T* NewOrNull(Args&&... args) {
    void* memory = ::operator new(sizeof(T));
    return memory ? new (memory) T(static_cast<Args&&>(args)...) : nullptr;
}
