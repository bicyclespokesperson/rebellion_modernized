// The scalar deleting destructors in the LIBCI vftables (slot 0). The original compiler made them; the reconstructed
// iostream classes are layout structs with free functions (src/game/iostream), so nothing else defines them. They carry
// the symbol name tools/gen_data_image.py --standalone gives the vtable slot (reb_orig_<address>); they are deliberately
// not hooks in the hybrid (docs: compiler-generated functions are never hooked).
#include <new>

#include "iostream/iostream.h"

#define REB_ORIG(address) __asm__("reb_orig_" #address)

namespace {

void* WholeObject(void* iosBase, size_t offsetOfIos) { return static_cast<char*>(iosBase) - offsetOfIos; }

void DeleteIfRequested(void* whole, unsigned flags) {
    if (flags & 1) ::operator delete(whole);
}

}  // namespace

void* __thiscall IosScalarDeletingDtor_ios(ios* self, unsigned flags) REB_ORIG(00615420);
void* __thiscall IosScalarDeletingDtor_ios(ios* self, unsigned flags) {
    ios_dtor(self);
    DeleteIfRequested(self, flags);
    return self;
}

void* __thiscall IosScalarDeletingDtor_strstreambuf(strstreambuf* self, unsigned flags) REB_ORIG(00615510);
void* __thiscall IosScalarDeletingDtor_strstreambuf(strstreambuf* self, unsigned flags) {
    strstreambuf_dtor(self);
    DeleteIfRequested(self, flags);
    return self;
}

void* __thiscall IosScalarDeletingDtor_filebuf(filebuf* self, unsigned flags) REB_ORIG(00616ef0);
void* __thiscall IosScalarDeletingDtor_filebuf(filebuf* self, unsigned flags) {
    filebuf_dtor(self);
    DeleteIfRequested(self, flags);
    return self;
}

void* __thiscall IosScalarDeletingDtor_streambuf(streambuf* self, unsigned flags) REB_ORIG(00617db0);
void* __thiscall IosScalarDeletingDtor_streambuf(streambuf* self, unsigned flags) {
    streambuf_dtor(self);
    DeleteIfRequested(self, flags);
    return self;
}

#define REB_STREAM_SDD(name, address, iosOffset, ownDtor)                                         \
    void* __thiscall IosScalarDeletingDtor_##name(ios* self, unsigned flags) REB_ORIG(address);   \
    void* __thiscall IosScalarDeletingDtor_##name(ios* self, unsigned flags) {                    \
        void* whole = WholeObject(self, iosOffset);                                               \
        ownDtor(self);                                                                            \
        ios_dtor(self);                                                                           \
        DeleteIfRequested(whole, flags);                                                          \
        return whole;                                                                             \
    }

REB_STREAM_SDD(istrstream, 00615aa0, 0xc, istrstream_dtor)
REB_STREAM_SDD(ostrstream, 00615c60, 8, ostrstream_dtor)
REB_STREAM_SDD(strstream, 00615e70, 0x14, strstream_dtor)
REB_STREAM_SDD(ostream, 006161e0, 8, ostream_dtor)
REB_STREAM_SDD(ofstream, 00616560, 8, ofstream_dtor)
REB_STREAM_SDD(ifstream, 006166d0, 0xc, ifstream_dtor)
REB_STREAM_SDD(istream, 006179b0, 0xc, istream_dtor)
REB_STREAM_SDD(iostream, 00618100, 0x14, iostream_dtor)
