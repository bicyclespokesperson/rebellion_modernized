#include "core/reb_abi.h"
#include <cstring>

#include "iostream/iostream.h"

// The game CRT's low-level file I/O (declared here: the SDK header turns _sopen into an inline wrapper).
extern "C" {
int REB_CDECL _close(int fd);
int REB_CDECL _read(int fd, void* buffer, unsigned count);
int REB_CDECL _write(int fd, const void* buffer, unsigned count);
long REB_CDECL _lseek(int fd, long offset, int origin);
int REB_CDECL _sopen(const char* name, int flags, int share, ...);
}

#ifndef REB_STANDALONE  // the shared flags also define REB_HYBRID in the standalone build
// The CRT's table of low-level file handle info: blocks of 32 entries of 0x24 bytes, the handle flags at +4.
extern uint8_t* g_pioinfo[];  // 0x6bed20
#else
#include <fcntl.h>
#include <io.h>
#endif

namespace {

constexpr int kEntriesPerBlockShift = 5;
constexpr int kEntryMask = 0x1f;
constexpr int kEntrySize = 0x24;
constexpr uint8_t kTextModeFlag = 0x80;
constexpr uint8_t kPendingCrFlag = 0x02;

constexpr int kOpenNoCreate = 0x20;
constexpr int kOpenNoReplace = 0x40;
constexpr int kOpenBinary = 0x80;
constexpr int kDefaultBufferSize = 0x200;

// _open() flags and _sopen() sharing modes
constexpr int kOText = 0x4000;
constexpr int kOBinary = 0x8000;
constexpr int kOCreate = 0x100;
constexpr int kOExcl = 0x400;
constexpr int kOAppend = 0x8;
constexpr int kOTrunc = 0x200;
constexpr int kOWriteOnly = 1;
constexpr int kOReadWrite = 2;
constexpr int kShareDenyNone = 0x40;
constexpr int kShareDenyRead = 0x30;
constexpr int kShareDenyWrite = 0x20;
constexpr int kShareCompat = 0x10;
constexpr int kDefaultPermissions = 0x180;

constexpr int kProtectionMask = 0xe00;

#ifndef REB_STANDALONE  // the shared flags also define REB_HYBRID in the standalone build
uint8_t HandleFlags(int fd) { return g_pioinfo[fd >> kEntriesPerBlockShift][4 + (fd & kEntryMask) * kEntrySize]; }
#else
// The host CRT keeps its handle table private. Text mode is observable through _setmode; the end-of-file flag the
// original adds a byte for is not, so a text-mode seek back after reading to a lone CR at the end of a buffer can be
// one byte off (docs/library_code.md, CRT decisions).
uint8_t HandleFlags(int fd) {
    const int mode = _setmode(fd, _O_BINARY);
    if (mode == -1) return 0;
    _setmode(fd, mode);
    return (mode & _O_TEXT) != 0 ? kTextModeFlag : 0;
}
#endif

int SharingModeFor(int protection) {
    protection &= kProtectionMask;
    int share = kShareDenyNone;
    if (protection != 0) {
        if (protection < 0xa01) {
            if (protection == 0xa00)
                share = kShareDenyWrite;
            else if (protection == 0x800)
                share = kShareCompat;
        } else if (protection == 0xc00) {
            share = kShareDenyRead;
        } else if (protection == 0xe00) {
            share = kShareDenyNone;
        }
    }
    return share;
}

unsigned RangeLength(const char* begin, const char* end) { return begin < end ? static_cast<unsigned>(end - begin) : 0u; }

}  // namespace

filebuf* REB_THISCALL filebuf_ctor(filebuf* self) {
    streambuf_ctor(self);
    self->m_vftable = &filebuf::vftable;
    self->m_open = 0;
    self->m_fd = -1;
    return self;
}

void REB_THISCALL filebuf_dtor(filebuf* self) {
    self->m_vftable = &filebuf::vftable;
    iostream_access::LockBuffer(self);
    if (self->m_open == 0)
        filebuf_sync(self);
    else
        filebuf_close(self);
    streambuf_dtor(self);
}

filebuf* REB_THISCALL filebuf_close(filebuf* self) {
    if (self->m_fd != -1) {
        iostream_access::LockBuffer(self);
        const int synced = self->m_vftable->m_sync(self);
        const int closed = _close(self->m_fd);
        if (closed != -1 && synced != -1) {
            self->m_fd = -1;
            iostream_access::UnlockBuffer(self);
            return self;
        }
        iostream_access::UnlockBuffer(self);
    }
    return nullptr;
}

int REB_THISCALL filebuf_overflow(filebuf* self, int c) {
    if (streambuf_allocate(self) == -1) return -1;
    if (filebuf_sync(self) == -1) return -1;
    if (self->m_unbuffered == 0) {
        self->m_pbase = self->m_base;
        self->m_pptr = self->m_base;
        self->m_epptr = self->m_ebuf;
    }
    if (c != -1) {
        if (self->m_unbuffered == 0 && self->m_pptr < self->m_epptr) {
            *self->m_pptr++ = static_cast<char>(c);
            return 1;
        }
        if (_write(self->m_fd, &c, 1) != 1) return -1;
    }
    return 1;
}

int REB_THISCALL filebuf_underflow(filebuf* self) {
    const char* get = self->m_gptr;
    if (RangeLength(self->m_gptr, self->m_egptr) == 0) {
        int single = 0;
        if (streambuf_allocate(self) == -1) return -1;
        if (filebuf_sync(self) == -1) return -1;
        if (self->m_unbuffered != 0) {
            if (_read(self->m_fd, &single, 1) < 1) return -1;
            return single & 0xff;
        }
        const int got = _read(self->m_fd, self->m_base, RangeLength(self->m_base, self->m_ebuf));
        if (got < 1) return -1;
        get = self->m_base;
        self->m_lastc = -1;
        self->m_eback = self->m_base;
        self->m_gptr = self->m_base;
        self->m_egptr = self->m_base + got;
    }
    return static_cast<unsigned char>(*get);
}

int REB_THISCALL filebuf_seekoff(filebuf* self, long offset, int dir, int) {
    if (dir != iostream_flags::kSeekBeg && dir != iostream_flags::kSeekCur && dir != iostream_flags::kSeekEnd) return -1;
    if (filebuf_sync(self) != -1) {
        const int position = _lseek(self->m_fd, offset, dir);
        if (position != -1) return position;
    }
    return -1;
}

int REB_THISCALL filebuf_sync(filebuf* self) {
    if (self->m_fd == -1) return -1;
    if (self->m_unbuffered == 0) {
        const int pending = static_cast<int>(RangeLength(self->m_pbase, self->m_pptr));
        if (pending != 0) {
            const int written = _write(self->m_fd, self->m_pbase, pending);
            if (written != pending) {
                if (written < 1) return -1;
                if (self->m_epptr != nullptr) self->m_pptr -= written;
                memmove(self->m_pbase, self->m_pbase + written, pending - written);
                return -1;
            }
        }
        const char* end = self->m_egptr;
        const char* get = self->m_gptr;
        self->m_pbase = nullptr;
        self->m_pptr = nullptr;
        self->m_epptr = nullptr;
        int unread = static_cast<int>(RangeLength(get, end));
        if (0 < unread) {
            const int fd = self->m_fd;
            const uint8_t flags = HandleFlags(fd);
            if ((flags & kTextModeFlag) != 0) {
                for (; get < end; ++get)
                    if (*get == '\n') ++unread;
                if ((flags & kPendingCrFlag) != 0) ++unread;
            }
            if (_lseek(fd, -unread, iostream_flags::kSeekCur) == -1) return -1;
        }
        self->m_eback = nullptr;
        self->m_gptr = nullptr;
        self->m_egptr = nullptr;
        self->m_lastc = -1;
    }
    return 0;
}

streambuf* REB_THISCALL filebuf_setbuf(filebuf* self, char* buffer, int size) {
    if (self->m_fd != -1 && self->m_ebuf != nullptr) return nullptr;
    if (buffer == nullptr || size < 1) {
        self->m_unbuffered = 1;
    } else {
        iostream_access::LockBuffer(self);
        streambuf_setb(self, buffer, buffer + size, 0);
        iostream_access::UnlockBuffer(self);
    }
    return self;
}

filebuf* REB_THISCALL filebuf_open(filebuf* self, const char* name, int mode, int protection) {
    if (self->m_fd != -1) return nullptr;
    int flags = (mode & kOpenBinary) != 0 ? kOBinary : kOText;
    if ((mode & kOpenNoCreate) == 0) flags |= kOCreate;
    if ((mode & kOpenNoReplace) != 0) flags |= kOExcl;
    if ((mode & iostream_flags::kApp) != 0) {
        mode |= iostream_flags::kOut;
        flags |= kOAppend;
    }
    if ((mode & iostream_flags::kTrunc) != 0) {
        mode |= iostream_flags::kOut;
        flags |= kOTrunc;
    }
    if ((mode & iostream_flags::kOut) == 0) {
        if ((mode & iostream_flags::kIn) == 0) return nullptr;
    } else {
        flags |= (mode & iostream_flags::kIn) == 0 ? kOWriteOnly : kOReadWrite;
        if ((mode & (iostream_flags::kIn | iostream_flags::kAte | iostream_flags::kApp | kOpenNoReplace)) == 0) {
            mode |= iostream_flags::kTrunc;
            flags |= kOTrunc;
        }
    }
    self->m_fd = _sopen(name, flags, SharingModeFor(protection), kDefaultPermissions);
    if (self->m_fd == -1) return nullptr;
    iostream_access::LockBuffer(self);
    self->m_open = 1;
    if (self->m_unbuffered == 0 && self->m_ebuf == nullptr) {
        char* buffer = static_cast<char*>(::operator new(kDefaultBufferSize));
        if (buffer == nullptr)
            self->m_unbuffered = 1;
        else
            streambuf_setb(self, buffer, buffer + kDefaultBufferSize, 1);
    }
    if ((mode & iostream_flags::kAte) != 0 && self->m_vftable->m_seekoff(self, 0, iostream_flags::kSeekEnd, mode) == -1) {
        filebuf_close(self);
        if (-1 < self->m_lockCount) return nullptr;
        mtx_unlock(self->m_lock);
        return nullptr;
    }
    iostream_access::UnlockBuffer(self);
    return self;
}
