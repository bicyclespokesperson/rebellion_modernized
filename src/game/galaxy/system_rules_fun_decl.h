#pragma once
#include <cstdint>

#include "galaxy/system.h"

// The "rules" functions of System (0x50ac80-0x50e820, reconstructed by the sibling sub-agent), declared here by their
// original addresses so that the event/virtual code of this slice can call them before the two branches are merged.
// Ghidra default names resolve to the original addresses at link time. After the merge, replace each FUN_ call by the
// sibling's System:: member and delete this header. All are __thiscall on a System; the unlabelled int arguments are
// plain values (sides, counts), `ctx` is the NotifyContext.
