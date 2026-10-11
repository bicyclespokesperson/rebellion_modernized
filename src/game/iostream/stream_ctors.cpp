#include <cstring>
#include <new>

#include "iostream/iostream.h"

namespace
{

strstreambuf* NewStrstreambuf()
{
  return new (std::nothrow) strstreambuf();
}
filebuf* NewFilebuf()
{
  return new (std::nothrow) filebuf();
}

void Bind(istream* stream, ios* state, streambuf* buffer)
{
  stream->Bind(state);
  ios_init(state, buffer);
  state->m_flags |= iostream_flags::kSkipWs;
  stream->m_gcount = 0;
  stream->m_fGline = 0;
}

void Bind(ostream* stream, ios* state, streambuf* buffer)
{
  stream->Bind(state);
  ios_init(state, buffer);
  stream->m_floatUsed = 0;
}

} // namespace

istream* istream_ctor(istream* self, streambuf* buffer, int constructVirtualBase)
{
  ios* state = self->State();
  if (constructVirtualBase)
    ios_ctor(state);
  Bind(self, state, buffer);
  return self;
}
void istream_dtor(void*)
{
}

ostream* ostream_ctor(ostream* self, streambuf* buffer, int constructVirtualBase)
{
  ios* state = self->State();
  if (constructVirtualBase)
    ios_ctor(state);
  Bind(self, state, buffer);
  return self;
}
void ostream_dtor(void*)
{
}

iostream* iostream_ctor(iostream* self, streambuf* buffer, int constructVirtualBase)
{
  if (constructVirtualBase)
    ios_ctor(self->Ios());
  Bind(self->In(), self->Ios(), buffer);
  Bind(self->Out(), self->Ios(), buffer);
  return self;
}
void iostream_dtor(ios*)
{
}

istrstream* istrstream_ctor_str(istrstream* self, char* text, int constructVirtualBase)
{
  return istrstream_ctor_str_len(self, text, 0, constructVirtualBase);
}
istrstream* istrstream_ctor_str_len(istrstream* self, char* text, int length, int constructVirtualBase)
{
  if (constructVirtualBase)
    ios_ctor(self->Ios());
  auto* buffer = new (std::nothrow) strstreambuf();
  if (buffer)
    strstreambuf_ctor_buf(buffer, text, length, nullptr);
  istream_ctor(self->In(), buffer, 0);
  self->Ios()->m_delbuf = buffer != nullptr;
  return self;
}
void istrstream_dtor(ios*)
{
}

ostrstream* ostrstream_ctor(ostrstream* self, int constructVirtualBase)
{
  if (constructVirtualBase)
    ios_ctor(self->Ios());
  auto* buffer = NewStrstreambuf();
  ostream_ctor(self->Out(), buffer, 0);
  self->Ios()->m_delbuf = buffer != nullptr;
  return self;
}
ostrstream* ostrstream_ctor_buf(ostrstream* self, char* buffer, int length, int mode, int constructVirtualBase)
{
  if (constructVirtualBase)
    ios_ctor(self->Ios());
  auto* streamBuffer = new (std::nothrow) strstreambuf();
  if (streamBuffer)
    strstreambuf_ctor_buf(streamBuffer, buffer, length, buffer);
  ostream_ctor(self->Out(), streamBuffer, 0);
  self->Ios()->m_delbuf = streamBuffer != nullptr;
  if (streamBuffer && (mode & (iostream_flags::kAte | iostream_flags::kApp)) != 0)
    ostream_seekp_off(self->Out(), static_cast<long>(strlen(buffer)), iostream_flags::kSeekBeg);
  return self;
}
void ostrstream_vbase_dtor(ios* state)
{
  ios_dtor(state);
}
void ostrstream_dtor(ios*)
{
}

strstream* strstream_ctor(strstream* self, int constructVirtualBase)
{
  if (constructVirtualBase)
    ios_ctor(self->Ios());
  auto* buffer = NewStrstreambuf();
  Bind(self->In(), self->Ios(), buffer);
  Bind(self->Out(), self->Ios(), buffer);
  self->Ios()->m_delbuf = buffer != nullptr;
  return self;
}
void strstream_dtor(ios*)
{
}

ofstream* ofstream_ctor(ofstream* self, int constructVirtualBase)
{
  if (constructVirtualBase)
    ios_ctor(self->Ios());
  auto* buffer = NewFilebuf();
  ostream_ctor(self->Out(), buffer, 0);
  self->Ios()->m_delbuf = buffer != nullptr;
  return self;
}
void ofstream_dtor(ios*)
{
}
void ofstream_open(ofstream* self, const char* name, int mode, int protection)
{
  ios* state = iostream_access::IosOf(self);
  auto* buffer = static_cast<filebuf*>(state->m_bp);
  if (buffer && buffer->m_fd == -1 && filebuf_open(buffer, name, mode | iostream_flags::kOut, protection))
    return;
  iostream_access::SetState(state, iostream_flags::kFail);
}

ifstream* ifstream_ctor(ifstream* self, int constructVirtualBase)
{
  if (constructVirtualBase)
    ios_ctor(self->Ios());
  auto* buffer = NewFilebuf();
  istream_ctor(self->In(), buffer, 0);
  self->Ios()->m_delbuf = buffer != nullptr;
  return self;
}
ifstream* ifstream_ctor_open(ifstream* self, const char* name, int mode, int protection, int constructVirtualBase)
{
  ifstream_ctor(self, constructVirtualBase);
  ios* state = self->Ios();
  auto* buffer = static_cast<filebuf*>(state->m_bp);
  if (!buffer || filebuf_open(buffer, name, mode | iostream_flags::kIn, protection) == nullptr)
    state->m_state |= iostream_flags::kFail;
  return self;
}
void ifstream_vbase_dtor(ios* state)
{
  ios_dtor(state);
}
void ifstream_dtor(ios*)
{
}
void ifstream_open(ifstream* self, const char* name, int mode, int protection)
{
  ios* state = iostream_access::IosOf(self);
  auto* buffer = static_cast<filebuf*>(state->m_bp);
  if (buffer && buffer->m_fd == -1 && filebuf_open(buffer, name, mode | iostream_flags::kIn, protection))
    return;
  iostream_access::SetState(state, iostream_flags::kFail);
}
namespace
{
void CloseState(ios* state)
{
  const int result =
    filebuf_close(static_cast<filebuf*>(state->m_bp)) == nullptr ? state->m_state | iostream_flags::kFail : 0;
  iostream_access::Clear(state, result);
}
} // namespace
void fstreambase_close(istream* stream)
{
  CloseState(stream->State());
}
void fstreambase_close(ostream* stream)
{
  CloseState(stream->State());
}
