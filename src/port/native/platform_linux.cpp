// Linux half of src/port/native/platform.h.
#include "port/native/platform.h"

#include <pthread.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace reb::platform
{

namespace
{

const char* Env(const char* name)
{
  const char* value = getenv(name);
  return (value && *value) ? value : nullptr;
}

} // namespace

std::string UserConfigDir()
{
  if (const char* xdg = Env("XDG_CONFIG_HOME")) {
    return std::string(xdg) + "/rebellion";
  }
  if (const char* home = Env("HOME")) {
    return std::string(home) + "/.config/rebellion";
  }
  return {};
}

std::vector<std::string> SystemFontDirs()
{
  std::vector<std::string> dirs;
  if (const char* home = Env("HOME")) {
    dirs.push_back(std::string(home) + "/.fonts");
    dirs.push_back(std::string(home) + "/.local/share/fonts");
  }
  if (const char* xdg = Env("XDG_DATA_HOME")) {
    dirs.push_back(std::string(xdg) + "/fonts");
  }
  dirs.push_back("/usr/share/fonts");
  dirs.push_back("/usr/local/share/fonts");
  return dirs;
}

std::string BundledFontDir()
{
  char buffer[4096];
  const ssize_t length = readlink("/proc/self/exe", buffer, sizeof buffer - 1);
  if (length <= 0) {
    return {};
  }
  std::string path(buffer, static_cast<size_t>(length));
  return path.substr(0, path.find_last_of('/')) + "/fonts";
}

uint32_t CurrentThreadId()
{
  return static_cast<uint32_t>(syscall(SYS_gettid));
}

// The audio and video threads ask for a slightly higher priority; the Unix ports never raised it (it needs privileges).
void RaiseCurrentThreadPriority() {}

void SetCurrentThreadName(const char* name)
{
  pthread_setname_np(pthread_self(), name);
}

FileTimestamps StatTimestamps(const struct stat& info)
{
  return {info.st_mtim, info.st_atim, info.st_ctim};
}

} // namespace reb::platform
