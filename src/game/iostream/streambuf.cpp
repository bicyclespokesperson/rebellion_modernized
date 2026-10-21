#include <cstring>

#include "iostream/iostream.h"

streambuf* streambuf_ctor(streambuf* self)
{
  if (self->m_lockInitialized) {
    return self;
  }
  self->m_cleaned = false;
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
  self->m_lastc = -1;
  self->m_lockCount = -1;
  self->m_lockInitialized = true;
  return self;
}

void streambuf_dtor(streambuf* self)
{
  self->Cleanup();
}
void streambuf::Initialize()
{
  streambuf_ctor(this);
}

void streambuf::Cleanup()
{
  streambuf* self = this;
  if (m_cleaned) {
    return;
  }
  m_cleaned = true;
  if (self->m_lockInitialized) {
    self->m_lockInitialized = false;
  }
  streambuf_sync(self);
  if (self->m_allocated != 0 && self->m_base != nullptr) {
    ::operator delete(self->m_base);
  }
}

streambuf* streambuf_setbuf(streambuf* self, char* buffer, int size)
{
  if (self->m_base != nullptr) {
    return nullptr;
  }
  if (buffer != nullptr && size != 0) {
    self->m_base = buffer;
    self->m_ebuf = buffer + size;
    self->m_unbuffered = 0;
    return self;
  }
  self->m_unbuffered = 1;
  return self;
}

int streambuf_xsputn(streambuf* self, const char* text, int count)
{
  int written = 0;
  while (count != 0) {
    --count;
    if (self->m_unbuffered == 0 && self->m_pptr < self->m_epptr) {
      *self->m_pptr++ = *text;
    } else if (self->Overflow(static_cast<unsigned char>(*text)) == -1) {
      return written;
    }
    ++text;
    ++written;
  }
  return written;
}

int streambuf_xsgetn(streambuf* self, char* buffer, int count)
{
  int total = 0;
  if (self->m_unbuffered == 0) {
    while (count != 0) {
      if (self->Underflow() == -1) {
        return total;
      }
      int available = static_cast<int>(self->m_egptr - self->m_gptr);
      if (count <= available) {
        available = count;
      }
      if (0 < available) {
        memmove(buffer, self->m_gptr, available);
        total += available;
        buffer += available;
        self->m_gptr += available;
        count -= available;
      }
    }
  } else {
    if (self->m_lastc == -1) {
      self->m_lastc = self->Underflow();
    }
    if (count != 0) {
      while (--count, self->m_lastc != -1) {
        *buffer++ = static_cast<char>(self->m_lastc);
        ++total;
        self->m_lastc = self->Underflow();
        if (count == 0) {
          return total;
        }
      }
    }
  }
  return total;
}

int streambuf_sync(streambuf* self)
{
  if (self->m_egptr <= self->m_gptr && self->m_pptr <= self->m_pbase) {
    return 0;
  }
  return -1;
}

int streambuf_allocate(streambuf* self)
{
  if (self->m_unbuffered == 0 && self->m_base == nullptr) {
    const int result = self->Allocate();
    return result != -1 ? 1 : -1;
  }
  return 0;
}

int streambuf_doallocate(streambuf* self)
{
  constexpr int kDefaultBufferSize = 0x200;
  char* buffer = static_cast<char*>(::operator new(kDefaultBufferSize));
  if (buffer == nullptr) {
    return -1;
  }
  streambuf_setb(self, buffer, buffer + kDefaultBufferSize, 1);
  return 1;
}

void streambuf_setb(streambuf* self, char* begin, char* end, int owned)
{
  if (self->m_allocated != 0 && self->m_base != nullptr) {
    ::operator delete(self->m_base);
  }
  self->m_base = begin;
  self->m_allocated = owned;
  self->m_ebuf = end;
}

int streambuf_seekoff(streambuf*, long, int, int)
{
  return -1;
}

int streambuf_seekpos(streambuf* self, long position, int mode)
{
  return self->SeekOffset(position, 0, mode);
}

int streambuf_pbackfail(streambuf* self, int c)
{
  if (self->m_eback < self->m_gptr) {
    *--self->m_gptr = static_cast<char>(c);
    return static_cast<char>(c);
  }
  if (self->SeekOffset(-1, 1, 1) == -1) {
    return -1;
  }
  if (self->m_unbuffered == 0 && self->m_egptr != nullptr) {
    memmove(self->m_gptr + 1, self->m_gptr, static_cast<int>(self->m_egptr - self->m_gptr) - 1);
    *self->m_gptr = static_cast<char>(c);
  }
  return c;
}

int streambuf_snextc(streambuf* self)
{
  if (self->m_unbuffered != 0) {
    if (self->m_lastc == -1) {
      self->Underflow();
    }
    self->m_lastc = self->Underflow();
    return self->m_lastc;
  }
  if (self->m_egptr == nullptr || self->m_egptr <= self->m_gptr) {
    self->Underflow();
  }
  ++self->m_gptr;
  if (self->m_gptr < self->m_egptr) {
    return static_cast<unsigned char>(*self->m_gptr);
  }
  return self->Underflow();
}

int streambuf_sbumpc(streambuf* self)
{
  if (self->m_unbuffered == 0) {
    if (self->m_gptr < self->m_egptr) {
      return static_cast<unsigned char>(*self->m_gptr++);
    }
    const int result = self->Underflow();
    ++self->m_gptr;
    return result;
  }
  const int pending = self->m_lastc;
  if (pending == -1) {
    return self->Underflow();
  }
  self->m_lastc = -1;
  return pending;
}

void streambuf_stossc(streambuf* self)
{
  if (self->m_unbuffered == 0) {
    if (self->m_egptr <= self->m_gptr) {
      self->Underflow();
    }
    if (self->m_gptr < self->m_egptr) {
      ++self->m_gptr;
    }
    return;
  }
  if (self->m_lastc == -1) {
    self->Underflow();
    return;
  }
  self->m_lastc = -1;
}

int streambuf_sgetc(streambuf* self)
{
  if (self->m_unbuffered != 0) {
    if (self->m_lastc == -1) {
      self->m_lastc = self->Underflow();
    }
    return self->m_lastc;
  }
  return self->Underflow();
}

streambuf::streambuf()
{
  streambuf_ctor(this);
}
streambuf::~streambuf()
{
  Cleanup();
}
int streambuf::Sync()
{
  return streambuf_sync(this);
}
streambuf* streambuf::SetBuffer(char* buffer, int size)
{
  return streambuf_setbuf(this, buffer, size);
}
int streambuf::SeekOffset(long offset, int dir, int mode)
{
  return streambuf_seekoff(this, offset, dir, mode);
}
int streambuf::SeekPosition(long position, int mode)
{
  return streambuf_seekpos(this, position, mode);
}
int streambuf::Write(const char* text, int count)
{
  return streambuf_xsputn(this, text, count);
}
int streambuf::Read(char* buffer, int count)
{
  return streambuf_xsgetn(this, buffer, count);
}
int streambuf::Overflow(int)
{
  return -1;
}
int streambuf::Underflow()
{
  return -1;
}
int streambuf::Putback(int c)
{
  return streambuf_pbackfail(this, c);
}
int streambuf::Allocate()
{
  return streambuf_doallocate(this);
}
