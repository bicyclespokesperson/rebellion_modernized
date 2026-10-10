// Text scenarios with plain Win32 names: font metrics, extents, DrawText layout and rendered pixels of the game's 11 distinct Arial
// fonts over sample strings the game draws. Built against the real gdi32 (Wine) and against minwin_impl/text.
#pragma once
#include <stdio.h>

#include <string>
#include <vector>

#include "../../gdi/tests/gdi_ops.h"

struct TextResult {
    std::vector<Dump> pixels;
    std::vector<std::string> lines;  // "key value" lines: metrics, extents, DrawText rectangles
};

// samplesPath: the file written by gen_samples.py. `filter` selects fonts by name substring ("f14w700").
bool RunTextScenarios(const char* samplesPath, const char* filter, TextResult* out);
