#pragma once
#include <new>
#include <utility>

// Callers that can recover from allocation failure receive nullptr. Constructor
// failures still propagate, and the new expression releases its allocation.
template <class T, class... Args>
T* NewOrNull(Args&&... args) {
    return new (std::nothrow) T(std::forward<Args>(args)...);
}
