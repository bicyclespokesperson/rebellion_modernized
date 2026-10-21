// Drives the Smacker API the way the game's SmackerWindow does and records what a caller can observe: the decoded 8-bit
// picture of every frame, the palette, the changed rectangles, the flags the game reads from the Smack struct, and the
// decoded audio, through minwin_impl/smack.
#pragma once
#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

struct SmackApiTable;

// Binds the table to the linked minwin_impl/smack.
bool LoadSmackApi(SmackApiTable* api);

// Plays `path` from frame 0 to the last (or maxFrames) and writes one text line per frame plus a header to `out`. If
// dumpFrame >= 0 the picture and palette of that frame are written to `dumpPath` (raw indices then 768 palette bytes).
bool RunSmackScenario(const SmackApiTable& api, const char* path, int maxFrames, int dumpFrame, const char* dumpPath,
                      FILE* out);
