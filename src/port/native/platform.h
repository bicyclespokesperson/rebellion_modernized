// The OS-specific part of the native build, and nothing else: every other source file of the native port is shared by
// Windows, Linux and macOS. One implementation per OS, selected by cmake/native_platform.cmake.
// The full list of OS differences is in docs/native.md.
#pragma once
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#include <string>
#include <vector>

namespace reb::platform
{

// Directory holding the port's config.ini when REBELLION_CONFIG is not set:
//   Windows  %APPDATA%\\Rebellion
//   Linux    $XDG_CONFIG_HOME/rebellion, else ~/.config/rebellion
//   macOS    ~/Library/Application Support/Rebellion
// Empty when the home directory is unknown (the config file is then looked up in the working directory).
std::string UserConfigDir();

// Directories searched for Arial (and, failing that, Liberation Sans), in order.
std::vector<std::string> SystemFontDirs();

// Directory next to the executable that holds the Liberation Sans fallback fonts shipped with the build:
//   Windows/Linux  <executable dir>/fonts        macOS  Rebellion.app/Contents/Resources/fonts
std::string BundledFontDir();

#if !defined(_WIN32)
struct FileTimestamps {
  timespec modified;
  timespec accessed;
  timespec created;
};
FileTimestamps StatTimestamps(const struct stat& info);
#endif

} // namespace reb::platform
