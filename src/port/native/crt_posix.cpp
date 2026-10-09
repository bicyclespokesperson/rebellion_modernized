// The MSVC CRT functions the game calls that POSIX does not have under that name, and the stand-ins for the original's
// statically linked helpers (the counterpart of src/standalone/crt_shims.cpp). Windows-style path names are translated by the
// resource library's ResolvePath (backslashes, drive letters, any case).
//
// There is no text mode here: the game opens everything in binary mode (src/game/core/file_util.cpp, sound_source.cpp), so
// the CRLF translation of the MSVC CRT is never needed.
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "core/reb_abi.h"
#include "port/minwin_impl/res/path.h"

namespace {

constexpr int kMsvcAppend = 0x8;
constexpr int kMsvcCreate = 0x100;
constexpr int kMsvcTruncate = 0x200;
constexpr int kMsvcExclusive = 0x400;
constexpr int kAccessMask = 3;

int PosixOpenFlags(int msvcFlags) {
    int flags = msvcFlags & kAccessMask;
    if (msvcFlags & kMsvcAppend) flags |= O_APPEND;
    if (msvcFlags & kMsvcCreate) flags |= O_CREAT;
    if (msvcFlags & kMsvcTruncate) flags |= O_TRUNC;
    if (msvcFlags & kMsvcExclusive) flags |= O_EXCL;
    return flags | O_CLOEXEC;
}

}  // namespace

extern "C" {

int _sopen(const char* name, int flags, int, ...) {
    return open(mw::res::ResolvePath(name).c_str(), PosixOpenFlags(flags), 0644);
}

int _close(int fd) { return close(fd); }
int _read(int fd, void* buffer, unsigned count) { return static_cast<int>(read(fd, buffer, count)); }
int _write(int fd, const void* buffer, unsigned count) { return static_cast<int>(write(fd, buffer, count)); }
long _lseek(int fd, long offset, int origin) { return static_cast<long>(lseek(fd, offset, origin)); }
int _setmode(int, int) { return 0x8000; }

int _chdir(const char* path) { return chdir(mw::res::ResolvePath(path).c_str()); }
int _mkdir(const char* path) { return mkdir(mw::res::ResolvePath(path).c_str(), 0755); }
int _chdrive(int) { return 0; }

FILE* reb_fopen(const char* name, const char* mode) { return fopen(mw::res::ResolvePath(name).c_str(), mode); }

int _isprint(int c) { return isprint(c); }
int _isspace(int c) { return isspace(c); }
int _ispunct(int c) { return ispunct(c); }

}  // extern "C"

int icf_return_zero() { return 0; }  // 0x6158b0, also strstreambuf::sync and _matherr

void FUN_00619730() {}  // 0x619730 icf_empty_function: the linker folded every empty no-argument function into it
