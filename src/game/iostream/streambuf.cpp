#include <cstring>

#include "iostream/iostream.h"

streambuf* __thiscall streambuf_ctor(streambuf* self) {
    self->m_allocated = 0;
    self->m_unbuffered = 0;
    self->m_base = nullptr;
    self->m_ebuf = nullptr;
    self->m_pbase = nullptr;
    self->m_pptr = nullptr;
    self->m_epptr = nullptr;
    self->m_eback = nullptr;
    self->m_gptr = nullptr;
    self->m_egptr = nullptr;
    self->m_vftable = &streambuf::vftable;
    self->m_lastc = -1;
    self->m_lockCount = -1;
    mtx_init(self->m_lock);
    return self;
}

void __thiscall streambuf_dtor(streambuf* self) {
    self->m_vftable = &streambuf::vftable;
    mtx_term(self->m_lock);
    streambuf_sync(self);
    if (self->m_allocated != 0 && self->m_base != nullptr) ::operator delete(self->m_base);
}

streambuf* __thiscall streambuf_setbuf(streambuf* self, char* buffer, int size) {
    if (self->m_base != nullptr) return nullptr;
    if (buffer != nullptr && size != 0) {
        self->m_base = buffer;
        self->m_ebuf = buffer + size;
        self->m_unbuffered = 0;
        return self;
    }
    self->m_unbuffered = 1;
    return self;
}

int __thiscall streambuf_xsputn(streambuf* self, const char* text, int count) {
    int written = 0;
    while (count != 0) {
        --count;
        if (self->m_unbuffered == 0 && self->m_pptr < self->m_epptr) {
            *self->m_pptr++ = *text;
        } else if (self->m_vftable->m_overflow(self, static_cast<unsigned char>(*text)) == -1) {
            return written;
        }
        ++text;
        ++written;
    }
    return written;
}

int __thiscall streambuf_xsgetn(streambuf* self, char* buffer, int count) {
    int total = 0;
    if (self->m_unbuffered == 0) {
        while (count != 0) {
            if (self->m_vftable->m_underflow(self) == -1) return total;
            int available = static_cast<int>(self->m_egptr - self->m_gptr);
            if (count <= available) available = count;
            if (0 < available) {
                memmove(buffer, self->m_gptr, available);
                total += available;
                buffer += available;
                self->m_gptr += available;
                count -= available;
            }
        }
    } else {
        if (self->m_lastc == -1) self->m_lastc = self->m_vftable->m_underflow(self);
        if (count != 0) {
            while (--count, self->m_lastc != -1) {
                *buffer++ = static_cast<char>(self->m_lastc);
                ++total;
                self->m_lastc = self->m_vftable->m_underflow(self);
                if (count == 0) return total;
            }
        }
    }
    return total;
}

int __thiscall streambuf_sync(streambuf* self) {
    if (self->m_egptr <= self->m_gptr && self->m_pptr <= self->m_pbase) return 0;
    return -1;
}

int __thiscall streambuf_allocate(streambuf* self) {
    if (self->m_unbuffered == 0 && self->m_base == nullptr) {
        const int result = self->m_vftable->m_doallocate(self);
        return result != -1 ? 1 : -1;
    }
    return 0;
}

int __thiscall streambuf_doallocate(streambuf* self) {
    constexpr int kDefaultBufferSize = 0x200;
    char* buffer = static_cast<char*>(::operator new(kDefaultBufferSize));
    if (buffer == nullptr) return -1;
    streambuf_setb(self, buffer, buffer + kDefaultBufferSize, 1);
    return 1;
}

void __thiscall streambuf_setb(streambuf* self, char* begin, char* end, int owned) {
    if (self->m_allocated != 0 && self->m_base != nullptr) ::operator delete(self->m_base);
    self->m_base = begin;
    self->m_allocated = owned;
    self->m_ebuf = end;
}

int __thiscall streambuf_seekoff(streambuf*, long, int, int) { return -1; }

int __thiscall streambuf_seekpos(streambuf* self, long position, int mode) { return self->m_vftable->m_seekoff(self, position, 0, mode); }

int __thiscall streambuf_pbackfail(streambuf* self, int c) {
    if (self->m_eback < self->m_gptr) {
        *--self->m_gptr = static_cast<char>(c);
        return static_cast<char>(c);
    }
    if (self->m_vftable->m_seekoff(self, -1, 1, 1) == -1) return -1;
    if (self->m_unbuffered == 0 && self->m_egptr != nullptr) {
        memmove(self->m_gptr + 1, self->m_gptr, static_cast<int>(self->m_egptr - self->m_gptr) - 1);
        *self->m_gptr = static_cast<char>(c);
    }
    return c;
}

int __thiscall streambuf_snextc(streambuf* self) {
    if (self->m_unbuffered != 0) {
        if (self->m_lastc == -1) self->m_vftable->m_underflow(self);
        self->m_lastc = self->m_vftable->m_underflow(self);
        return self->m_lastc;
    }
    if (self->m_egptr == nullptr || self->m_egptr <= self->m_gptr) self->m_vftable->m_underflow(self);
    ++self->m_gptr;
    if (self->m_gptr < self->m_egptr) return static_cast<unsigned char>(*self->m_gptr);
    return self->m_vftable->m_underflow(self);
}

int __thiscall streambuf_sbumpc(streambuf* self) {
    if (self->m_unbuffered == 0) {
        if (self->m_gptr < self->m_egptr) return static_cast<unsigned char>(*self->m_gptr++);
        const int result = self->m_vftable->m_underflow(self);
        ++self->m_gptr;
        return result;
    }
    const int pending = self->m_lastc;
    if (pending == -1) return self->m_vftable->m_underflow(self);
    self->m_lastc = -1;
    return pending;
}

void __thiscall streambuf_stossc(streambuf* self) {
    if (self->m_unbuffered == 0) {
        if (self->m_egptr <= self->m_gptr) self->m_vftable->m_underflow(self);
        if (self->m_gptr < self->m_egptr) ++self->m_gptr;
        return;
    }
    if (self->m_lastc == -1) {
        self->m_vftable->m_underflow(self);
        return;
    }
    self->m_lastc = -1;
}

int __thiscall streambuf_sgetc(streambuf* self) {
    if (self->m_unbuffered != 0) {
        if (self->m_lastc == -1) self->m_lastc = self->m_vftable->m_underflow(self);
        return self->m_lastc;
    }
    return self->m_vftable->m_underflow(self);
}
