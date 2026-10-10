// The OS-specific part of the native build, and nothing else: every other source file of the native port is shared by
// Linux and macOS. One implementation per OS (platform_linux.cpp, platform_macos.cpp), selected by cmake/native_platform.cmake.
// The full list of OS differences is in docs/native.md.
#pragma once
#include <stdint.h>
#include <sys/stat.h>

#include <string>
#include <vector>

namespace reb::platform {

// Directory holding the port's config.ini when $REBELLION_CONFIG is not set:
//   Linux  $XDG_CONFIG_HOME/rebellion, else ~/.config/rebellion
//   macOS  ~/Library/Application Support/Rebellion
// Empty when the home directory is unknown (the config file is then looked up in the working directory).
std::string UserConfigDir();

// Directories searched for Arial (and, failing that, Liberation Sans), in order.
std::vector<std::string> SystemFontDirs();

// Directory next to the executable that holds the Liberation Sans fallback fonts shipped with the build:
//   Linux  <executable dir>/fonts        macOS  Rebellion.app/Contents/Resources/fonts
std::string BundledFontDir();

// Operating-system identifier of the calling thread (kernel32 GetCurrentThreadId).
uint32_t CurrentThreadId();

// Names the calling thread for debuggers and profilers (the two OSes take different pthread_setname_np forms).
void SetCurrentThreadName(const char* name);

struct FileTimestamps {
    timespec modified;
    timespec accessed;
    timespec created;
};
FileTimestamps StatTimestamps(const struct stat& info);

}  // namespace reb::platform
