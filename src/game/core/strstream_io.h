#pragma once
#include <cstdint>

#include "iostream/iostream.h"

// The string streams as the original game code builds them: objects living on the caller's stack (or embedded by
// value in another object) that are constructed and destroyed through the iostream library entry points. A strstream
// holds the istream part at +0x00, the ostream part at +0x0c and the virtual base ios at +0x14 (which in turn
// points at a heap strstreambuf).
using StrStream = strstream;
using OStrStream = ostrstream;
using IStrStream = istrstream;

// Typed access to the ios and streambuf members owned by the stream object.
template <typename T>
inline ios* IosPartOf(T* stream)
{
  return iostream_access::IosOf(stream);
}

template <typename T>
inline streambuf* StreambufOf(T* stream)
{
  return iostream_access::BufferOf(stream);
}

// Inline ios::clear() as the original compiled it: the state word (+8) is zeroed under the ios lock.
template <typename T>
inline void ClearIosState(T* stream)
{
  iostream_access::Clear(iostream_access::IosOf(stream), 0);
}
