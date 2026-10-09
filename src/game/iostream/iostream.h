#pragma once
#include <cstdint>

#include "core/layout.h"

// The statically linked iostream library (LIBCI, the pre-standard Microsoft iostreams shipped with Visual C++ 5), as
// reconstructed in src/game/iostream. This is the only declaration of the library: game code includes it and calls the
// entry points below, which are hooked over the original addresses.
//
// The class templates the library was written in (virtual base `ios`, virtual functions in `streambuf`) are modelled as
// plain layout structs plus free functions named as in symbols/library_functions.csv, so every object has exactly the
// original layout and virtual calls go through the original vftables in the EXE. A pointer to an istream/ostream/
// strstream/... addresses the object's own part (its vbptr); the shared `ios` lives at the offset vbtable[1] names,
// and a destructor entry point is called with the address of the `ios` base (ECX in the original).

class ios;
class streambuf;
class istream;
class ostream;
class iostream;
class strstreambuf;
class filebuf;
class istrstream;
class ostrstream;
class strstream;
class ifstream;
class ofstream;

namespace iostream_flags {
// ios::state
constexpr int kBad = 4;
constexpr int kFail = 2;
constexpr int kEof = 1;
// ios::fmtflags
constexpr unsigned kSkipWs = 0x0001;
constexpr unsigned kLeft = 0x0002;
constexpr unsigned kRight = 0x0004;
constexpr unsigned kInternal = 0x0008;
constexpr unsigned kDec = 0x0010;
constexpr unsigned kOct = 0x0020;
constexpr unsigned kHex = 0x0040;
constexpr unsigned kShowBase = 0x0080;
constexpr unsigned kShowPoint = 0x0100;
constexpr unsigned kUpperCase = 0x0200;
constexpr unsigned kShowPos = 0x0400;
constexpr unsigned kUnitBuf = 0x2000;
constexpr unsigned kStdio = 0x4000;
// ios::seek_dir
constexpr int kSeekBeg = 0;
constexpr int kSeekCur = 1;
constexpr int kSeekEnd = 2;
// ios::open_mode
constexpr int kIn = 1;
constexpr int kOut = 2;
constexpr int kAte = 4;
constexpr int kApp = 8;
constexpr int kTrunc = 0x10;
}  // namespace iostream_flags

typedef int (__thiscall* IosScalarDeletingDtor)(void*, unsigned);

// ios vftable (one slot, the scalar deleting destructor).
struct IosVtable {
    IosScalarDeletingDtor m_scalarDeletingDtor;  // [0]
};

// streambuf vftable. The methods the library reaches virtually are called through these slots.
struct StreambufVtable {
    IosScalarDeletingDtor m_scalarDeletingDtor;                  // [0]
    int(__thiscall* m_sync)(streambuf*);                          // [1]
    streambuf*(__thiscall* m_setbuf)(streambuf*, char*, int);     // [2]
    int(__thiscall* m_seekoff)(streambuf*, long, int, int);       // [3]
    int(__thiscall* m_seekpos)(streambuf*, long, int);            // [4]
    int(__thiscall* m_xsputn)(streambuf*, const char*, int);      // [5]
    int(__thiscall* m_xsgetn)(streambuf*, char*, int);            // [6]
    int(__thiscall* m_overflow)(streambuf*, int);                 // [7]
    int(__thiscall* m_underflow)(streambuf*);                     // [8]
    int(__thiscall* m_pbackfail)(streambuf*, int);                // [9]
    int(__thiscall* m_doallocate)(streambuf*);                    // [10]
};

// ios (0x50 bytes): the virtual base of every stream.
class ios {
public:
    const IosVtable* m_vftable;  // +0x00
    streambuf* m_bp;             // +0x04
    int m_state;                 // +0x08
    int m_ispecial;              // +0x0c
    int m_ospecial;              // +0x10
    int m_isfxSpecial;           // +0x14 never initialised by the constructor
    int m_osfxSpecial;           // +0x18 never initialised by the constructor
    int m_delbuf;                // +0x1c the ios deletes its streambuf
    ostream* m_tie;              // +0x20
    unsigned m_flags;            // +0x24
    int m_precision;             // +0x28
    char m_fill;                 // +0x2c
    uint8_t m_pad2d[3];          // +0x2d
    int m_width;                 // +0x30
    int m_lockCount;             // +0x34 negative while locking is enabled (the library's x_lockc)
    uint8_t m_lock[0x18];        // +0x38 CRITICAL_SECTION

    static const IosVtable vftable;  // 0x66e4dc
};

// streambuf (0x4c bytes). The put area comes first, then the get area.
class streambuf {
public:
    const StreambufVtable* m_vftable;  // +0x00
    int m_allocated;                   // +0x04 the buffer [base, ebuf) belongs to the streambuf
    int m_unbuffered;                  // +0x08
    int m_lastc;                       // +0x0c pushed-back character of an unbuffered streambuf
    char* m_base;                      // +0x10
    char* m_ebuf;                      // +0x14
    char* m_pbase;                     // +0x18
    char* m_pptr;                      // +0x1c
    char* m_epptr;                     // +0x20
    char* m_eback;                     // +0x24
    char* m_gptr;                      // +0x28
    char* m_egptr;                     // +0x2c
    int m_lockCount;                   // +0x30
    uint8_t m_lock[0x18];              // +0x34 CRITICAL_SECTION

    static const StreambufVtable vftable;  // 0x66e5e4
};

// strstreambuf (0x64 bytes).
class strstreambuf : public streambuf {
public:
    int m_dynamic;       // +0x4c the buffer may be grown and is freed by the destructor (unless frozen)
    int m_nextSize;      // +0x50 size of the next allocation
    uint8_t m_pad54[4];  // +0x54 never initialised
    int m_static;        // +0x58 the buffer was supplied by the caller
    void*(__cdecl* m_allocate)(size_t);  // +0x5c
    void(__cdecl* m_free)(void*);        // +0x60

    static const StreambufVtable vftable;  // 0x66e4e4
};

// filebuf (0x54 bytes).
class filebuf : public streambuf {
public:
    int m_fd;           // +0x4c file descriptor, -1 while closed
    int m_open;         // +0x50 a file was opened through this filebuf

    static const StreambufVtable vftable;  // 0x66e59c
};

// istream part (0x0c bytes): the vbptr, the counter istream::getline bumps (_fGline) and gcount().
class istream {
public:
    const int* m_vbptr;  // +0x00
    int m_fGline;        // +0x04
    int m_gcount;        // +0x08

    static const IosVtable vftable;  // 0x66e5d4 (the ios part's vftable while an istream is constructed)
    static const int vbtable[2];     // 0x66e5d8
};

// ostream part (8 bytes).
class ostream {
public:
    const int* m_vbptr;  // +0x00
    int m_floatUsed;     // +0x04

    static const IosVtable vftable;  // 0x66e54c
    static const int vbtable[2];     // 0x66e550
};

// iostream (0x64 bytes): istream at +0, ostream at +0x0c, ios at +0x14.
class iostream {
public:
    static constexpr int kOstreamOffset = 0x0c;
    static constexpr int kIosOffset = 0x14;
    uint8_t m_bytes[0x64];

    static const IosVtable vftable;     // 0x66e614
    static const int vbtableIstream[2];  // 0x66e620
    static const int vbtableOstream[2];  // 0x66e618
};

// istrstream (0x5c bytes): istream at +0, ios at +0x0c.
class istrstream {
public:
    istream* In() { return reinterpret_cast<istream*>(m_bytes); }
    ios* Ios() { return reinterpret_cast<ios*>(m_bytes + kIosOffset); }
    static constexpr int kIosOffset = 0x0c;
    uint8_t m_bytes[0x5c];

    static const IosVtable vftable;  // 0x66e514
    static const int vbtable[2];     // 0x66e518
};

// ostrstream (0x58 bytes): ostream at +0, ios at +8.
class ostrstream {
public:
    ostream* Out() { return reinterpret_cast<ostream*>(m_bytes); }
    ios* Ios() { return reinterpret_cast<ios*>(m_bytes + kIosOffset); }
    static constexpr int kIosOffset = 0x08;
    uint8_t m_bytes[0x58];

    static const IosVtable vftable;  // 0x66e524
    static const int vbtable[2];     // 0x66e528
};

// strstream (0x64 bytes): istream at +0, ostream at +0x0c, ios at +0x14.
class strstream {
public:
    ostream* Out() { return reinterpret_cast<ostream*>(m_bytes + kOstreamOffset); }
    ios* Ios() { return reinterpret_cast<ios*>(m_bytes + kIosOffset); }
    static constexpr int kOstreamOffset = 0x0c;
    static constexpr int kIosOffset = 0x14;
    static constexpr int kSize = 0x64;
    uint8_t m_bytes[kSize];

    static const IosVtable vftable;      // 0x66e534
    static const int vbtableIstream[2];  // 0x66e540
    static const int vbtableOstream[2];  // 0x66e538
};

// ifstream (0x5c bytes): istream at +0, ios at +0x0c.
class ifstream {
public:
    static constexpr int kIosOffset = 0x0c;
    static constexpr int kSize = 0x5c;
    uint8_t m_bytes[kSize];

    static const IosVtable vftable;  // 0x66e56c
    static const int vbtable[2];     // 0x66e570
};

// ofstream (0x58 bytes): ostream at +0, ios at +8.
class ofstream {
public:
    static constexpr int kIosOffset = 0x08;
    static constexpr int kSize = 0x58;
    uint8_t m_bytes[kSize];

    static const IosVtable vftable;  // 0x66e55c
    static const int vbtable[2];     // 0x66e560
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ios, 0x50);
REB_ASSERT_SIZE(streambuf, 0x4c);
REB_ASSERT_SIZE(strstreambuf, 0x64);
REB_ASSERT_SIZE(filebuf, 0x54);
REB_ASSERT_SIZE(istream, 0x0c);
REB_ASSERT_SIZE(ostream, 0x08);
REB_ASSERT_SIZE(strstream, 0x64);
REB_ASSERT_SIZE(istrstream, 0x5c);
REB_ASSERT_SIZE(ostrstream, 0x58);
REB_ASSERT_SIZE(ifstream, 0x5c);
REB_ASSERT_SIZE(ofstream, 0x58);
REB_ASSERT_OFFSET(ios, m_bp, 0x04);
REB_ASSERT_OFFSET(ios, m_state, 0x08);
REB_ASSERT_OFFSET(ios, m_tie, 0x20);
REB_ASSERT_OFFSET(ios, m_flags, 0x24);
REB_ASSERT_OFFSET(ios, m_fill, 0x2c);
REB_ASSERT_OFFSET(ios, m_width, 0x30);
REB_ASSERT_OFFSET(ios, m_lockCount, 0x34);
REB_ASSERT_OFFSET(ios, m_lock, 0x38);
REB_ASSERT_OFFSET(streambuf, m_base, 0x10);
REB_ASSERT_OFFSET(streambuf, m_pbase, 0x18);
REB_ASSERT_OFFSET(streambuf, m_eback, 0x24);
REB_ASSERT_OFFSET(streambuf, m_egptr, 0x2c);
REB_ASSERT_OFFSET(streambuf, m_lockCount, 0x30);
REB_ASSERT_OFFSET(streambuf, m_lock, 0x34);
REB_ASSERT_OFFSET(strstreambuf, m_dynamic, 0x4c);
REB_ASSERT_OFFSET(strstreambuf, m_nextSize, 0x50);
REB_ASSERT_OFFSET(strstreambuf, m_static, 0x58);
REB_ASSERT_OFFSET(strstreambuf, m_allocate, 0x5c);
REB_ASSERT_OFFSET(strstreambuf, m_free, 0x60);
REB_ASSERT_OFFSET(filebuf, m_fd, 0x4c);
#pragma clang diagnostic pop

// ---- ios -------------------------------------------------------------------------------------------------------
ios* __thiscall ios_ctor(ios* self);                       // 0x6153b0
void __thiscall ios_dtor(ios* self);                       // 0x615440
void __thiscall ios_init(ios* self, streambuf* buffer);    // 0x6154a0
void __cdecl mtx_init(void* criticalSection);              // 0x615ec0
void __cdecl mtx_term(void* criticalSection);              // 0x615ed0
void __cdecl mtx_lock(void* criticalSection);              // 0x615ee0
void __cdecl mtx_unlock(void* criticalSection);            // 0x615ef0

// The out-of-line copies of ios::lock(), unlock(), unsetf() and setf() the game's own code carries (0x4f90b0-0x4f9130).
void __cdecl ios_lock(ios* stream);
void __cdecl ios_unlock(ios* stream);
ios* __cdecl ios_unsetf(ios* stream, unsigned mask);
ios* __cdecl ios_setf(ios* stream, unsigned mask);

// ---- streambuf -------------------------------------------------------------------------------------------------
streambuf* __thiscall streambuf_ctor(streambuf* self);                             // 0x617d60
void __thiscall streambuf_dtor(streambuf* self);                                   // 0x617dd0
int __thiscall streambuf_sync(streambuf* self);                                    // 0x617fa0
streambuf* __thiscall streambuf_setbuf(streambuf* self, char* buffer, int size);   // 0x617e10
int __thiscall streambuf_seekoff(streambuf* self, long offset, int dir, int mode);  // 0x618060
int __thiscall streambuf_seekpos(streambuf* self, long position, int mode);        // 0x618070
int __thiscall streambuf_xsputn(streambuf* self, const char* text, int count);     // 0x617e50
int __thiscall streambuf_xsgetn(streambuf* self, char* buffer, int count);         // 0x617eb0
int __thiscall streambuf_pbackfail(streambuf* self, int c);                        // 0x618090
int __thiscall streambuf_doallocate(streambuf* self);                              // 0x617fe0
int __thiscall streambuf_allocate(streambuf* self);                                // 0x617fc0
void __thiscall streambuf_setb(streambuf* self, char* begin, char* end, int owned);  // 0x618020
int __thiscall streambuf_snextc(streambuf* self);                                  // 0x6183e0
int __thiscall streambuf_sbumpc(streambuf* self);                                  // 0x618440
void __thiscall streambuf_stossc(streambuf* self);                                 // 0x618490
int __thiscall streambuf_sgetc(streambuf* self);                                   // 0x6184e0

// ---- strstreambuf ----------------------------------------------------------------------------------------------
strstreambuf* __thiscall strstreambuf_ctor(strstreambuf* self);                                            // 0x6154e0
strstreambuf* __thiscall strstreambuf_ctor_buf(strstreambuf* self, char* buffer, int size, char* start);  // 0x615530
void __thiscall strstreambuf_dtor(strstreambuf* self);                                                     // 0x6155f0
void __thiscall strstreambuf_freeze(streambuf* self, int frozen);                                          // 0x615660
char* __thiscall strstreambuf_str(streambuf* self);                                                        // 0x615680
int __thiscall strstreambuf_doallocate(strstreambuf* self);                                                // 0x615690
streambuf* __thiscall strstreambuf_setbuf(strstreambuf* self, char* buffer, int size);                     // 0x6157c0
int __thiscall strstreambuf_overflow(strstreambuf* self, int c);                                           // 0x6157d0
int __thiscall strstreambuf_underflow(strstreambuf* self);                                                 // 0x615860
int __thiscall strstreambuf_seekoff(strstreambuf* self, long offset, int dir, int mode);                   // 0x6158c0

// ---- istream / ostream / iostream construction -----------------------------------------------------------------
istream* __thiscall istream_ctor(istream* self, streambuf* buffer, int constructVirtualBase);   // 0x6179e0
void __thiscall istream_dtor(void* istreamEnd);                                                 // 0x617a80: ECX = istream part + 0xc
ostream* __thiscall ostream_ctor(ostream* self, streambuf* buffer, int constructVirtualBase);   // 0x616210
void __thiscall ostream_dtor(void* ostreamEnd);                                                // 0x6162a0: ECX = ostream part + 8
iostream* __thiscall iostream_ctor(iostream* self, streambuf* buffer, int constructVirtualBase);  // 0x618130
void __thiscall iostream_dtor(ios* iosBase);                                                    // 0x6181d0

istrstream* __thiscall istrstream_ctor_str(istrstream* self, char* text, int constructVirtualBase);                // 0x6159e0
istrstream* __thiscall istrstream_ctor_str_len(istrstream* self, char* text, int length, int constructVirtualBase);  // 0x615ad0
void __thiscall istrstream_dtor(ios* iosBase);                                                                      // 0x615b90
ostrstream* __thiscall ostrstream_ctor(ostrstream* self, int constructVirtualBase);                                 // 0x615bb0
ostrstream* __thiscall ostrstream_ctor_buf(ostrstream* self, char* buffer, int length, int mode, int constructVirtualBase);  // 0x615c90
void __thiscall ostrstream_vbase_dtor(ios* iosBase);                                                                // 0x615d70
void __thiscall ostrstream_dtor(ios* iosBase);                                                                      // 0x615d90
strstream* __thiscall strstream_ctor(strstream* self, int constructVirtualBase);                                    // 0x615db0
void __thiscall strstream_dtor(ios* iosBase);                                                                       // 0x615ea0

// ---- ostream ---------------------------------------------------------------------------------------------------
ostream* __thiscall ostream_operator_ls_char(ostream* out, char value);                   // 0x615f00
int __thiscall ostream_opfx(ostream* out);                                                // 0x615fc0
void __thiscall ostream_osfx(ostream* out);                                               // 0x616040
ostream* __thiscall ostream_operator_ls_cstr(ostream* out, const char* text);             // 0x616110
ostream* __thiscall ostream_flush(ostream* out);                                          // 0x616140
void __thiscall ostream_outstr(ostream* out, const char* prefix, const char* text);       // 0x6162b0
ostream* __thiscall ostream_seekp_pos(ostream* out, long position);                       // 0x616900
ostream* __thiscall ostream_seekp_off(ostream* out, long offset, int dir);                // 0x6169a0
long __thiscall ostream_tellp(ostream* out);                                              // 0x616a40
ostream* __thiscall ostream_operator_ls_streambuf(ostream* out, streambuf* buffer);       // 0x616ae0
ostream* __thiscall ostream_operator_ls_long(ostream* out, long value);                   // 0x616e10
ostream* __thiscall ostream_write(ostream* out, const void* buffer, int count);           // 0x617430
ostream* __thiscall ostream_operator_ls_ulong(ostream* out, unsigned long value);         // 0x617480
ostream* __thiscall ostream_operator_ls_int(ostream* out, int value);                     // 0x617540

// ---- istream ---------------------------------------------------------------------------------------------------
istream* __thiscall istream_operator_rs_streambuf(istream* in, streambuf* buffer);        // 0x616b70
istream* __thiscall istream_seekg_pos(istream* in, long position);                        // 0x616c30
istream* __thiscall istream_seekg_off(istream* in, long offset, int dir);                 // 0x616cd0
long __thiscall istream_tellg(istream* in);                                               // 0x616d70
int __thiscall istream_get(istream* in);                                                  // 0x617330
istream* __thiscall istream_read(istream* in, void* buffer, int count);                   // 0x6173b0
istream* __thiscall istream_operator_rs_long(istream* in, int* value);                    // 0x617610
istream* __thiscall istream_operator_rs_ushort(istream* in, unsigned short* value);       // 0x6176a0
istream* __thiscall istream_operator_rs_ulong(istream* in, unsigned int* value);          // 0x617760
istream* __thiscall istream_getline(istream* in, char* buffer, int limit, int delimiter);  // 0x617800
istream* __thiscall istream_operator_rs_short(istream* in, short* value);                 // 0x617900
int __thiscall istream_ipfx(istream* in, int need);                                       // 0x617a90
istream* __thiscall istream_operator_rs_charptr(istream* in, char* value);                // 0x617b90
void __thiscall istream_eatwhite(istream* in);                                            // 0x617cb0
int __thiscall istream_getint(istream* in, char* buffer);                                 // 0x618510

// ---- filebuf / fstream -----------------------------------------------------------------------------------------
filebuf* __thiscall filebuf_ctor(filebuf* self);                                           // 0x616ed0
void __thiscall filebuf_dtor(filebuf* self);                                               // 0x616f10
filebuf* __thiscall filebuf_close(filebuf* self);                                          // 0x616f90
int __thiscall filebuf_overflow(filebuf* self, int c);                                     // 0x617010
int __thiscall filebuf_underflow(filebuf* self);                                           // 0x6170a0
int __thiscall filebuf_seekoff(filebuf* self, long offset, int dir, int mode);             // 0x617160
int __thiscall filebuf_sync(filebuf* self);                                                // 0x6171b0
streambuf* __thiscall filebuf_setbuf(filebuf* self, char* buffer, int size);               // 0x6172b0
filebuf* __thiscall filebuf_open(filebuf* self, const char* name, int mode, int protection);  // 0x618240

ofstream* __thiscall ofstream_ctor(ofstream* self, int constructVirtualBase);              // 0x6164b0
void __thiscall ofstream_dtor(ios* iosBase);                                               // 0x616590
void __thiscall ofstream_open(ofstream* self, const char* name, int mode, int protection);  // 0x6165b0
ifstream* __thiscall ifstream_ctor(ifstream* self, int constructVirtualBase);              // 0x616620
ifstream* __thiscall ifstream_ctor_open(ifstream* self, const char* name, int mode, int protection, int constructVirtualBase);  // 0x616700
void __thiscall ifstream_vbase_dtor(ios* iosBase);                                         // 0x6167f0
void __thiscall ifstream_dtor(ios* iosBase);                                               // 0x616810
void __thiscall ifstream_open(ifstream* self, const char* name, int mode, int protection);  // 0x616830
void __thiscall fstreambase_close(void* stream);                                           // 0x6168a0

// ---- helpers the game's inlined iostream code and the library share --------------------------------------------
namespace iostream_access {

// The virtual-base ios part of any stream, found through the first vbtable slot.
inline ios* IosOf(const void* stream) {
    const char* base = static_cast<const char*>(stream);
    const int* vbtable = *reinterpret_cast<const int* const*>(base);
    return const_cast<ios*>(reinterpret_cast<const ios*>(base + vbtable[1]));
}

inline int State(const void* stream) { return IosOf(stream)->m_state; }

inline streambuf* BufferOf(const void* stream) { return IosOf(stream)->m_bp; }

// The inlined ios::lock()/unlock() pair guards every access to the state with the ios critical section once the
// lock has been initialised.
inline void Lock(ios* stream) {
    if (stream->m_lockCount < 0) mtx_lock(stream->m_lock);
}
inline void Unlock(ios* stream) {
    if (stream->m_lockCount < 0) mtx_unlock(stream->m_lock);
}

// ios::clear(state) as the original compiled it.
inline void Clear(ios* stream, int state) {
    Lock(stream);
    stream->m_state = state;
    Unlock(stream);
}

// ios::setstate(bits): the new state is computed before the lock is taken.
inline void SetState(ios* stream, int bits) { Clear(stream, stream->m_state | bits); }

inline void LockBuffer(streambuf* buffer) {
    if (buffer->m_lockCount < 0) mtx_lock(buffer->m_lock);
}
inline void UnlockBuffer(streambuf* buffer) {
    if (buffer->m_lockCount < 0) mtx_unlock(buffer->m_lock);
}

// The counter at +4 that the inlined istream::getline() bumps around its call.
inline int& GetlineCount(strstream* stream) { return *reinterpret_cast<int*>(stream->m_bytes + 4); }

inline istream* AsIstream(strstream* stream) { return reinterpret_cast<istream*>(stream); }
inline ostream* AsOstream(strstream* stream) {
    return reinterpret_cast<ostream*>(reinterpret_cast<char*>(stream) + strstream::kOstreamOffset);
}

// Bytes in the put area of a streambuf (pptr - pbase, 0 if pptr is below pbase).
inline unsigned WrittenBytes(const streambuf* buffer) {
    return buffer->m_pptr < buffer->m_pbase ? 0u : static_cast<unsigned>(buffer->m_pptr - buffer->m_pbase);
}

}  // namespace iostream_access
