#pragma once
#include <cstdint>
#include <cstdio>

int64_t GameSeedSeconds();
FILE* OpenGameFile(const char* name, const char* mode);
