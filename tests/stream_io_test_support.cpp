#if !defined(_WIN32)
#include <cstdarg>
#include <fcntl.h>
#include <unistd.h>

#include "platform/win32.h"

namespace
{

constexpr int kMsvcAppend = 0x8;
constexpr int kMsvcCreate = 0x100;
constexpr int kMsvcTruncate = 0x200;
constexpr int kMsvcExclusive = 0x400;

int PosixOpenFlags(int flags)
{
  int result = (flags & 3) == 0 ? O_RDONLY : (flags & 3) == 1 ? O_WRONLY : O_RDWR;
  if ((flags & kMsvcAppend) != 0) {
    result |= O_APPEND;
  }
  if ((flags & kMsvcCreate) != 0) {
    result |= O_CREAT;
  }
  if ((flags & kMsvcTruncate) != 0) {
    result |= O_TRUNC;
  }
  if ((flags & kMsvcExclusive) != 0) {
    result |= O_EXCL;
  }
  return result;
}

} // namespace

extern "C" {

int _sopen(const char* name, int flags, int, ...)
{
  return open(name, PosixOpenFlags(flags), 0644);
}
int _close(int fd)
{
  return close(fd);
}
int _read(int fd, void* buffer, unsigned count)
{
  return static_cast<int>(read(fd, buffer, count));
}
int _write(int fd, const void* buffer, unsigned count)
{
  return static_cast<int>(write(fd, buffer, count));
}
long _lseek(int fd, long offset, int origin)
{
  return static_cast<long>(lseek(fd, offset, origin));
}
int _setmode(int, int)
{
  return 0x8000;
}

} // extern "C"
#endif

const char* g_emptyText = "";
