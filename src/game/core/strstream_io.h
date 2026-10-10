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

// The virtual-base ios part of any strstream-derived stream, found through the first vbtable slot.
inline char* IosPartOf(void* stream) { return reinterpret_cast<char*>(iostream_access::IosOf(stream)); }

inline streambuf* StreambufOf(void* stream) { return iostream_access::BufferOf(stream); }

// Inline ios::clear() as the original compiled it: the state word (+8) is zeroed under the ios lock.
inline void ClearIosState(void* stream) { iostream_access::Clear(iostream_access::IosOf(stream), 0); }
