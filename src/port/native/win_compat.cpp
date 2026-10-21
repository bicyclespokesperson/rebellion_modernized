#include "port/native/win_compat.h"

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "port/minwin_impl/res/path.h"

DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size)
{
  const char* value = getenv(name);
  if (!value) {
    return 0;
  }
  const size_t length = strlen(value);
  if (length >= size) {
    return static_cast<DWORD>(length + 1);
  }
  memcpy(buffer, value, length + 1);
  return static_cast<DWORD>(length);
}

HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD, void*, DWORD disposition, DWORD, HANDLE)
{
  int flags = (access & GENERIC_WRITE) || (access & FILE_APPEND_DATA) ? O_WRONLY : O_RDONLY;
  if (access & FILE_APPEND_DATA) {
    flags |= O_APPEND;
  }
  if (disposition == CREATE_ALWAYS) {
    flags |= O_CREAT | O_TRUNC;
  }
  if (disposition == OPEN_ALWAYS) {
    flags |= O_CREAT;
  }
  const int fd = open(mw::res::ResolvePath(path).c_str(), flags | O_CLOEXEC, 0644);
  return fd < 0 ? INVALID_HANDLE_VALUE : reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd) + 1);
}

BOOL WriteFile(HANDLE file, const void* data, DWORD bytes, DWORD* written, void*)
{
  const ssize_t n = write(static_cast<int>(reinterpret_cast<intptr_t>(file) - 1), data, bytes);
  if (written) {
    *written = n < 0 ? 0 : static_cast<DWORD>(n);
  }
  return n >= 0;
}

void ExitProcess(UINT code)
{
  _exit(static_cast<int>(code));
}
