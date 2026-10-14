#pragma once
// Strings of the original image that reach Win32 calls unchanged. They are referenced instead of copied so that
// the pointers match the original's (the call tracer of difftest compares arguments by value). Names marked
// "shared" are also declared in app_strings.h with identical types.

extern const char* g_registryKeyPath; // 0x6a81fc shared: "SOFTWARE\LucasArts ...\Star Wars Rebellion"
extern const char* g_volumeName; // 0x6a81e8 shared: "Volume"
extern const char* g_displayOptsName; // 0x6a81f0 shared: "DisplayOpts"
extern const char* g_installedLocationName; // 0x6a7c84 shared: "InstalledLocation"
extern const char* g_tooBigLocationName; // 0x6a7c74 shared: "TooBigLocation"
extern const char* g_dllLocationName; // 0x6a7c10: "DllLocation"
extern const char* g_dataLocationName; // 0x6a7c64: "DataLocation"
extern const char* g_localMachineRootName; // 0x6a844c: "HKEY_LOCAL_MACHINE"
extern const char* g_textstraDllName; // 0x6a7ecc: "textstra.dll"
