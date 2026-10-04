#include "mission/mission_extra_key.h"

MissionExtraKeyBase::MissionExtraKeyBase() {}

MissionExtraKeyBase::~MissionExtraKeyBase() { REB_KEEP_VPTR_STORE(); }

int MissionExtraKeyBase::Read(istream* in) {
    if (!FUN_00592c50(this, in)) return 0;
    return m_extraKey.Read(in);
}

int MissionExtraKeyBase::Write(ostream* out) {
    if (!WriteImpl(out)) return 0;
    return m_extraKey.Write(out);
}

int MissionExtraKeyBase::Checksum(Crc32* crc) {
    if (!FUN_00593ad0(this, crc)) return 0;
    return m_extraKey.Checksum(crc);
}

int MissionExtraKeyBase::Validate() { return FUN_00523c60(this); }

int MissionExtraKeyBase::CreateViews(int controlKind, NotifyContext* ctx) { return FUN_005236e0(this, controlKind, ctx); }
