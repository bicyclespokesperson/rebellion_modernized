// Drives the Smacker API the way the game's SmackerWindow does and records what a caller can observe: the decoded 8-bit picture of
// every frame, the palette, the changed rectangles, the flags the game reads from the Smack struct, and the decoded audio.
// Built twice: against the real SMACKW32.DLL (loaded by name under Wine) and against minwin_impl/smack.
#pragma once
#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

struct SmackApiTable;

// "real": load SMACKW32.DLL from `dllPath` (Windows only); otherwise use the linked implementation.
bool LoadSmackApi(SmackApiTable* api, const char* dllPath);

// Plays `path` from frame 0 to the last (or maxFrames) and writes one text line per frame plus a header to `out`. If dumpFrame >= 0 the
// picture and palette of that frame are written to `dumpPath` (raw indices then 768 palette bytes).
bool RunSmackScenario(const SmackApiTable& api, const char* path, int maxFrames, int dumpFrame, const char* dumpPath, FILE* out);
