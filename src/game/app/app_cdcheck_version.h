#pragma once
#include "platform/win32.h"

// Start-up check of the game CD and of the version of the text DLL (0x406600-0x406950).
//
// The texts of the error boxes are kept in "data0.reb" on the CD: a file of five quoted strings, XOR-ed with 0xdd
// unless it starts with '#'. Without that file the built-in English texts are used.

void InitCdAndVersionCheck(); // 0x406600: loads the error texts and checks textstra.dll's version
int GetVersionCheckResult(); // 0x4067d0: the result of CheckTextstraVersion made at start-up
void ShowCdNotFoundError(); // 0x4067e0: message box, no owner
void ShowGameVersionOlderError(); // 0x406800: "Your version of the game is older than your opponent's version."
void ShowOpponentVersionOlderError(); // 0x406820: "Your opponent's version of the game is older than your version."
const char* GetLanguageName(); // 0x406840: language of textstra.dll, e.g. "English (United States)"
// Reads the file version of textstra.dll ("major.minor.build" in its StringFileInfo) and returns 1 when both numbers
// that are looked at are 0; 1 as well when the DLL carries no version resource.
int CheckTextstraVersion(); // 0x406850
