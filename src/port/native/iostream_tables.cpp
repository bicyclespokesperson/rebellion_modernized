// The iostream library's vftables and vbtables for the native build. The reconstructed library (src/game/iostream) models the
// original's classes as layout structs with free functions, so its tables are data, not compiler output; the Windows builds
// take them from the original image (tools/gen_data_image.py). Here they are written out: the slots are the reconstructed
// functions (the original tables, dumped from REBEXE.EXE 0x66e4dc-0x66e620, name the same ones) and the vbtable offsets follow
// from the layout the headers compute.
#include <new>

#include "iostream/iostream.h"

int icf_return_zero();

namespace {

template <typename Slot, typename Function>
Slot AsSlot(Function function) {
    return reinterpret_cast<Slot>(function);
}

int DeleteIfRequested(void* whole, unsigned flags) {
    if (flags & 1) ::operator delete(whole);
    return 0;
}

void* WholeObject(void* iosBase, size_t offsetOfIos) { return static_cast<char*>(iosBase) - offsetOfIos; }

int ScalarDeletingDtorIos(void* self, unsigned flags) {
    ios_dtor(static_cast<ios*>(self));
    return DeleteIfRequested(self, flags);
}

int ScalarDeletingDtorStrstreambuf(void* self, unsigned flags) {
    strstreambuf_dtor(static_cast<strstreambuf*>(self));
    return DeleteIfRequested(self, flags);
}

int ScalarDeletingDtorFilebuf(void* self, unsigned flags) {
    filebuf_dtor(static_cast<filebuf*>(self));
    return DeleteIfRequested(self, flags);
}

int ScalarDeletingDtorStreambuf(void* self, unsigned flags) {
    streambuf_dtor(static_cast<streambuf*>(self));
    return DeleteIfRequested(self, flags);
}

#define REB_STREAM_SDD(name, ownDtor, ownerClass)                             \
    int ScalarDeletingDtor##name(void* self, unsigned flags) {                \
        void* whole = WholeObject(self, ownerClass::kIosOffset);              \
        ownDtor(static_cast<ios*>(self));                                     \
        ios_dtor(static_cast<ios*>(self));                                    \
        return DeleteIfRequested(whole, flags);                               \
    }

struct IstreamAlone {
    static constexpr int kIosOffset = StreamLayout::Align(sizeof(istream));
};
struct OstreamAlone {
    static constexpr int kIosOffset = StreamLayout::Align(sizeof(ostream));
};

void IstreamDtorFromIos(ios* base) { istream_dtor(base); }
void OstreamDtorFromIos(ios* base) { ostream_dtor(base); }

REB_STREAM_SDD(Istrstream, istrstream_dtor, istrstream)
REB_STREAM_SDD(Ostrstream, ostrstream_dtor, ostrstream)
REB_STREAM_SDD(Strstream, strstream_dtor, strstream)
REB_STREAM_SDD(Ostream, OstreamDtorFromIos, OstreamAlone)
REB_STREAM_SDD(Ofstream, ofstream_dtor, ofstream)
REB_STREAM_SDD(Ifstream, ifstream_dtor, ifstream)
REB_STREAM_SDD(Istream, IstreamDtorFromIos, IstreamAlone)
REB_STREAM_SDD(Iostream, iostream_dtor, iostream)

int PureCall(streambuf*) { return -1; }

using SyncSlot = int (*)(streambuf*);
using SetbufSlot = streambuf* (*)(streambuf*, char*, int);
using SeekoffSlot = int (*)(streambuf*, long, int, int);
using SeekposSlot = int (*)(streambuf*, long, int);
using XsputnSlot = int (*)(streambuf*, const char*, int);
using XsgetnSlot = int (*)(streambuf*, char*, int);
using CharSlot = int (*)(streambuf*, int);
using ProbeSlot = int (*)(streambuf*);

}  // namespace

const IosVtable ios::vftable = {ScalarDeletingDtorIos};
const IosVtable istream::vftable = {ScalarDeletingDtorIstream};
const IosVtable ostream::vftable = {ScalarDeletingDtorOstream};
const IosVtable iostream::vftable = {ScalarDeletingDtorIostream};
const IosVtable istrstream::vftable = {ScalarDeletingDtorIstrstream};
const IosVtable ostrstream::vftable = {ScalarDeletingDtorOstrstream};
const IosVtable strstream::vftable = {ScalarDeletingDtorStrstream};
const IosVtable ifstream::vftable = {ScalarDeletingDtorIfstream};
const IosVtable ofstream::vftable = {ScalarDeletingDtorOfstream};

const StreambufVtable streambuf::vftable = {
    ScalarDeletingDtorStreambuf,
    streambuf_sync,
    streambuf_setbuf,
    streambuf_seekoff,
    streambuf_seekpos,
    streambuf_xsputn,
    streambuf_xsgetn,
    AsSlot<CharSlot>(PureCall),
    PureCall,
    streambuf_pbackfail,
    streambuf_doallocate,
};

const StreambufVtable strstreambuf::vftable = {
    ScalarDeletingDtorStrstreambuf,
    AsSlot<SyncSlot>(icf_return_zero),
    AsSlot<SetbufSlot>(strstreambuf_setbuf),
    AsSlot<SeekoffSlot>(strstreambuf_seekoff),
    streambuf_seekpos,
    streambuf_xsputn,
    streambuf_xsgetn,
    AsSlot<CharSlot>(strstreambuf_overflow),
    AsSlot<ProbeSlot>(strstreambuf_underflow),
    streambuf_pbackfail,
    AsSlot<ProbeSlot>(strstreambuf_doallocate),
};

const StreambufVtable filebuf::vftable = {
    ScalarDeletingDtorFilebuf,
    AsSlot<SyncSlot>(filebuf_sync),
    AsSlot<SetbufSlot>(filebuf_setbuf),
    AsSlot<SeekoffSlot>(filebuf_seekoff),
    streambuf_seekpos,
    streambuf_xsputn,
    streambuf_xsgetn,
    AsSlot<CharSlot>(filebuf_overflow),
    AsSlot<ProbeSlot>(filebuf_underflow),
    streambuf_pbackfail,
    streambuf_doallocate,
};

const int istream::vbtable[2] = {0, IstreamAlone::kIosOffset};
const int ostream::vbtable[2] = {0, OstreamAlone::kIosOffset};
const int iostream::vbtableIstream[2] = {0, iostream::kIosOffset};
const int iostream::vbtableOstream[2] = {0, iostream::kIosOffset - iostream::kOstreamOffset};
const int istrstream::vbtable[2] = {0, istrstream::kIosOffset};
const int ostrstream::vbtable[2] = {0, ostrstream::kIosOffset};
const int strstream::vbtableIstream[2] = {0, strstream::kIosOffset};
const int strstream::vbtableOstream[2] = {0, strstream::kIosOffset - strstream::kOstreamOffset};
const int ifstream::vbtable[2] = {0, ifstream::kIosOffset};
const int ofstream::vbtable[2] = {0, ofstream::kIosOffset};
