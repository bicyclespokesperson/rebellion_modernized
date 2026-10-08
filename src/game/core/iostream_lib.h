#pragma once
#include <cstdint>

#include "core/layout.h"

// The statically linked iostream library (LIBCI) as the game's own code uses it. The library is called by address;
// these declarations describe only the entry points and the object layouts that game code touches directly.
//
// MSVC iostreams use virtual inheritance: an istream/ostream/strstream pointer addresses the vbptr, and the
// shared `ios` base lives at the offset stored at vbtable[1]. The game inlines ios::lock()/unlock()/clear() and
// reads ios::state straight through that offset, so the helpers below do the same.

class ios;
class streambuf;
class istream;
class ostream;
class strstream;

// ios (0x50 bytes).
class ios {
public:
    void* m_vftable;       // +0x00
    streambuf* m_bp;       // +0x04
    int m_state;           // +0x08
    uint8_t m_pad0c[0x28];  // +0x0c
    int m_lockCount;       // +0x34, negative while the lock is in use (the library's x_lockc)
    uint8_t m_lock[0x18];  // +0x38, CRITICAL_SECTION
};

// streambuf: only the get area the game inspects.
class streambuf {
public:
    uint8_t m_pad00[0x18];
    const char* m_gptr;  // +0x18
    const char* m_egptr;  // +0x1c
};

// strstream (100 bytes): istream at +0, ostream at +0x0c, ios virtual base at +0x14.
class strstream {
public:
    static constexpr int kOstreamOffset = 0x0c;
    static constexpr int kIosOffset = 0x14;
    static constexpr int kSize = 100;
    uint8_t m_bytes[kSize];
};

// ifstream (0x5c bytes): the ios virtual base is at +0x0c.
class ifstream {
public:
    static constexpr int kIosOffset = 0x0c;
    static constexpr int kSize = 0x5c;
    uint8_t m_bytes[kSize];
};

// ofstream (0x58 bytes): ostream at +0, the ios virtual base at +8.
class ofstream {
public:
    static constexpr int kIosOffset = 0x08;
    static constexpr int kSize = 0x58;
    uint8_t m_bytes[kSize];
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ios, 0x50);
REB_ASSERT_SIZE(ofstream, 0x58);
REB_ASSERT_SIZE(ifstream, 0x5c);
REB_ASSERT_OFFSET(ios, m_bp, 0x04);
REB_ASSERT_OFFSET(ios, m_state, 0x08);
REB_ASSERT_OFFSET(ios, m_lockCount, 0x34);
REB_ASSERT_OFFSET(ios, m_lock, 0x38);
REB_ASSERT_OFFSET(streambuf, m_gptr, 0x18);
REB_ASSERT_OFFSET(streambuf, m_egptr, 0x1c);
REB_ASSERT_SIZE(strstream, 100);
#pragma clang diagnostic pop

// Library entry points (names from symbols/library_functions.csv). Stream arguments are the object pointer the
// original passes in ECX; `strstream_dtor` and `ios_dtor` take the address of the ios virtual base.
strstream* __thiscall strstream_ctor(strstream* self, int isMostDerived);
void __thiscall strstream_dtor(ios* iosBase);
void __thiscall ios_dtor(ios* self);
ofstream* __thiscall ofstream_ctor(ofstream* self, int isMostDerived);
void __thiscall ofstream_dtor(ios* iosBase);
ifstream* __thiscall ifstream_ctor(ifstream* self, int isMostDerived);
void __thiscall ifstream_dtor(ios* iosBase);

// The out-of-line copies of ios::lock(), unlock(), unsetf() and setf() the game's own code carries (0x4f90b0-0x4f9130).
void __cdecl ios_lock(ios* stream);
void __cdecl ios_unlock(ios* stream);
ios* __cdecl ios_unsetf(ios* stream, unsigned mask);
ios* __cdecl ios_setf(ios* stream, unsigned mask);

void __cdecl mtx_lock(void* criticalSection);
void __cdecl mtx_unlock(void* criticalSection);

int __thiscall istream_get(istream* in);
istream* __thiscall istream_read(istream* in, void* buffer, int count);
void __thiscall istream_getline(istream* in, char* buffer, int limit, char delimiter);
istream* __thiscall istream_operator_rs_long(istream* in, int* value);
istream* __thiscall istream_operator_rs_ulong(istream* in, unsigned int* value);
istream* __thiscall istream_operator_rs_short(istream* in, short* value);
istream* __thiscall istream_operator_rs_ushort(istream* in, unsigned short* value);
istream* __thiscall istream_operator_rs_charptr(istream* in, char* value);
istream* __thiscall istream_seekg_pos(istream* in, long position);

ostream* __thiscall ostream_operator_ls_char(ostream* out, char value);
ostream* __thiscall ostream_operator_ls_long(ostream* out, long value);
ostream* __thiscall ostream_operator_ls_cstr(ostream* out, const char* text);
ostream* __thiscall ostream_write(ostream* out, const void* buffer, int count);
ostream* __thiscall ostream_seekp_pos(ostream* out, long position);

char* __thiscall strstreambuf_str(streambuf* buffer);
void __thiscall strstreambuf_freeze(streambuf* buffer, int frozen);

namespace iostream_access {

inline ios* IosOf(void* stream) {
    char* base = static_cast<char*>(stream);
    const int* vbtable = *reinterpret_cast<int**>(base);
    return reinterpret_cast<ios*>(base + vbtable[1]);
}

// ios::state of a stream; non-zero once the stream has failed or reached the end.
inline int State(void* stream) { return IosOf(stream)->m_state; }

// The inlined ios::lock()/unlock() pair guards every access to the state with the ios critical section once the
// lock has been initialised.
inline void Lock(ios* stream) {
    if (stream->m_lockCount < 0) mtx_lock(stream->m_lock);
}
inline void Unlock(ios* stream) {
    if (stream->m_lockCount < 0) mtx_unlock(stream->m_lock);
}

// istream::_fGline, the counter at +4 that the inlined istream::getline() bumps around its call.
inline int& GetlineCount(strstream* stream) { return *reinterpret_cast<int*>(stream->m_bytes + 4); }

inline istream* AsIstream(strstream* stream) { return reinterpret_cast<istream*>(stream); }
inline ostream* AsOstream(strstream* stream) {
    return reinterpret_cast<ostream*>(reinterpret_cast<char*>(stream) + strstream::kOstreamOffset);
}

// Bytes left in a streambuf's get area (egptr - gptr, 0 if egptr is below gptr).
inline unsigned AvailableBytes(const streambuf* buffer) {
    return buffer->m_egptr < buffer->m_gptr ? 0u : static_cast<unsigned>(buffer->m_egptr - buffer->m_gptr);
}

inline streambuf* BufferOf(strstream* stream) { return IosOf(stream)->m_bp; }

}  // namespace iostream_access

// An ifstream on the stack, constructed and destroyed the way the compiler does it for a local object.
class ScopedIfstream {
public:
    ScopedIfstream() { ifstream_ctor(&m_stream, 1); }
    ~ScopedIfstream() {
        ios* base = reinterpret_cast<ios*>(m_stream.m_bytes + ifstream::kIosOffset);
        ifstream_dtor(base);
        ios_dtor(base);
    }
    ScopedIfstream(const ScopedIfstream&) = delete;
    ScopedIfstream& operator=(const ScopedIfstream&) = delete;

    ifstream* get() { return &m_stream; }
    // The file descriptor of the stream's filebuf: -1 if no file is open.
    int FileDescriptor() { return *reinterpret_cast<int*>(reinterpret_cast<char*>(iostream_access::IosOf(&m_stream)->m_bp) + 0x4c); }

private:
    ifstream m_stream;
};

// A strstream on the stack, constructed and destroyed the way the compiler does it for a local object.
class ScopedStrstream {
public:
    ScopedStrstream() { strstream_ctor(&m_stream, 1); }
    ~ScopedStrstream() {
        ios* base = reinterpret_cast<ios*>(m_stream.m_bytes + strstream::kIosOffset);
        strstream_dtor(base);
        ios_dtor(base);
    }
    ScopedStrstream(const ScopedStrstream&) = delete;
    ScopedStrstream& operator=(const ScopedStrstream&) = delete;

    strstream* get() { return &m_stream; }

private:
    strstream m_stream;
};

// An ofstream on the stack, constructed and destroyed the way the compiler does it for a local object.
class ScopedOfstream {
public:
    ScopedOfstream() { ofstream_ctor(&m_stream, 1); }
    ~ScopedOfstream() {
        ios* base = reinterpret_cast<ios*>(m_stream.m_bytes + ofstream::kIosOffset);
        ofstream_dtor(base);
        ios_dtor(base);
    }
    ScopedOfstream(const ScopedOfstream&) = delete;
    ScopedOfstream& operator=(const ScopedOfstream&) = delete;

    ofstream* get() { return &m_stream; }

private:
    ofstream m_stream;
};
