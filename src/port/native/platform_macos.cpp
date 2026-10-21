// macOS half of src/port/native/platform.h.
#include "port/native/platform.h"

#include <cstdint>
#include <cstdlib>
#include <mach-o/dyld.h>

namespace reb::platform
{

std::string UserConfigDir()
{
  const char* home = getenv("HOME");
  if (home && *home) {
    return std::string(home) + "/Library/Application Support/Rebellion";
  }
  return {};
}

std::vector<std::string> SystemFontDirs()
{
  std::vector<std::string> dirs = {"/System/Library/Fonts/Supplemental", "/Library/Fonts"};
  if (const char* home = getenv("HOME")) {
    dirs.push_back(std::string(home) + "/Library/Fonts");
  }
  return dirs;
}

std::string BundledFontDir()
{
  char buffer[4096];
  uint32_t size = sizeof buffer;
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    return {};
  }
  std::string path(buffer);
  const std::string macosDir = path.substr(0, path.find_last_of('/'));
  return macosDir + "/../Resources/fonts";
}

FileTimestamps StatTimestamps(const struct stat& info)
{
  return {info.st_mtimespec, info.st_atimespec, info.st_birthtimespec};
}

} // namespace reb::platform
