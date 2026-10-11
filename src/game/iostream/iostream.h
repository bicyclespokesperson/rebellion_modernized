#pragma once
#include "core/reb_abi.h"
#include <cstddef>
#include <cstdint>


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

// The CRITICAL_SECTION embedded in ios and streambuf: 0x18 bytes on 32-bit Windows, 0x28 for the 64-bit layout of minwin.
constexpr size_t kCriticalSectionBytes = sizeof(void*) == 4 ? 0x18 : 0x28;
constexpr size_t kPointerAlignment = alignof(void*);

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

typedef int (REB_THISCALL* IosScalarDeletingDtor)(void*, unsigned);

// ios vftable (one slot, the scalar deleting destructor).
struct IosVtable {
    IosScalarDeletingDtor m_scalarDeletingDtor;  // [0]
};

// streambuf vftable. The methods the library reaches virtually are called through these slots.
struct StreambufVtable {
    IosScalarDeletingDtor m_scalarDeletingDtor;                  // [0]
    int(REB_THISCALL* m_sync)(streambuf*);                          // [1]
    streambuf*(REB_THISCALL* m_setbuf)(streambuf*, char*, int);     // [2]
    int(REB_THISCALL* m_seekoff)(streambuf*, long, int, int);       // [3]
    int(REB_THISCALL* m_seekpos)(streambuf*, long, int);            // [4]
    int(REB_THISCALL* m_xsputn)(streambuf*, const char*, int);      // [5]
    int(REB_THISCALL* m_xsgetn)(streambuf*, char*, int);            // [6]
    int(REB_THISCALL* m_overflow)(streambuf*, int);                 // [7]
    int(REB_THISCALL* m_underflow)(streambuf*);                     // [8]
    int(REB_THISCALL* m_pbackfail)(streambuf*, int);                // [9]
    int(REB_THISCALL* m_doallocate)(streambuf*);                    // [10]
};

// ios (0x50 bytes): the virtual base of every stream.
class ios {
public:
    const IosVtable* m_vftable;  // +0x00
    streambuf* m_bp;             // +0x04
    int m_state;                 // +0x08
    int m_ispecial;              // +0x0c
    int m_ospecial;              // +0x10
    int m_isfxSpecial;           // +0x14
    int m_osfxSpecial;           // +0x18
    int m_delbuf;                // +0x1c the ios deletes its streambuf
    ostream* m_tie;              // +0x20
    unsigned m_flags;            // +0x24
    int m_precision;             // +0x28
    char m_fill;                 // +0x2c
    uint8_t m_pad2d[3];          // +0x2d
    int m_width;                 // +0x30
    int m_lockCount;             // +0x34 negative while locking is enabled (the library's x_lockc)
    alignas(kPointerAlignment) uint8_t m_lock[kCriticalSectionBytes];  // +0x38 CRITICAL_SECTION

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
    alignas(kPointerAlignment) uint8_t m_lock[kCriticalSectionBytes];  // +0x34 CRITICAL_SECTION

    static const StreambufVtable vftable;  // 0x66e5e4
};

// strstreambuf (0x64 bytes).
class strstreambuf : public streambuf {
public:
    int m_dynamic;       // +0x4c the buffer may be grown and is freed by the destructor (unless frozen)
    int m_nextSize;      // +0x50 size of the next allocation
    uint8_t m_pad54[4];  // +0x54 never initialised
    int m_static;        // +0x58 the buffer was supplied by the caller
    void*(REB_CDECL* m_allocate)(size_t);  // +0x5c
    void(REB_CDECL* m_free)(void*);        // +0x60

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

// The virtual base ios sits after the stream's own parts, at the next multiple of its alignment (the 32-bit values are the
// original's: istream 0x0c, ostream 8, ios 0x50).
struct StreamLayout {
    static constexpr int Align(size_t offset) { return static_cast<int>((offset + alignof(ios) - 1) / alignof(ios) * alignof(ios)); }
};

// iostream (0x64 bytes): istream at +0, ostream at +0x0c, ios at +0x14.
class iostream {
public:
    static constexpr int kOstreamOffset = sizeof(istream);
    static constexpr int kIosOffset = StreamLayout::Align(sizeof(istream) + sizeof(ostream));
    static constexpr int kSize = kIosOffset + sizeof(ios);
    alignas(ios) uint8_t m_bytes[kSize];

    static const IosVtable vftable;     // 0x66e614
    static const int vbtableIstream[2];  // 0x66e620
    static const int vbtableOstream[2];  // 0x66e618
};

// istrstream (0x5c bytes): istream at +0, ios at +0x0c.
class istrstream {
public:
    istream* In() { return reinterpret_cast<istream*>(m_bytes); }
    ios* Ios() { return reinterpret_cast<ios*>(m_bytes + kIosOffset); }
    static constexpr int kIosOffset = StreamLayout::Align(sizeof(istream));
    static constexpr int kSize = kIosOffset + sizeof(ios);
    alignas(ios) uint8_t m_bytes[kSize];

    static const IosVtable vftable;  // 0x66e514
    static const int vbtable[2];     // 0x66e518
};

// ostrstream (0x58 bytes): ostream at +0, ios at +8.
class ostrstream {
public:
    ostream* Out() { return reinterpret_cast<ostream*>(m_bytes); }
    ios* Ios() { return reinterpret_cast<ios*>(m_bytes + kIosOffset); }
    static constexpr int kIosOffset = StreamLayout::Align(sizeof(ostream));
    static constexpr int kSize = kIosOffset + sizeof(ios);
    alignas(ios) uint8_t m_bytes[kSize];

    static const IosVtable vftable;  // 0x66e524
    static const int vbtable[2];     // 0x66e528
};

// strstream (0x64 bytes): istream at +0, ostream at +0x0c, ios at +0x14.
class strstream {
public:
    ostream* Out() { return reinterpret_cast<ostream*>(m_bytes + kOstreamOffset); }
    ios* Ios() { return reinterpret_cast<ios*>(m_bytes + kIosOffset); }
    static constexpr int kOstreamOffset = sizeof(istream);
    static constexpr int kIosOffset = StreamLayout::Align(sizeof(istream) + sizeof(ostream));
    static constexpr int kSize = kIosOffset + sizeof(ios);
    alignas(ios) uint8_t m_bytes[kSize];

    static const IosVtable vftable;      // 0x66e534
    static const int vbtableIstream[2];  // 0x66e540
    static const int vbtableOstream[2];  // 0x66e538
};

// ifstream (0x5c bytes): istream at +0, ios at +0x0c.
class ifstream {
public:
    static constexpr int kIosOffset = StreamLayout::Align(sizeof(istream));
    static constexpr int kSize = kIosOffset + sizeof(ios);
    alignas(ios) uint8_t m_bytes[kSize];

    static const IosVtable vftable;  // 0x66e56c
    static const int vbtable[2];     // 0x66e570
};

// ofstream (0x58 bytes): ostream at +0, ios at +8.
class ofstream {
public:
    static constexpr int kIosOffset = StreamLayout::Align(sizeof(ostream));
    static constexpr int kSize = kIosOffset + sizeof(ios);
    alignas(ios) uint8_t m_bytes[kSize];

    static const IosVtable vftable;  // 0x66e55c
    static const int vbtable[2];     // 0x66e560
};


// ---- ios -------------------------------------------------------------------------------------------------------
ios* REB_THISCALL ios_ctor(ios* self);                       // 0x6153b0
void REB_THISCALL ios_dtor(ios* self);                       // 0x615440
void REB_THISCALL ios_init(ios* self, streambuf* buffer);    // 0x6154a0
void REB_CDECL mtx_init(void* criticalSection);              // 0x615ec0
void REB_CDECL mtx_term(void* criticalSection);              // 0x615ed0
void REB_CDECL mtx_lock(void* criticalSection);              // 0x615ee0
void REB_CDECL mtx_unlock(void* criticalSection);            // 0x615ef0

// The out-of-line copies of ios::lock(), unlock(), unsetf() and setf() the game's own code carries (0x4f90b0-0x4f9130).
void REB_CDECL ios_lock(ios* stream);
void REB_CDECL ios_unlock(ios* stream);
ios* REB_CDECL ios_unsetf(ios* stream, unsigned mask);
ios* REB_CDECL ios_setf(ios* stream, unsigned mask);

// ---- streambuf -------------------------------------------------------------------------------------------------
streambuf* REB_THISCALL streambuf_ctor(streambuf* self);                             // 0x617d60
void REB_THISCALL streambuf_dtor(streambuf* self);                                   // 0x617dd0
int REB_THISCALL streambuf_sync(streambuf* self);                                    // 0x617fa0
streambuf* REB_THISCALL streambuf_setbuf(streambuf* self, char* buffer, int size);   // 0x617e10
int REB_THISCALL streambuf_seekoff(streambuf* self, long offset, int dir, int mode);  // 0x618060
int REB_THISCALL streambuf_seekpos(streambuf* self, long position, int mode);        // 0x618070
int REB_THISCALL streambuf_xsputn(streambuf* self, const char* text, int count);     // 0x617e50
int REB_THISCALL streambuf_xsgetn(streambuf* self, char* buffer, int count);         // 0x617eb0
int REB_THISCALL streambuf_pbackfail(streambuf* self, int c);                        // 0x618090
int REB_THISCALL streambuf_doallocate(streambuf* self);                              // 0x617fe0
int REB_THISCALL streambuf_allocate(streambuf* self);                                // 0x617fc0
void REB_THISCALL streambuf_setb(streambuf* self, char* begin, char* end, int owned);  // 0x618020
int REB_THISCALL streambuf_snextc(streambuf* self);                                  // 0x6183e0
int REB_THISCALL streambuf_sbumpc(streambuf* self);                                  // 0x618440
void REB_THISCALL streambuf_stossc(streambuf* self);                                 // 0x618490
int REB_THISCALL streambuf_sgetc(streambuf* self);                                   // 0x6184e0

// ---- strstreambuf ----------------------------------------------------------------------------------------------
strstreambuf* REB_THISCALL strstreambuf_ctor(strstreambuf* self);                                            // 0x6154e0
strstreambuf* REB_THISCALL strstreambuf_ctor_buf(strstreambuf* self, char* buffer, int size, char* start);  // 0x615530
void REB_THISCALL strstreambuf_dtor(strstreambuf* self);                                                     // 0x6155f0
void REB_THISCALL strstreambuf_freeze(streambuf* self, int frozen);                                          // 0x615660
char* REB_THISCALL strstreambuf_str(streambuf* self);                                                        // 0x615680
int REB_THISCALL strstreambuf_doallocate(strstreambuf* self);                                                // 0x615690
streambuf* REB_THISCALL strstreambuf_setbuf(strstreambuf* self, char* buffer, int size);                     // 0x6157c0
int REB_THISCALL strstreambuf_overflow(strstreambuf* self, int c);                                           // 0x6157d0
int REB_THISCALL strstreambuf_underflow(strstreambuf* self);                                                 // 0x615860
int REB_THISCALL strstreambuf_seekoff(strstreambuf* self, long offset, int dir, int mode);                   // 0x6158c0

// ---- istream / ostream / iostream construction -----------------------------------------------------------------
istream* REB_THISCALL istream_ctor(istream* self, streambuf* buffer, int constructVirtualBase);   // 0x6179e0
void REB_THISCALL istream_dtor(void* istreamEnd);                                                 // 0x617a80: ECX = istream part + 0xc
ostream* REB_THISCALL ostream_ctor(ostream* self, streambuf* buffer, int constructVirtualBase);   // 0x616210
void REB_THISCALL ostream_dtor(void* ostreamEnd);                                                // 0x6162a0: ECX = ostream part + 8
iostream* REB_THISCALL iostream_ctor(iostream* self, streambuf* buffer, int constructVirtualBase);  // 0x618130
void REB_THISCALL iostream_dtor(ios* iosBase);                                                    // 0x6181d0

istrstream* REB_THISCALL istrstream_ctor_str(istrstream* self, char* text, int constructVirtualBase);                // 0x6159e0
istrstream* REB_THISCALL istrstream_ctor_str_len(istrstream* self, char* text, int length, int constructVirtualBase);  // 0x615ad0
void REB_THISCALL istrstream_dtor(ios* iosBase);                                                                      // 0x615b90
ostrstream* REB_THISCALL ostrstream_ctor(ostrstream* self, int constructVirtualBase);                                 // 0x615bb0
ostrstream* REB_THISCALL ostrstream_ctor_buf(ostrstream* self, char* buffer, int length, int mode, int constructVirtualBase);  // 0x615c90
void REB_THISCALL ostrstream_vbase_dtor(ios* iosBase);                                                                // 0x615d70
void REB_THISCALL ostrstream_dtor(ios* iosBase);                                                                      // 0x615d90
strstream* REB_THISCALL strstream_ctor(strstream* self, int constructVirtualBase);                                    // 0x615db0
void REB_THISCALL strstream_dtor(ios* iosBase);                                                                       // 0x615ea0

// ---- ostream ---------------------------------------------------------------------------------------------------
ostream* REB_THISCALL ostream_operator_ls_char(ostream* out, char value);                   // 0x615f00
int REB_THISCALL ostream_opfx(ostream* out);                                                // 0x615fc0
void REB_THISCALL ostream_osfx(ostream* out);                                               // 0x616040
ostream* REB_THISCALL ostream_operator_ls_cstr(ostream* out, const char* text);             // 0x616110
ostream* REB_THISCALL ostream_flush(ostream* out);                                          // 0x616140
void REB_THISCALL ostream_outstr(ostream* out, const char* prefix, const char* text);       // 0x6162b0
ostream* REB_THISCALL ostream_seekp_pos(ostream* out, long position);                       // 0x616900
ostream* REB_THISCALL ostream_seekp_off(ostream* out, long offset, int dir);                // 0x6169a0
long REB_THISCALL ostream_tellp(ostream* out);                                              // 0x616a40
ostream* REB_THISCALL ostream_operator_ls_streambuf(ostream* out, streambuf* buffer);       // 0x616ae0
ostream* REB_THISCALL ostream_operator_ls_long(ostream* out, long value);                   // 0x616e10
ostream* REB_THISCALL ostream_write(ostream* out, const void* buffer, int count);           // 0x617430
ostream* REB_THISCALL ostream_operator_ls_ulong(ostream* out, unsigned long value);         // 0x617480
ostream* REB_THISCALL ostream_operator_ls_int(ostream* out, int value);                     // 0x617540

// ---- istream ---------------------------------------------------------------------------------------------------
istream* REB_THISCALL istream_operator_rs_streambuf(istream* in, streambuf* buffer);        // 0x616b70
istream* REB_THISCALL istream_seekg_pos(istream* in, long position);                        // 0x616c30
istream* REB_THISCALL istream_seekg_off(istream* in, long offset, int dir);                 // 0x616cd0
long REB_THISCALL istream_tellg(istream* in);                                               // 0x616d70
int REB_THISCALL istream_get(istream* in);                                                  // 0x617330
istream* REB_THISCALL istream_read(istream* in, void* buffer, int count);                   // 0x6173b0
istream* REB_THISCALL istream_operator_rs_long(istream* in, int* value);                    // 0x617610
istream* REB_THISCALL istream_operator_rs_ushort(istream* in, unsigned short* value);       // 0x6176a0
istream* REB_THISCALL istream_operator_rs_ulong(istream* in, unsigned int* value);          // 0x617760
istream* REB_THISCALL istream_getline(istream* in, char* buffer, int limit, int delimiter);  // 0x617800
istream* REB_THISCALL istream_operator_rs_short(istream* in, short* value);                 // 0x617900
int REB_THISCALL istream_ipfx(istream* in, int need);                                       // 0x617a90
istream* REB_THISCALL istream_operator_rs_charptr(istream* in, char* value);                // 0x617b90
void REB_THISCALL istream_eatwhite(istream* in);                                            // 0x617cb0
int REB_THISCALL istream_getint(istream* in, char* buffer);                                 // 0x618510

// ---- filebuf / fstream -----------------------------------------------------------------------------------------
filebuf* REB_THISCALL filebuf_ctor(filebuf* self);                                           // 0x616ed0
void REB_THISCALL filebuf_dtor(filebuf* self);                                               // 0x616f10
filebuf* REB_THISCALL filebuf_close(filebuf* self);                                          // 0x616f90
int REB_THISCALL filebuf_overflow(filebuf* self, int c);                                     // 0x617010
int REB_THISCALL filebuf_underflow(filebuf* self);                                           // 0x6170a0
int REB_THISCALL filebuf_seekoff(filebuf* self, long offset, int dir, int mode);             // 0x617160
int REB_THISCALL filebuf_sync(filebuf* self);                                                // 0x6171b0
streambuf* REB_THISCALL filebuf_setbuf(filebuf* self, char* buffer, int size);               // 0x6172b0
filebuf* REB_THISCALL filebuf_open(filebuf* self, const char* name, int mode, int protection);  // 0x618240

ofstream* REB_THISCALL ofstream_ctor(ofstream* self, int constructVirtualBase);              // 0x6164b0
void REB_THISCALL ofstream_dtor(ios* iosBase);                                               // 0x616590
void REB_THISCALL ofstream_open(ofstream* self, const char* name, int mode, int protection);  // 0x6165b0
ifstream* REB_THISCALL ifstream_ctor(ifstream* self, int constructVirtualBase);              // 0x616620
ifstream* REB_THISCALL ifstream_ctor_open(ifstream* self, const char* name, int mode, int protection, int constructVirtualBase);  // 0x616700
void REB_THISCALL ifstream_vbase_dtor(ios* iosBase);                                         // 0x6167f0
void REB_THISCALL ifstream_dtor(ios* iosBase);                                               // 0x616810
void REB_THISCALL ifstream_open(ifstream* self, const char* name, int mode, int protection);  // 0x616830
void REB_THISCALL fstreambase_close(void* stream);                                           // 0x6168a0

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
inline int& GetlineCount(strstream* stream) { return reinterpret_cast<istream*>(stream)->m_fGline; }

inline istream* AsIstream(strstream* stream) { return reinterpret_cast<istream*>(stream); }
inline ostream* AsOstream(strstream* stream) {
    return reinterpret_cast<ostream*>(reinterpret_cast<char*>(stream) + strstream::kOstreamOffset);
}

// Bytes in the put area of a streambuf (pptr - pbase, 0 if pptr is below pbase).
inline unsigned WrittenBytes(const streambuf* buffer) {
    return buffer->m_pptr < buffer->m_pbase ? 0u : static_cast<unsigned>(buffer->m_pptr - buffer->m_pbase);
}

}  // namespace iostream_access
