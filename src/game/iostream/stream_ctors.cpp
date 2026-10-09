// Construction and destruction of the stream classes. Every class with a virtual base `ios` has a constructor that
// builds the ios only when it is the most derived object, and a destructor entry point that is reached through the
// ios (so the part's own address is found by subtracting the part's size from ECX).
#include <cstring>

#include "iostream/iostream.h"

using iostream_access::IosOf;

namespace {

char* Offset(void* object, int bytes) { return static_cast<char*>(object) + bytes; }

ios* IosAt(void* object, int bytes) { return IosOf(Offset(object, bytes)); }

streambuf* NewStrstreambuf() {
    void* memory = ::operator new(sizeof(strstreambuf));
    return memory ? strstreambuf_ctor(static_cast<strstreambuf*>(memory)) : nullptr;
}

streambuf* NewFilebuf() {
    constexpr int kFilebufSize = 0x54;
    void* memory = ::operator new(kFilebufSize);
    return memory ? filebuf_ctor(static_cast<filebuf*>(memory)) : nullptr;
}

void SetOwnVtable(ios* stream, const IosVtable* vftable) { stream->m_vftable = vftable; }

// Marks the ios as the owner of its streambuf (the constructors of the string and file streams all do this).
void OwnBuffer(ios* stream) { stream->m_delbuf = 1; }

}  // namespace

istream* __thiscall istream_ctor(istream* self, streambuf* buffer, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        self->m_vbptr = istream::vbtable;
        ios_ctor(IosOf(self));
    }
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &istream::vftable);
    ios_init(stream, buffer);
    stream->m_flags |= iostream_flags::kSkipWs;
    self->m_gcount = 0;
    self->m_fGline = 0;
    return self;
}

void __thiscall istream_dtor(void* istreamEnd) { SetOwnVtable(IosAt(istreamEnd, -0x0c), &istream::vftable); }

ostream* __thiscall ostream_ctor(ostream* self, streambuf* buffer, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        self->m_vbptr = ostream::vbtable;
        ios_ctor(IosOf(self));
    }
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &ostream::vftable);
    ios_init(stream, buffer);
    self->m_floatUsed = 0;
    return self;
}

void __thiscall ostream_dtor(void* ostreamEnd) { SetOwnVtable(IosAt(ostreamEnd, -0x08), &ostream::vftable); }

iostream* __thiscall iostream_ctor(iostream* self, streambuf* buffer, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(Offset(self, 0)) = iostream::vbtableIstream;
        *reinterpret_cast<const int**>(Offset(self, iostream::kOstreamOffset)) = iostream::vbtableOstream;
        ios_ctor(reinterpret_cast<ios*>(Offset(self, iostream::kIosOffset)));
    }
    istream_ctor(reinterpret_cast<istream*>(self), buffer, 0);
    ostream_ctor(reinterpret_cast<ostream*>(Offset(self, iostream::kOstreamOffset)), buffer, 0);
    SetOwnVtable(IosOf(self), &iostream::vftable);
    return self;
}

void __thiscall iostream_dtor(ios* iosBase) {
    SetOwnVtable(IosAt(iosBase, -iostream::kIosOffset), &iostream::vftable);
    ostream_dtor(iosBase);
    istream_dtor(Offset(iosBase, -8));
}

istrstream* __thiscall istrstream_ctor_str(istrstream* self, char* text, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(self) = istrstream::vbtable;
        ios_ctor(IosOf(self));
    }
    void* memory = ::operator new(sizeof(strstreambuf));
    streambuf* buffer = memory ? strstreambuf_ctor_buf(static_cast<strstreambuf*>(memory), text, 0, nullptr) : nullptr;
    istream_ctor(reinterpret_cast<istream*>(self), buffer, 0);
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &istrstream::vftable);
    OwnBuffer(stream);
    return self;
}

istrstream* __thiscall istrstream_ctor_str_len(istrstream* self, char* text, int length, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(self) = istrstream::vbtable;
        ios_ctor(IosOf(self));
    }
    void* memory = ::operator new(sizeof(strstreambuf));
    streambuf* buffer = memory ? strstreambuf_ctor_buf(static_cast<strstreambuf*>(memory), text, length, nullptr) : nullptr;
    istream_ctor(reinterpret_cast<istream*>(self), buffer, 0);
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &istrstream::vftable);
    OwnBuffer(stream);
    return self;
}

void __thiscall istrstream_dtor(ios* iosBase) {
    SetOwnVtable(IosAt(iosBase, -istrstream::kIosOffset), &istrstream::vftable);
    istream_dtor(iosBase);
}

ostrstream* __thiscall ostrstream_ctor(ostrstream* self, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(self) = ostrstream::vbtable;
        ios_ctor(IosOf(self));
    }
    streambuf* buffer = NewStrstreambuf();
    ostream_ctor(reinterpret_cast<ostream*>(self), buffer, 0);
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &ostrstream::vftable);
    OwnBuffer(stream);
    return self;
}

ostrstream* __thiscall ostrstream_ctor_buf(ostrstream* self, char* buffer, int length, int mode, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(self) = ostrstream::vbtable;
        ios_ctor(IosOf(self));
    }
    void* memory = ::operator new(sizeof(strstreambuf));
    streambuf* streamBuffer = memory ? strstreambuf_ctor_buf(static_cast<strstreambuf*>(memory), buffer, length, buffer) : nullptr;
    ostream_ctor(reinterpret_cast<ostream*>(self), streamBuffer, 0);
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &ostrstream::vftable);
    OwnBuffer(stream);
    if ((mode & (iostream_flags::kAte | iostream_flags::kApp)) != 0)
        ostream_seekp_off(reinterpret_cast<ostream*>(self), static_cast<long>(strlen(buffer)), iostream_flags::kSeekBeg);
    return self;
}

void __thiscall ostrstream_vbase_dtor(ios* iosBase) {
    ostream_dtor(iosBase);
    ios_dtor(iosBase);
}

void __thiscall ostrstream_dtor(ios* iosBase) {
    SetOwnVtable(IosAt(iosBase, -ostrstream::kIosOffset), &ostrstream::vftable);
    ostream_dtor(iosBase);
}

strstream* __thiscall strstream_ctor(strstream* self, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(Offset(self, 0)) = strstream::vbtableIstream;
        *reinterpret_cast<const int**>(Offset(self, strstream::kOstreamOffset)) = strstream::vbtableOstream;
        ios_ctor(reinterpret_cast<ios*>(Offset(self, strstream::kIosOffset)));
    }
    streambuf* buffer = NewStrstreambuf();
    iostream_ctor(reinterpret_cast<iostream*>(self), buffer, 0);
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &strstream::vftable);
    OwnBuffer(stream);
    return self;
}

void __thiscall strstream_dtor(ios* iosBase) {
    SetOwnVtable(IosAt(iosBase, -strstream::kIosOffset), &strstream::vftable);
    iostream_dtor(iosBase);
}

ofstream* __thiscall ofstream_ctor(ofstream* self, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(self) = ofstream::vbtable;
        ios_ctor(IosOf(self));
    }
    streambuf* buffer = NewFilebuf();
    ostream_ctor(reinterpret_cast<ostream*>(self), buffer, 0);
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &ofstream::vftable);
    OwnBuffer(stream);
    return self;
}

void __thiscall ofstream_dtor(ios* iosBase) {
    SetOwnVtable(IosAt(iosBase, -ofstream::kIosOffset), &ofstream::vftable);
    ostream_dtor(iosBase);
}

void __thiscall ofstream_open(ofstream* self, const char* name, int mode, int protection) {
    ios* stream = IosOf(self);
    auto* buffer = static_cast<filebuf*>(stream->m_bp);
    if (buffer->m_fd == -1 && filebuf_open(buffer, name, mode | iostream_flags::kOut, protection) != nullptr) return;
    iostream_access::SetState(stream, iostream_flags::kFail);
}

ifstream* __thiscall ifstream_ctor(ifstream* self, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(self) = ifstream::vbtable;
        ios_ctor(IosOf(self));
    }
    streambuf* buffer = NewFilebuf();
    istream_ctor(reinterpret_cast<istream*>(self), buffer, 0);
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &ifstream::vftable);
    OwnBuffer(stream);
    return self;
}

ifstream* __thiscall ifstream_ctor_open(ifstream* self, const char* name, int mode, int protection, int constructVirtualBase) {
    if (constructVirtualBase != 0) {
        *reinterpret_cast<const int**>(self) = ifstream::vbtable;
        ios_ctor(IosOf(self));
    }
    streambuf* buffer = NewFilebuf();
    istream_ctor(reinterpret_cast<istream*>(self), buffer, 0);
    ios* stream = IosOf(self);
    SetOwnVtable(stream, &ifstream::vftable);
    OwnBuffer(stream);
    if (filebuf_open(static_cast<filebuf*>(stream->m_bp), name, mode | iostream_flags::kIn, protection) == nullptr)
        stream->m_state |= iostream_flags::kFail;
    return self;
}

void __thiscall ifstream_vbase_dtor(ios* iosBase) {
    istream_dtor(iosBase);
    ios_dtor(iosBase);
}

void __thiscall ifstream_dtor(ios* iosBase) {
    SetOwnVtable(IosAt(iosBase, -ifstream::kIosOffset), &ifstream::vftable);
    istream_dtor(iosBase);
}

void __thiscall ifstream_open(ifstream* self, const char* name, int mode, int protection) {
    ios* stream = IosOf(self);
    auto* buffer = static_cast<filebuf*>(stream->m_bp);
    if (buffer->m_fd == -1 && filebuf_open(buffer, name, mode | iostream_flags::kIn, protection) != nullptr) return;
    iostream_access::SetState(stream, iostream_flags::kFail);
}

void __thiscall fstreambase_close(void* stream) {
    ios* base = IosOf(stream);
    const int state = filebuf_close(static_cast<filebuf*>(base->m_bp)) == nullptr ? base->m_state | iostream_flags::kFail : 0;
    iostream_access::Clear(base, state);
}
