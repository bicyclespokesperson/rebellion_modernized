// Compile-time check of the minwin headers against the real Windows SDK (docs/phase3_plan.md, P0c).
//
// layout_expected.inc lists, as measured on the SDK by tools/minwin_layout_gen.py, the size of every struct minwin declares, the
// offset of every field and the value (and, on Windows targets, the type) of every constant. This translation unit includes only
// minwin and asserts all of it, so a layout or value that drifts from the original fails the build. Sizes and offsets are the
// i686 ones and are checked only there; constant values hold on every target. Built by the standalone build when REB_MINWIN is
// on, and by tools/minwin_check.sh.
#include <stddef.h>

#include "port/minwin/minwin.h"

#if defined(__i386__) || defined(_M_IX86)
#define MINWIN_LAYOUT_SIZE(tag, bytes) static_assert(sizeof(tag) == (bytes), "sizeof(" #tag ") differs from the SDK");
#define MINWIN_LAYOUT_OFFSET(tag, field, bytes) \
    static_assert(offsetof(tag, field) == (bytes), "offsetof(" #tag "," #field ") differs from the SDK");
#else
#define MINWIN_LAYOUT_SIZE(tag, bytes)
#define MINWIN_LAYOUT_OFFSET(tag, field, bytes)
#endif

#if MINWIN_TARGET_WINDOWS
#define MINWIN_LAYOUT_TYPE(name, type) static_assert(__is_same(decltype((name)), type), #name " has another type than in the SDK");
#else
#define MINWIN_LAYOUT_TYPE(name, type)
#endif

#define MINWIN_LAYOUT_VALUE(name, value, type) \
    static_assert(static_cast<long long>(name) == (value), #name " differs from the SDK"); \
    MINWIN_LAYOUT_TYPE(name, type)

#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "port/minwin/layout_expected.inc"
