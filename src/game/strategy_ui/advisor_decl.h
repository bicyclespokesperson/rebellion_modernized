#pragma once
#include <cstdint>
#include <map>

// Function of the player dispatch module that the advisor droid calls.
int LoadAdvisorNotices(int side, std::map<uint32_t, int>* notices); // 0x48b460
