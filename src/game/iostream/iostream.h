#pragma once
#include <cstddef>
#include <cstdint>

#include "platform/win32.h"


// The statically linked iostream library (LIBCI, the pre-standard Microsoft iostreams shipped with Visual C++ 5), as
// reconstructed in src/game/iostream. This is the only declaration of the library: game code includes it and calls the
// entry points below, which are hooked over the original addresses.
//
// The old Microsoft stream library is implemented with typed C++ ownership while retaining its game-specific parsing and serialization entry points.

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

class ios {
public:
    ios();
    ~ios();
    ios(const ios&) = delete;
    ios& operator=(const ios&) = delete;

    void Initialize(streambuf* buffer = nullptr);
    void Cleanup();

    streambuf* m_bp{};
    int m_state{};
    int m_ispecial{};
    int m_ospecial{};
    int m_isfxSpecial{};
    int m_osfxSpecial{};
    int m_delbuf{};
    ostream* m_tie{};
    unsigned m_flags{};
    int m_precision{};
    char m_fill{};
    int m_width{};
    int m_lockCount{-1};
    CRITICAL_SECTION m_lock{};

    bool m_lockInitialized{};
    bool m_bufferReleased{};
};

class streambuf {
public:
    streambuf();
    virtual ~streambuf();
    streambuf(const streambuf&) = delete;
    streambuf& operator=(const streambuf&) = delete;

    virtual int Sync();
    virtual streambuf* SetBuffer(char* buffer, int size);
    virtual int SeekOffset(long offset, int dir, int mode);
    virtual int SeekPosition(long position, int mode);
    virtual int Write(const char* text, int count);
    virtual int Read(char* buffer, int count);
    virtual int Overflow(int c);
    virtual int Underflow();
    virtual int Putback(int c);
    virtual int Allocate();

    void Initialize();
    void Cleanup();

    int m_allocated{};
    int m_unbuffered{};
    int m_lastc{-1};
    char* m_base{};
    char* m_ebuf{};
    char* m_pbase{};
    char* m_pptr{};
    char* m_epptr{};
    char* m_eback{};
    char* m_gptr{};
    char* m_egptr{};
    int m_lockCount{-1};
    CRITICAL_SECTION m_lock{};

    bool m_lockInitialized{};
    bool m_cleaned{};
};

class strstreambuf : public streambuf {
public:
    strstreambuf();
    ~strstreambuf() override;

    int m_dynamic{};
    int m_nextSize{};
    int m_static{};
    void*(* m_allocate)(size_t){};
    void(* m_free)(void*){};

    int Sync() override;
    streambuf* SetBuffer(char* buffer, int size) override;
    int SeekOffset(long offset, int dir, int mode) override;
    int Overflow(int c) override;
    int Underflow() override;
    int Putback(int c) override;
    int Allocate() override;
};

class filebuf : public streambuf {
public:
    filebuf();
    ~filebuf() override;

    int m_fd{-1};
    int m_open{};

    int Sync() override;
    streambuf* SetBuffer(char* buffer, int size) override;
    int SeekOffset(long offset, int dir, int mode) override;
    int Overflow(int c) override;
    int Underflow() override;
};

class istream {
public:
    explicit istream(ios* state = nullptr) : m_state(state) {}
    ios* State() const { return m_state; }
    void Bind(ios* state) { m_state = state; }
    int m_fGline{};
    int m_gcount{};

private:
    ios* m_state{};
};

class ostream {
public:
    explicit ostream(ios* state = nullptr) : m_state(state) {}
    ios* State() const { return m_state; }
    void Bind(ios* state) { m_state = state; }
    int m_floatUsed{};

private:
    ios* m_state{};
};

class iostream {
public:
    iostream() : m_in(&m_ios), m_out(&m_ios) {}
    istream* In() { return &m_in; }
    const istream* In() const { return &m_in; }
    ostream* Out() { return &m_out; }
    const ostream* Out() const { return &m_out; }
    ios* Ios() { return &m_ios; }
    const ios* Ios() const { return &m_ios; }

private:
    ios m_ios;
    istream m_in;
    ostream m_out;
};

class istrstream {
public:
    istrstream() : m_in(&m_ios) {}
    istream* In() { return &m_in; }
    const istream* In() const { return &m_in; }
    ios* Ios() { return &m_ios; }
    const ios* Ios() const { return &m_ios; }

private:
    ios m_ios;
    istream m_in;
};

class ostrstream {
public:
    ostrstream() : m_out(&m_ios) {}
    ostream* Out() { return &m_out; }
    const ostream* Out() const { return &m_out; }
    ios* Ios() { return &m_ios; }
    const ios* Ios() const { return &m_ios; }

private:
    ios m_ios;
    ostream m_out;
};

class strstream {
public:
    strstream() : m_in(&m_ios), m_out(&m_ios) {}
    istream* In() { return &m_in; }
    const istream* In() const { return &m_in; }
    ostream* Out() { return &m_out; }
    const ostream* Out() const { return &m_out; }
    ios* Ios() { return &m_ios; }
    const ios* Ios() const { return &m_ios; }

private:
    ios m_ios;
    istream m_in;
    ostream m_out;
};

class ifstream {
public:
    ifstream() : m_in(&m_ios) {}
    istream* In() { return &m_in; }
    const istream* In() const { return &m_in; }
    ios* Ios() { return &m_ios; }
    const ios* Ios() const { return &m_ios; }

private:
    ios m_ios;
    istream m_in;
};

class ofstream {
public:
    ofstream() : m_out(&m_ios) {}
    ostream* Out() { return &m_out; }
    const ostream* Out() const { return &m_out; }
    ios* Ios() { return &m_ios; }
    const ios* Ios() const { return &m_ios; }

private:
    ios m_ios;
    ostream m_out;
};

// ---- ios -------------------------------------------------------------------------------------------------------
ios* ios_ctor(ios* self);                       // 0x6153b0
void ios_dtor(ios* self);                       // 0x615440
void ios_init(ios* self, streambuf* buffer);    // 0x6154a0
void mtx_init(void* criticalSection);              // 0x615ec0
void mtx_term(void* criticalSection);              // 0x615ed0
void mtx_lock(void* criticalSection);              // 0x615ee0
void mtx_unlock(void* criticalSection);            // 0x615ef0

// The out-of-line copies of ios::lock(), unlock(), unsetf() and setf() the game's own code carries (0x4f90b0-0x4f9130).
void ios_lock(ios* stream);
void ios_unlock(ios* stream);
ios* ios_unsetf(ios* stream, unsigned mask);
ios* ios_setf(ios* stream, unsigned mask);

// ---- streambuf -------------------------------------------------------------------------------------------------
streambuf* streambuf_ctor(streambuf* self);                             // 0x617d60
void streambuf_dtor(streambuf* self);                                   // 0x617dd0
int streambuf_sync(streambuf* self);                                    // 0x617fa0
streambuf* streambuf_setbuf(streambuf* self, char* buffer, int size);   // 0x617e10
int streambuf_seekoff(streambuf* self, long offset, int dir, int mode);  // 0x618060
int streambuf_seekpos(streambuf* self, long position, int mode);        // 0x618070
int streambuf_xsputn(streambuf* self, const char* text, int count);     // 0x617e50
int streambuf_xsgetn(streambuf* self, char* buffer, int count);         // 0x617eb0
int streambuf_pbackfail(streambuf* self, int c);                        // 0x618090
int streambuf_doallocate(streambuf* self);                              // 0x617fe0
int streambuf_allocate(streambuf* self);                                // 0x617fc0
void streambuf_setb(streambuf* self, char* begin, char* end, int owned);  // 0x618020
int streambuf_snextc(streambuf* self);                                  // 0x6183e0
int streambuf_sbumpc(streambuf* self);                                  // 0x618440
void streambuf_stossc(streambuf* self);                                 // 0x618490
int streambuf_sgetc(streambuf* self);                                   // 0x6184e0

// ---- strstreambuf ----------------------------------------------------------------------------------------------
strstreambuf* strstreambuf_ctor(strstreambuf* self);                                            // 0x6154e0
strstreambuf* strstreambuf_ctor_buf(strstreambuf* self, char* buffer, int size, char* start);  // 0x615530
void strstreambuf_dtor(strstreambuf* self);                                                     // 0x6155f0
void strstreambuf_freeze(streambuf* self, int frozen);                                          // 0x615660
char* strstreambuf_str(streambuf* self);                                                        // 0x615680
int strstreambuf_doallocate(strstreambuf* self);                                                // 0x615690
streambuf* strstreambuf_setbuf(strstreambuf* self, char* buffer, int size);                     // 0x6157c0
int strstreambuf_overflow(strstreambuf* self, int c);                                           // 0x6157d0
int strstreambuf_underflow(strstreambuf* self);                                                 // 0x615860
int strstreambuf_seekoff(strstreambuf* self, long offset, int dir, int mode);                   // 0x6158c0

// ---- istream / ostream / iostream construction -----------------------------------------------------------------
istream* istream_ctor(istream* self, streambuf* buffer, int constructVirtualBase);   // 0x6179e0
void istream_dtor(void* istreamEnd);                                                 // 0x617a80: ECX = istream part + 0xc
ostream* ostream_ctor(ostream* self, streambuf* buffer, int constructVirtualBase);   // 0x616210
void ostream_dtor(void* ostreamEnd);                                                // 0x6162a0: ECX = ostream part + 8
iostream* iostream_ctor(iostream* self, streambuf* buffer, int constructVirtualBase);  // 0x618130
void iostream_dtor(ios* iosBase);                                                    // 0x6181d0

istrstream* istrstream_ctor_str(istrstream* self, char* text, int constructVirtualBase);                // 0x6159e0
istrstream* istrstream_ctor_str_len(istrstream* self, char* text, int length, int constructVirtualBase);  // 0x615ad0
void istrstream_dtor(ios* iosBase);                                                                      // 0x615b90
ostrstream* ostrstream_ctor(ostrstream* self, int constructVirtualBase);                                 // 0x615bb0
ostrstream* ostrstream_ctor_buf(ostrstream* self, char* buffer, int length, int mode, int constructVirtualBase);  // 0x615c90
void ostrstream_vbase_dtor(ios* iosBase);                                                                // 0x615d70
void ostrstream_dtor(ios* iosBase);                                                                      // 0x615d90
strstream* strstream_ctor(strstream* self, int constructVirtualBase);                                    // 0x615db0
void strstream_dtor(ios* iosBase);                                                                       // 0x615ea0

// ---- ostream ---------------------------------------------------------------------------------------------------
ostream* ostream_operator_ls_char(ostream* out, char value);                   // 0x615f00
int ostream_opfx(ostream* out);                                                // 0x615fc0
void ostream_osfx(ostream* out);                                               // 0x616040
ostream* ostream_operator_ls_cstr(ostream* out, const char* text);             // 0x616110
ostream* ostream_flush(ostream* out);                                          // 0x616140
void ostream_outstr(ostream* out, const char* prefix, const char* text);       // 0x6162b0
ostream* ostream_seekp_pos(ostream* out, long position);                       // 0x616900
ostream* ostream_seekp_off(ostream* out, long offset, int dir);                // 0x6169a0
long ostream_tellp(ostream* out);                                              // 0x616a40
ostream* ostream_operator_ls_streambuf(ostream* out, streambuf* buffer);       // 0x616ae0
ostream* ostream_operator_ls_long(ostream* out, long value);                   // 0x616e10
ostream* ostream_write(ostream* out, const void* buffer, int count);           // 0x617430
ostream* ostream_operator_ls_ulong(ostream* out, unsigned long value);         // 0x617480
ostream* ostream_operator_ls_int(ostream* out, int value);                     // 0x617540

// ---- istream ---------------------------------------------------------------------------------------------------
istream* istream_operator_rs_streambuf(istream* in, streambuf* buffer);        // 0x616b70
istream* istream_seekg_pos(istream* in, long position);                        // 0x616c30
istream* istream_seekg_off(istream* in, long offset, int dir);                 // 0x616cd0
long istream_tellg(istream* in);                                               // 0x616d70
int istream_get(istream* in);                                                  // 0x617330
istream* istream_read(istream* in, void* buffer, int count);                   // 0x6173b0
istream* istream_operator_rs_long(istream* in, int* value);                    // 0x617610
istream* istream_operator_rs_ushort(istream* in, unsigned short* value);       // 0x6176a0
istream* istream_operator_rs_ulong(istream* in, unsigned int* value);          // 0x617760
istream* istream_getline(istream* in, char* buffer, int limit, int delimiter);  // 0x617800
istream* istream_operator_rs_short(istream* in, short* value);                 // 0x617900
int istream_ipfx(istream* in, int need);                                       // 0x617a90
istream* istream_operator_rs_charptr(istream* in, char* value);                // 0x617b90
void istream_eatwhite(istream* in);                                            // 0x617cb0
int istream_getint(istream* in, char* buffer);                                 // 0x618510

// ---- filebuf / fstream -----------------------------------------------------------------------------------------
filebuf* filebuf_ctor(filebuf* self);                                           // 0x616ed0
void filebuf_dtor(filebuf* self);                                               // 0x616f10
filebuf* filebuf_close(filebuf* self);                                          // 0x616f90
int filebuf_overflow(filebuf* self, int c);                                     // 0x617010
int filebuf_underflow(filebuf* self);                                           // 0x6170a0
int filebuf_seekoff(filebuf* self, long offset, int dir, int mode);             // 0x617160
int filebuf_sync(filebuf* self);                                                // 0x6171b0
streambuf* filebuf_setbuf(filebuf* self, char* buffer, int size);               // 0x6172b0
filebuf* filebuf_open(filebuf* self, const char* name, int mode, int protection);  // 0x618240

ofstream* ofstream_ctor(ofstream* self, int constructVirtualBase);              // 0x6164b0
void ofstream_dtor(ios* iosBase);                                               // 0x616590
void ofstream_open(ofstream* self, const char* name, int mode, int protection);  // 0x6165b0
ifstream* ifstream_ctor(ifstream* self, int constructVirtualBase);              // 0x616620
ifstream* ifstream_ctor_open(ifstream* self, const char* name, int mode, int protection, int constructVirtualBase);  // 0x616700
void ifstream_vbase_dtor(ios* iosBase);                                         // 0x6167f0
void ifstream_dtor(ios* iosBase);                                               // 0x616810
void ifstream_open(ifstream* self, const char* name, int mode, int protection);  // 0x616830
void fstreambase_close(istream* stream);
void fstreambase_close(ostream* stream);                                           // 0x6168a0

// ---- helpers the game's inlined iostream code and the library share --------------------------------------------
namespace iostream_access {

inline ios* IosOf(istream* stream) { return stream->State(); }
inline ios* IosOf(const istream* stream) { return stream->State(); }
inline ios* IosOf(ostream* stream) { return stream->State(); }
inline ios* IosOf(const ostream* stream) { return stream->State(); }
template <typename T>
inline ios* IosOf(T* stream) { return stream->Ios(); }
template <typename T>
inline ios* IosOf(const T* stream) { return const_cast<ios*>(stream->Ios()); }
inline ios* IosOf(ios* stream) { return stream; }
inline ios* IosOf(const ios* stream) { return const_cast<ios*>(stream); }

inline int State(const ios* stream) { return stream->m_state; }
template <typename T>
inline int State(const T* stream) { return IosOf(stream)->m_state; }
inline streambuf* BufferOf(const ios* stream) { return stream->m_bp; }
template <typename T>
inline streambuf* BufferOf(const T* stream) { return IosOf(stream)->m_bp; }

inline void Lock(ios* stream) {
    if (stream->m_lockCount < 0) mtx_lock(&stream->m_lock);
}
inline void Unlock(ios* stream) {
    if (stream->m_lockCount < 0) mtx_unlock(&stream->m_lock);
}
inline void Clear(ios* stream, int state) {
    Lock(stream);
    stream->m_state = state;
    Unlock(stream);
}
inline void SetState(ios* stream, int bits) { Clear(stream, stream->m_state | bits); }
inline void LockBuffer(streambuf* buffer) {
    if (buffer->m_lockCount < 0) mtx_lock(&buffer->m_lock);
}
inline void UnlockBuffer(streambuf* buffer) {
    if (buffer->m_lockCount < 0) mtx_unlock(&buffer->m_lock);
}
inline int& GetlineCount(strstream* stream) { return stream->In()->m_fGline; }
inline istream* AsIstream(strstream* stream) { return stream->In(); }
inline ostream* AsOstream(strstream* stream) { return stream->Out(); }
inline unsigned WrittenBytes(const streambuf* buffer) {
    return buffer->m_pptr < buffer->m_pbase ? 0u : static_cast<unsigned>(buffer->m_pptr - buffer->m_pbase);
}

}  // namespace iostream_access
