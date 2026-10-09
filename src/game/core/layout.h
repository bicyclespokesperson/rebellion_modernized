// Layout checks against the original 32-bit binary. Only meaningful (and only enforced) on 32-bit builds;
// the 64-bit port has different pointer sizes and must not depend on these layouts.
#pragma once
#include <cstddef>

#define REB_LAYOUT_CAT2(a, b) a##b
#define REB_LAYOUT_CAT(a, b) REB_LAYOUT_CAT2(a, b)

#if defined(_M_IX86) || defined(__i386__)
#define REB_ASSERT_SIZE(type, size) static_assert(sizeof(type) == (size), #type " size differs from the original")
#define REB_ASSERT_OFFSET(type, member, offset) \
    static_assert(offsetof(type, member) == (offset), #type "::" #member " offset differs from the original")
#else
#define REB_ASSERT_SIZE(type, size) static_assert(true)
#define REB_ASSERT_OFFSET(type, member, offset) static_assert(true)
#endif

// Storage for an object whose class is only known by its 32-bit size: a pointer-bearing class is at most twice as large on a 64-bit
// target (every 4-byte field becomes at most 8 bytes). Use with a static_assert on the class where it is constructed.
#define REB_LP64_SCALED(bytes32) ((bytes32) * (sizeof(void*) / 4))
