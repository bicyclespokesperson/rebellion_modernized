#include "mission/mission_extra_key.h"
#include "galaxy/mission_decl.h"

MissionExtraKeyBase::MissionExtraKeyBase() = default;

MissionExtraKeyBase::~MissionExtraKeyBase() = default;

int MissionExtraKeyBase::Read(istream* in)
{
  if (!this->Mission::ReadImpl(in)) {
    return 0;
  }
  return m_extraKey.Read(in);
}

int MissionExtraKeyBase::Write(ostream* out)
{
  if (!WriteImpl(out)) {
    return 0;
  }
  return m_extraKey.Write(out);
}

int MissionExtraKeyBase::Checksum(Crc32* crc)
{
  if (!this->Mission::ChecksumImpl(crc)) {
    return 0;
  }
  return m_extraKey.Checksum(crc);
}

int MissionExtraKeyBase::Validate()
{
  return this->Mission::ValidateImpl();
}

int MissionExtraKeyBase::CreateViews(int controlKind, NotifyContext* ctx)
{
  return this->Mission::CreateViewsImpl(controlKind, ctx);
}
