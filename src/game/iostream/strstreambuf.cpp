#include <cstring>

#include "iostream/iostream.h"

namespace {

unsigned RangeLength(const char* begin, const char* end) { return begin < end ? static_cast<unsigned>(end - begin) : 0u; }

void FreeBuffer(strstreambuf* self, char* buffer) {
    if (self->m_free == nullptr)
        ::operator delete(buffer);
    else
        self->m_free(buffer);
}

}  // namespace

strstreambuf* __thiscall strstreambuf_ctor(strstreambuf* self) {
    streambuf_ctor(self);
    self->m_vftable = &strstreambuf::vftable;
    self->m_dynamic = 1;
    self->m_nextSize = 1;
    self->m_static = 0;
    self->m_allocate = nullptr;
    self->m_free = nullptr;
    return self;
}

strstreambuf* __thiscall strstreambuf_ctor_buf(strstreambuf* self, char* buffer, int size, char* start) {
    streambuf_ctor(self);
    self->m_vftable = &strstreambuf::vftable;
    self->m_static = 1;
    self->m_dynamic = 0;
    char* end;
    if (size == 0)
        end = buffer + strlen(buffer);
    else if (size < 0)
        end = reinterpret_cast<char*>(-1);
    else
        end = buffer + size;
    streambuf_setb(self, buffer, end, 0);
    self->m_eback = buffer;
    self->m_gptr = buffer;
    if (start == nullptr) {
        self->m_egptr = end;
        self->m_lastc = -1;
        self->m_pbase = nullptr;
        self->m_pptr = nullptr;
        self->m_epptr = nullptr;
    } else {
        self->m_egptr = start;
        self->m_lastc = -1;
        self->m_pbase = start;
        self->m_pptr = start;
        self->m_epptr = end;
    }
    return self;
}

void __thiscall strstreambuf_dtor(strstreambuf* self) {
    self->m_vftable = &strstreambuf::vftable;
    if (self->m_dynamic != 0 && self->m_base != nullptr) FreeBuffer(self, self->m_base);
    streambuf_dtor(self);
}

void __thiscall strstreambuf_freeze(streambuf* self, int frozen) {
    auto* buffer = static_cast<strstreambuf*>(self);
    if (buffer->m_static == 0) buffer->m_dynamic = frozen == 0;
}

char* __thiscall strstreambuf_str(streambuf* self) {
    static_cast<strstreambuf*>(self)->m_dynamic = 0;
    return self->m_base;
}

int __thiscall strstreambuf_doallocate(strstreambuf* self) {
    int newSize = self->m_nextSize;
    const int grown = RangeLength(self->m_base, self->m_ebuf) + (self->m_nextSize < 2 ? 1 : self->m_nextSize);
    if (self->m_nextSize <= grown) newSize = grown;
    char* buffer = static_cast<char*>(self->m_allocate == nullptr ? ::operator new(newSize) : self->m_allocate(newSize));
    if (buffer == nullptr) return -1;
    int delta = 0;
    if (RangeLength(self->m_base, self->m_ebuf) != 0) {
        memmove(buffer, self->m_base, RangeLength(self->m_base, self->m_ebuf));
        delta = static_cast<int>(buffer - self->m_base);
    }
    FreeBuffer(self, self->m_base);
    streambuf_setb(self, buffer, buffer + newSize, 0);
    if (delta != 0 && self->m_egptr != nullptr) {
        self->m_eback += delta;
        self->m_gptr += delta;
        self->m_egptr += delta;
        self->m_lastc = -1;
    }
    if (self->m_epptr != nullptr) {
        char* oldBase = self->m_pbase;
        char* oldPptr = self->m_pptr;
        self->m_epptr += delta;
        self->m_pbase = oldBase + delta;
        self->m_pptr = oldBase + delta;
        if (self->m_epptr != nullptr) self->m_pptr = oldBase + delta + (oldPptr - oldBase);
    }
    return 1;
}

streambuf* __thiscall strstreambuf_setbuf(strstreambuf* self, char*, int size) {
    if (size != 0) self->m_nextSize = size;
    return self;
}

int __thiscall strstreambuf_overflow(strstreambuf* self, int c) {
    if (self->m_epptr <= self->m_pptr) {
        if (self->m_dynamic == 0) return -1;
        if (strstreambuf_doallocate(self) == -1) return -1;
        if (self->m_epptr == nullptr) {
            char* put = (self->m_egptr - self->m_eback) + self->m_base;
            self->m_pbase = put;
            self->m_pptr = put;
            self->m_epptr = self->m_ebuf;
        } else {
            char* oldBase = self->m_pbase;
            char* oldPptr = self->m_pptr;
            self->m_pptr = oldBase;
            self->m_epptr = self->m_ebuf;
            if (self->m_ebuf != nullptr) self->m_pptr = oldBase + (oldPptr - oldBase);
        }
    }
    if (c != -1) {
        *self->m_pptr = static_cast<char>(c);
        if (self->m_epptr != nullptr) ++self->m_pptr;
    }
    return 1;
}

int __thiscall strstreambuf_underflow(strstreambuf* self) {
    const unsigned char* get = reinterpret_cast<unsigned char*>(self->m_gptr);
    if (self->m_egptr <= self->m_gptr) {
        if (self->m_egptr < self->m_pptr) {
            char* oldBack = self->m_eback;
            self->m_eback = self->m_base;
            self->m_egptr = self->m_pptr;
            self->m_lastc = -1;
            self->m_gptr = self->m_gptr + (self->m_base - oldBack);
        }
        get = reinterpret_cast<unsigned char*>(self->m_gptr);
        if (self->m_egptr <= self->m_gptr) return -1;
    }
    return *get;
}

int __thiscall strstreambuf_seekoff(strstreambuf* self, long offset, int dir, int mode) {
    int result = -1;
    if (mode & iostream_flags::kIn) {
        strstreambuf_underflow(self);
        char* origin;
        if (dir == iostream_flags::kSeekBeg)
            origin = self->m_eback;
        else if (dir == iostream_flags::kSeekCur)
            origin = self->m_gptr;
        else if (dir == iostream_flags::kSeekEnd)
            origin = self->m_egptr;
        else
            return -1;
        char* target = origin + offset;
        result = static_cast<int>(target - self->m_eback);
        if (target < self->m_eback) return -1;
        if (self->m_egptr < target) return -1;
        if (self->m_egptr != nullptr) self->m_gptr = target;
    }
    if (mode & iostream_flags::kOut) {
        if (self->m_epptr == nullptr && strstreambuf_overflow(self, -1) == -1) return -1;
        char* origin;
        if (dir == iostream_flags::kSeekBeg)
            origin = self->m_pbase;
        else if (dir == iostream_flags::kSeekCur)
            origin = self->m_pptr;
        else if (dir == iostream_flags::kSeekEnd)
            origin = self->m_epptr;
        else
            return -1;
        char* target = origin + offset;
        result = static_cast<int>(target - self->m_pbase);
        if (target < self->m_pbase) return -1;
        if (self->m_epptr < target) {
            if (self->m_dynamic == 0) return -1;
            self->m_nextSize = result < self->m_nextSize ? self->m_nextSize : result;
            if (strstreambuf_doallocate(self) == -1) return -1;
        }
        // BUG(original): after the buffer grew, `target` still points into the freed old buffer.
        if (self->m_epptr != nullptr) self->m_pptr = target;
    }
    return result;
}
