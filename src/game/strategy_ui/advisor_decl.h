#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"

// Function of the player dispatch module that the advisor droid calls.
int REB_CDECL LoadAdvisorNotices(int side, IdValueTree* notices);  // 0x48b460
