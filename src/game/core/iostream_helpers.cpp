#include "core/reb_abi.h"
#include "core/iostream_lib.h"

namespace {

unsigned& formatFlags(ios* stream) { return *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(stream) + 0x24); }

}  // namespace

void REB_CDECL ios_lock(ios* stream) { iostream_access::Lock(stream); }  // 0x4f90b0

void REB_CDECL ios_unlock(ios* stream) { iostream_access::Unlock(stream); }  // 0x4f90d0

ios* REB_CDECL ios_unsetf(ios* stream, unsigned mask) {  // 0x4f90f0
    iostream_access::Lock(stream);
    formatFlags(stream) &= ~mask;
    iostream_access::Unlock(stream);
    return stream;
}

ios* REB_CDECL ios_setf(ios* stream, unsigned mask) {  // 0x4f9130
    iostream_access::Lock(stream);
    formatFlags(stream) |= mask;
    iostream_access::Unlock(stream);
    return stream;
}
