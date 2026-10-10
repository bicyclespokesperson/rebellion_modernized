// Operation sequences on 8-bit DIB sections, written with plain Win32 names. The same source is built against the real gdi32 (run
// under Wine to produce the reference) and against minwin_impl/gdi (the implementation under test); the dumps are diffed.
#pragma once
#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

struct Dump {
    std::string name;
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;  // top-down, one palette index per pixel
};

// Runs every scenario (or only those whose name contains `filter`), appending results to `out`.
void RunGdiScenarios(const char* filter, std::vector<Dump>* out);
void WriteDumps(const std::vector<Dump>& dumps, FILE* f);
bool ReadDumps(FILE* f, std::vector<Dump>* dumps);
