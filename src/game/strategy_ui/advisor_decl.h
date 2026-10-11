#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"

// Function of the player dispatch module that the advisor droid calls.
int LoadAdvisorNotices(int side, IdValueTree* notices); // 0x48b460
