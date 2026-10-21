// Windows half of src/port/native/platform.h.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "port/native/platform.h"

#include <stdlib.h>

#include <string>

namespace reb::platform
{

namespace
{

std::string Environment(const char* name)
{
  const DWORD required = GetEnvironmentVariableA(name, nullptr, 0);
  if (required == 0) {
    return {};
  }
  std::string value(required, '\0');
  const DWORD written = GetEnvironmentVariableA(name, value.data(), required);
  if (written == 0 || written >= required) {
    return {};
  }
  value.resize(written);
  return value;
}

std::string ModulePath()
{
  std::string path(32768, '\0');
  const DWORD length = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size()) {
    return {};
  }
  path.resize(length);
  return path;
}

} // namespace

std::string UserConfigDir()
{
  const std::string appData = Environment("APPDATA");
  return appData.empty() ? std::string{} : appData + "\\Rebellion";
}

std::vector<std::string> SystemFontDirs()
{
  std::vector<std::string> dirs;
  std::string windows(32768, '\0');
  const UINT length = GetWindowsDirectoryA(windows.data(), static_cast<UINT>(windows.size()));
  if (length > 0 && length < windows.size()) {
    windows.resize(length);
    dirs.push_back(windows + "\\Fonts");
  }
  const std::string local = Environment("LOCALAPPDATA");
  if (!local.empty()) {
    dirs.push_back(local + "\\Microsoft\\Windows\\Fonts");
  }
  return dirs;
}

std::string BundledFontDir()
{
  const std::string path = ModulePath();
  const size_t separator = path.find_last_of("\\/");
  return separator == std::string::npos ? std::string{} : path.substr(0, separator) + "\\fonts";
}

} // namespace reb::platform
