#pragma once
#include <cstdint>

class istream;
class ostream;

// A strstream living on the caller's stack, as the original game code builds them: the object holds the istream
// part at +0x00, the ostream part at +0x0c and the virtual base ios at +0x14 (which in turn points at a heap
// strstreambuf). The library functions below are the statically linked LIBCI ones, called by address.
struct StrStream {
    alignas(4) uint8_t bytes[0x64];

    ostream* Out() { return reinterpret_cast<ostream*>(bytes + 0x0c); }
    void* Ios() { return bytes + 0x14; }
};

StrStream* __thiscall strstream_ctor(StrStream* stream, int constructVirtualBase);
void __thiscall strstream_dtor(void* ios);
void __thiscall ios_dtor(void* ios);
ostream* __thiscall ostream_operator_ls_cstr(ostream* out, const char* text);
ostream* __thiscall ostream_operator_ls_char(ostream* out, char c);
ostream* __thiscall ostream_seekp_pos(ostream* out, long position);
char* __thiscall strstreambuf_str(void* streambuf);
void __thiscall strstreambuf_freeze(void* streambuf, int freeze);
void __cdecl mtx_lock(void* lock);
void __cdecl mtx_unlock(void* lock);

// The virtual-base ios part of any strstream-derived stream, found through the first vbtable slot.
inline char* IosPartOf(void* stream) {
    const int* vbtable = *static_cast<int**>(stream);
    return static_cast<char*>(stream) + vbtable[1];
}

inline void* StreambufOf(void* stream) { return *reinterpret_cast<void**>(IosPartOf(stream) + 4); }

// Inline ios::clear() as the original compiled it: the state word (+8) is zeroed under the ios lock.
inline void ClearIosState(void* stream) {
    char* ios = IosPartOf(stream);
    if (*reinterpret_cast<int*>(ios + 0x34) < 0) mtx_lock(ios + 0x38);
    const int lockCount = *reinterpret_cast<int*>(ios + 0x34);
    *reinterpret_cast<int*>(ios + 8) = 0;
    if (lockCount < 0) mtx_unlock(ios + 0x38);
}

// The two library string streams that the multiplayer code embeds by value in its objects, constructed and destroyed
// through the library by address: an ostrstream with a dynamic buffer (0x58 bytes: ostream at +0x00, the virtual-base
// ios at +0x08) and an istrstream over a caller's buffer (0x5c bytes: istream at +0x00, ios at +0x0c).
struct OStrStream {
    alignas(4) uint8_t bytes[0x58];

    ostream* Out() { return reinterpret_cast<ostream*>(bytes); }
    void* Ios() { return bytes + 0x08; }
};

struct IStrStream {
    alignas(4) uint8_t bytes[0x5c];

    istream* In() { return reinterpret_cast<istream*>(bytes); }
    void* Ios() { return bytes + 0x0c; }
};

OStrStream* __thiscall ostrstream_ctor(OStrStream* stream, int constructVirtualBase);
void __thiscall ostrstream_dtor(void* ios);
IStrStream* __thiscall istrstream_ctor_str(IStrStream* stream, char* text, int constructVirtualBase);
IStrStream* __thiscall istrstream_ctor_str_len(IStrStream* stream, char* text, int length, int constructVirtualBase);
void __thiscall istrstream_dtor(void* ios);
OStrStream* __thiscall ostrstream_ctor_buf(OStrStream* stream, char* buffer, int length, int mode, int constructVirtualBase);
