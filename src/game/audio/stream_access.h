#pragma once
#include <cstdint>

#include "core/stream_io.h"

// Statically linked iostream entry points the sound code uses beyond core/stream_io.h, plus direct access to the
// few ios fields it reads. The classes derive from ios through a virtual base: the first word of a stream object
// is its vbtable and vbtable[1] is the offset of the ios part, whose streambuf pointer (+4) leads to the filebuf
// (file descriptor at +0x4c); the ios state bits are at +8 (bit 0 = eofbit); istream keeps its last read count at +8.
istream* __thiscall istream_seekg_pos(istream* in, long position);                // 0x616c30
istream* __thiscall istream_seekg_off(istream* in, long offset, int direction);  // 0x616cd0 (0 = begin, 1 = current)
void __thiscall fstreambase_close(void* stream);                                  // 0x6168a0
ifstream* __thiscall ifstream_ctor_open(void* memory, const char* name, int mode, int protection, int mostDerived);  // 0x616700

namespace stream_access {

constexpr int kSeekBegin = 0;
constexpr int kSeekCurrent = 1;

inline char* IosPart(void* stream) {
    const int32_t* vbtable = *static_cast<int32_t**>(stream);
    return static_cast<char*>(stream) + vbtable[1];
}

inline bool IsOpen(void* stream) {
    char* filebuf = *reinterpret_cast<char**>(IosPart(stream) + 4);
    return *reinterpret_cast<int32_t*>(filebuf + 0x4c) != -1;
}

inline bool AtEof(void* stream) { return (*reinterpret_cast<uint8_t*>(IosPart(stream) + 8) & 1) != 0; }

inline uint32_t LastReadCount(istream* in) { return *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(in) + 8); }

// Calls the virtual deleting destructor of the ios part (slot 0), which frees the whole stream object.
inline void DeleteStream(void* stream) {
    char* ios = IosPart(stream);
    using Deleter = void*(__thiscall*)(void*, int);
    (*reinterpret_cast<Deleter**>(ios))[0](ios, 1);
}

inline void Read(istream* in, void* buffer, uint32_t count) { istream_read(in, buffer, static_cast<int>(count)); }

}  // namespace stream_access
