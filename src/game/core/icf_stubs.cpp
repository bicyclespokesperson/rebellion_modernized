#include "core/reb_abi.h"
#include "core/icf_stubs.h"

int ReturnOne() { return 1; }

int16_t ReturnOneShort() { return 1; }

void REB_THISCALL IgnoreOneArgument(void*, uintptr_t) {}
