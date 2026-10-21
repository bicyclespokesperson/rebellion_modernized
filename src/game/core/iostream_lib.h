#pragma once
#include <cstdint>

#include "iostream/iostream.h"

// Game-side helpers around the iostream library (declared in iostream/iostream.h).

// An ifstream on the stack, constructed and destroyed the way the compiler does it for a local object.
class ScopedIfstream
{
  public:
  ScopedIfstream()
  {
    ifstream_ctor(&m_stream, 1);
  }
  ~ScopedIfstream()
  {
    ios* base = m_stream.Ios();
    ifstream_dtor(base);
    ios_dtor(base);
  }
  ScopedIfstream(const ScopedIfstream&) = delete;
  ScopedIfstream& operator=(const ScopedIfstream&) = delete;

  ifstream* get()
  {
    return &m_stream;
  }
  // The file descriptor of the stream's filebuf: -1 if no file is open.
  int FileDescriptor()
  {
    auto* buffer = static_cast<filebuf*>(m_stream.Ios()->m_bp);
    return buffer ? buffer->m_fd : -1;
  }

  private:
  ifstream m_stream;
};

// A strstream on the stack, constructed and destroyed the way the compiler does it for a local object.
class ScopedStrstream
{
  public:
  ScopedStrstream()
  {
    strstream_ctor(&m_stream, 1);
  }
  ~ScopedStrstream()
  {
    ios* base = m_stream.Ios();
    strstream_dtor(base);
    ios_dtor(base);
  }
  ScopedStrstream(const ScopedStrstream&) = delete;
  ScopedStrstream& operator=(const ScopedStrstream&) = delete;

  strstream* get()
  {
    return &m_stream;
  }

  private:
  strstream m_stream;
};

// An ofstream on the stack, constructed and destroyed the way the compiler does it for a local object.
class ScopedOfstream
{
  public:
  ScopedOfstream()
  {
    ofstream_ctor(&m_stream, 1);
  }
  ~ScopedOfstream()
  {
    ios* base = m_stream.Ios();
    ofstream_dtor(base);
    ios_dtor(base);
  }
  ScopedOfstream(const ScopedOfstream&) = delete;
  ScopedOfstream& operator=(const ScopedOfstream&) = delete;

  ofstream* get()
  {
    return &m_stream;
  }

  private:
  ofstream m_stream;
};
