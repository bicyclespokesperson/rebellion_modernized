#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

#include "iostream/iostream.h"

namespace
{

unsigned RangeLength(const char* begin, const char* end)
{
  return begin && end && begin < end ? static_cast<unsigned>(end - begin) : 0u;
}

void FreeBuffer(strstreambuf* self, char* buffer)
{
  if (self->m_free == nullptr)
    ::operator delete(buffer);
  else
    self->m_free(buffer);
}

} // namespace

strstreambuf* strstreambuf_ctor(strstreambuf* self)
{
  streambuf_ctor(self);
  self->m_dynamic = 1;
  self->m_nextSize = 1;
  self->m_static = 0;
  self->m_allocate = nullptr;
  self->m_free = nullptr;
  return self;
}

strstreambuf* strstreambuf_ctor_buf(strstreambuf* self, char* buffer, int size, char* start)
{
  streambuf_ctor(self);
  self->m_static = 1;
  self->m_dynamic = 0;
  char* end;
  if (size == 0)
    end = buffer + strlen(buffer);
  else if (size < 0)
    end = reinterpret_cast<char*>(static_cast<intptr_t>(-1));
  else
    end = buffer + size;
  streambuf_setb(self, buffer, end, 0);
  self->m_eback = buffer;
  self->m_gptr = buffer;
  if (start == nullptr)
  {
    self->m_egptr = end;
    self->m_lastc = -1;
    self->m_pbase = nullptr;
    self->m_pptr = nullptr;
    self->m_epptr = nullptr;
  }
  else
  {
    self->m_egptr = start;
    self->m_lastc = -1;
    self->m_pbase = start;
    self->m_pptr = start;
    self->m_epptr = end;
  }
  return self;
}

void strstreambuf_dtor(strstreambuf* self)
{
  if (self->m_dynamic != 0 && self->m_base != nullptr)
    FreeBuffer(self, self->m_base);
  streambuf_dtor(self);
}

void strstreambuf_freeze(streambuf* self, int frozen)
{
  auto* buffer = static_cast<strstreambuf*>(self);
  if (buffer->m_static == 0)
    buffer->m_dynamic = frozen == 0;
}

char* strstreambuf_str(streambuf* self)
{
  static_cast<strstreambuf*>(self)->m_dynamic = 0;
  return self->m_base;
}

int strstreambuf_doallocate(strstreambuf* self)
{
  const unsigned oldSize = RangeLength(self->m_base, self->m_ebuf);
  const unsigned increment = self->m_nextSize < 2 ? 1 : static_cast<unsigned>(self->m_nextSize);
  if (oldSize > static_cast<unsigned>(std::numeric_limits<int>::max()) - increment)
    return -1;
  const int newSize = static_cast<int>(oldSize + increment);
  char* buffer = static_cast<char*>(self->m_allocate == nullptr ? ::operator new(newSize) : self->m_allocate(newSize));
  if (buffer == nullptr)
    return -1;
  std::memset(buffer, 0, static_cast<size_t>(newSize));
  const auto offset = [&](const char* pointer) -> ptrdiff_t
  {
    return pointer && self->m_base ? pointer - self->m_base : 0;
  };
  const bool hasGetArea = self->m_egptr != nullptr;
  const bool hasPutArea = self->m_epptr != nullptr;
  const ptrdiff_t getBegin = offset(self->m_eback);
  const ptrdiff_t getPosition = offset(self->m_gptr);
  const ptrdiff_t getEnd = offset(self->m_egptr);
  const ptrdiff_t putBegin = offset(self->m_pbase);
  const ptrdiff_t putPosition = offset(self->m_pptr);
  const ptrdiff_t putEnd = offset(self->m_epptr);
  if (oldSize != 0)
    std::memcpy(buffer, self->m_base, oldSize);
  FreeBuffer(self, self->m_base);
  streambuf_setb(self, buffer, buffer + newSize, 0);
  if (hasGetArea)
  {
    self->m_eback = buffer + getBegin;
    self->m_gptr = buffer + getPosition;
    self->m_egptr = buffer + getEnd;
    self->m_lastc = -1;
  }
  if (hasPutArea)
  {
    self->m_epptr = buffer + putEnd;
    self->m_pbase = buffer + putBegin;
    self->m_pptr = buffer + putPosition;
  }
  return 1;
}

streambuf* strstreambuf_setbuf(strstreambuf* self, char*, int size)
{
  if (size != 0)
    self->m_nextSize = size;
  return self;
}

int strstreambuf_overflow(strstreambuf* self, int c)
{
  if (!self->m_epptr || !self->m_pptr || self->m_epptr <= self->m_pptr)
  {
    if (self->m_dynamic == 0)
      return -1;
    if (strstreambuf_doallocate(self) == -1)
      return -1;
    if (self->m_epptr == nullptr)
    {
      char* put = self->m_base + RangeLength(self->m_eback, self->m_egptr);
      self->m_pbase = put;
      self->m_pptr = put;
      self->m_epptr = self->m_ebuf;
    }
    else
    {
      char* oldBase = self->m_pbase;
      char* oldPptr = self->m_pptr;
      self->m_pptr = oldBase;
      self->m_epptr = self->m_ebuf;
      if (self->m_ebuf != nullptr)
        self->m_pptr = oldBase + (oldPptr - oldBase);
    }
  }
  if (c != -1)
  {
    *self->m_pptr = static_cast<char>(c);
    if (self->m_epptr != nullptr)
      ++self->m_pptr;
  }
  return 1;
}

int strstreambuf_underflow(strstreambuf* self)
{
  const unsigned char* get = reinterpret_cast<unsigned char*>(self->m_gptr);
  if (!self->m_egptr || !self->m_gptr || self->m_egptr <= self->m_gptr)
  {
    if (self->m_pptr && (!self->m_egptr || self->m_egptr < self->m_pptr))
    {
      const ptrdiff_t getOffset = self->m_gptr && self->m_eback ? self->m_gptr - self->m_eback : 0;
      self->m_eback = self->m_base;
      self->m_egptr = self->m_pptr;
      self->m_lastc = -1;
      self->m_gptr = self->m_base + getOffset;
    }
    get = reinterpret_cast<unsigned char*>(self->m_gptr);
    if (!self->m_egptr || !self->m_gptr || self->m_egptr <= self->m_gptr)
      return -1;
  }
  return *get;
}

int strstreambuf_seekoff(strstreambuf* self, long offset, int dir, int mode)
{
  int result = -1;
  const auto position = [&](char* begin, char* current, char* end) -> long
  {
    if (!begin || !end)
      return -1;
    long origin;
    if (dir == iostream_flags::kSeekBeg)
      origin = 0;
    else if (dir == iostream_flags::kSeekCur && current)
      origin = current - begin;
    else if (dir == iostream_flags::kSeekEnd)
      origin = end - begin;
    else
      return -1;
    if (offset < -origin || offset > std::numeric_limits<int>::max() - origin)
      return -1;
    return origin + offset;
  };
  if (mode & iostream_flags::kIn)
  {
    strstreambuf_underflow(self);
    const long target = position(self->m_eback, self->m_gptr, self->m_egptr);
    if (target < 0 || target > self->m_egptr - self->m_eback)
      return -1;
    self->m_gptr = self->m_eback + target;
    result = static_cast<int>(target);
  }
  if (mode & iostream_flags::kOut)
  {
    if (!self->m_epptr && strstreambuf_overflow(self, -1) == -1)
      return -1;
    const long target = position(self->m_pbase, self->m_pptr, self->m_epptr);
    if (target < 0)
      return -1;
    if (target > self->m_epptr - self->m_pbase)
    {
      if (!self->m_dynamic)
        return -1;
      self->m_nextSize = std::max(self->m_nextSize, static_cast<int>(target));
      if (strstreambuf_doallocate(self) == -1)
        return -1;
      self->m_epptr = self->m_ebuf;
    }
    self->m_pptr = self->m_pbase + target;
    result = static_cast<int>(target);
  }
  return result;
}

strstreambuf::strstreambuf()
{
  strstreambuf_ctor(this);
}
strstreambuf::~strstreambuf()
{
  strstreambuf_dtor(this);
}
int strstreambuf::Sync()
{
  return 0;
}
streambuf* strstreambuf::SetBuffer(char* buffer, int size)
{
  return strstreambuf_setbuf(this, buffer, size);
}
int strstreambuf::SeekOffset(long offset, int dir, int mode)
{
  return strstreambuf_seekoff(this, offset, dir, mode);
}
int strstreambuf::Overflow(int c)
{
  return strstreambuf_overflow(this, c);
}
int strstreambuf::Underflow()
{
  return strstreambuf_underflow(this);
}
int strstreambuf::Putback(int c)
{
  return streambuf_pbackfail(this, c);
}
int strstreambuf::Allocate()
{
  return strstreambuf_doallocate(this);
}
