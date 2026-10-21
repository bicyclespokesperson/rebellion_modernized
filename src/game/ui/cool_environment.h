#pragma once
#include <cstdint>
#include <string>

#include "platform/win32.h"

struct ResourceStringId;

// Start-up checks and services of the Cool toolkit that are not tied to a window.

// WinMain's capability check: a mouse, at least `minWidth` x `minHeight` and `minColors` colours, and no other
// running copy (`previousInstance` is WinMain's hPrevInstance). Shows a message box and returns 0 when one
// fails, 1 otherwise. The messages still name Coolhand's StarTrader, which shared the toolkit.
int CheckDisplayRequirements(HINSTANCE previousInstance, int minWidth, int minHeight,
                             unsigned int minColors); // 0x60a910

// MessageBeep(MB_ICONHAND) and a modal error box without an owner.
void ShowErrorMessage(const char* text, const char* caption); // 0x60a9e0

// Installs LoadGameString as the std::string class's resource loader (once).
void InitStringLoader();    // 0x60aa00
void InstallStringLoader(); // 0x60aa20

// The std::string resource loader: LoadStringA of `id->m_id` from the resource DLL in slot `id->m_group`.
// Returns non-zero when a string was loaded.
int LoadGameString(const ResourceStringId* id, int size, LPSTR buffer); // 0x60aa30

extern int g_stringLoaderInstalled;          // 0x6be5c0
extern const char* g_displayDriverName;      // 0x6ac8c4 "DISPLAY"
extern const char* g_noMouseMessage;         // 0x6ac8cc
extern const char* g_alreadyRunningMessage;  // 0x6ac868
extern const char* g_displayTooSmallMessage; // 0x6ac824
