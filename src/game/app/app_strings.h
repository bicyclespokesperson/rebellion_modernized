#pragma once
// Strings of the executable's read-only data that the application shell passes on to Win32 and registry calls.
// Reconstructed code refers to the original copies (not its own literals) so that the pointers handed to the API
// are the same as in the original, which is what the differential tester compares.

extern const char* g_registryKeyPath;  // 0x6a81fc "SOFTWARE\LucasArts Entertainment Company LLC\Star Wars Rebellion"
extern const char* g_mismatchOptsName; // 0x6a8240 "MismatchOpts"
extern const char* g_displayOptsName;  // 0x6a81f0 "DisplayOpts"
extern const char* g_volumeName;       // 0x6a81e8 "Volume"
extern const char* g_rebdlogDllName;   // 0x6a8250 "Rebdlog.dll"
extern const char* g_textcommDllName;  // 0x6a7edc "textcomm.dll"
extern const char* g_commonDllName;    // 0x6a7c98 "common.dll"
extern const char* g_appClassName;     // 0x6a8314 "STARWARS_REBELLION"
extern const char* g_introMusicFailedText;   // 0x6a8328 "Start Of Intro Music Failed."
extern const char* g_soundManagerCaption;    // 0x6a8348 "Application SoundManager."
extern const char* g_installedLocationName;  // 0x6a7c84 "InstalledLocation"
extern const char* g_tooBigLocationName;     // 0x6a7c74 "TooBigLocation"
extern const char* g_textsFileName;          // 0x6a7e78 "data0.reb"
extern const char* g_textstraDllName;        // 0x6a7ecc "textstra.dll"
extern const char* g_translationQuery;       // 0x6a7eb0 "\VarFileInfo\Translation"
extern const char* g_fileVersionQueryFormat; // 0x6a7e88 "\StringFileInfo\%04X%04X\FileVersion"
