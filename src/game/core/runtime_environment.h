#pragma once
#include <cstdio>
#include <cstdint>

int64_t GameSeedSeconds();
FILE* OpenGameFile(const char* name, const char* mode);
