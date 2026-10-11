#include "core/iostream_lib.h"

namespace
{

unsigned& formatFlags(ios* stream)
{
  return stream->m_flags;
}

} // namespace

void ios_lock(ios* stream)
{
  iostream_access::Lock(stream);
} // 0x4f90b0

void ios_unlock(ios* stream)
{
  iostream_access::Unlock(stream);
} // 0x4f90d0

ios* ios_unsetf(ios* stream, unsigned mask)
{ // 0x4f90f0
  iostream_access::Lock(stream);
  formatFlags(stream) &= ~mask;
  iostream_access::Unlock(stream);
  return stream;
}

ios* ios_setf(ios* stream, unsigned mask)
{ // 0x4f9130
  iostream_access::Lock(stream);
  formatFlags(stream) |= mask;
  iostream_access::Unlock(stream);
  return stream;
}
