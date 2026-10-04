#pragma once
#include <cstdint>

#include "galaxy/system.h"

// The "rules" functions of System (0x50ac80-0x50e820, reconstructed by the sibling sub-agent), declared here by their
// original addresses so that the event/virtual code of this slice can call them before the two branches are merged.
// Ghidra default names resolve to the original addresses at link time. After the merge, replace each FUN_ call by the
// sibling's System:: member and delete this header. All are __thiscall on a System; the unlabelled int arguments are
// plain values (sides, counts), `ctx` is the NotifyContext.

int __thiscall FUN_0050ac80(System* self, NotifyContext* ctx);
int __thiscall FUN_0050ad60(System* self, NotifyContext* ctx);
int __thiscall FUN_0050ad80(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b0b0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b230(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b2c0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b310(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b4d0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b500(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b5a0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b610(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b760(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b800(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b890(System* self, NotifyContext* ctx);
int __thiscall FUN_0050b8e0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050ba90(System* self, NotifyContext* ctx);
int __thiscall FUN_0050bb00(System* self, NotifyContext* ctx);
int __thiscall FUN_0050bb70(System* self, NotifyContext* ctx);
int __thiscall FUN_0050bbb0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050bc60(System* self, NotifyContext* ctx);
int __thiscall FUN_0050be00(System* self, NotifyContext* ctx);
int __thiscall FUN_0050bfe0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050c0b0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050c180(System* self, NotifyContext* ctx);
int __thiscall FUN_0050c1b0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050c1e0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050c210(System* self, NotifyContext* ctx);
int __thiscall FUN_0050c2b0(System* self, NotifyContext* ctx);
int __thiscall FUN_0050c350(System* self, NotifyContext* ctx);
// (system, child object, ctx)
int __thiscall FUN_0050c540(System* self, GameObj* child, NotifyContext* ctx);
int __thiscall FUN_0050c560(System* self, GameObj* child, NotifyContext* ctx);
int __thiscall FUN_0050c580(System* self, GameObj* child, NotifyContext* ctx);
// (system, child object, added/removed flag, ctx)
int __thiscall FUN_0050c6c0(System* self, GameObj* child, int flag, NotifyContext* ctx);
// (system, old control kind, new control kind, ctx)
int __thiscall FUN_0050c820(System* self, uint32_t oldKind, uint32_t newKind, NotifyContext* ctx);
// (system, value, side, ctx)
int __thiscall FUN_0050c9f0(System* self, int value, uint32_t side, NotifyContext* ctx);
// (system, value, side): the three per-side adjustments at 0x50ce20/0x50ce50/0x50cee0 take (0, side) or (n, side)
int __thiscall FUN_0050ce20(System* self, int value, uint32_t side);
int __thiscall FUN_0050ce50(System* self, int value, uint32_t side);
int __thiscall FUN_0050cee0(System* self, int value, uint32_t side);
int __thiscall FUN_0050ce80(System* self, uint32_t side);
int __thiscall FUN_0050ceb0(System* self, int value);
int __thiscall FUN_0050cfd0(System* self, int value);
int __thiscall FUN_0050d000(System* self, int value);
int __thiscall FUN_0050d030(System* self, NotifyContext* ctx);
int __thiscall FUN_0050d510(System* self, NotifyContext* ctx);
