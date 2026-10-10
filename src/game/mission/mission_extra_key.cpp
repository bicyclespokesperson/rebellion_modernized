#include "mission/mission_extra_key.h"
#include "galaxy/mission_decl.h"

MissionExtraKeyBase::MissionExtraKeyBase() {}

MissionExtraKeyBase::~MissionExtraKeyBase() { REB_KEEP_VPTR_STORE(); }

int MissionExtraKeyBase::Read(istream* in) {
    if (!static_cast<MissionRuleBase*>(this)->MissionRuleBase::ReadBody(in)) return 0;
    return m_extraKey.Read(in);
}

int MissionExtraKeyBase::Write(ostream* out) {
    if (!WriteImpl(out)) return 0;
    return m_extraKey.Write(out);
}

int MissionExtraKeyBase::Checksum(Crc32* crc) {
    if (!reinterpret_cast<MissionTypeBase*>(this)->MissionTypeBase::ChecksumBody(crc)) return 0;
    return m_extraKey.Checksum(crc);
}

int MissionExtraKeyBase::Validate() { return this->Mission::ValidateImpl(); }

int MissionExtraKeyBase::CreateViews(int controlKind, NotifyContext* ctx) { return this->Mission::CreateViewsImpl(controlKind, ctx); }
