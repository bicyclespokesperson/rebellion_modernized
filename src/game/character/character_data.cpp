#include "character/character_data.h"

#include "core/utility_classes.h"

// ---- RoleData ---------------------------------------------------------------------------------------------

RoleData::RoleData() = default;

RoleData::~RoleData() = default;

int RoleData::Read(istream* in) { return UnitObjData::Read(in); }

int RoleData::Write(ostream* out) { return UnitObjData::Write(out); }

int RoleData::Checksum(Crc32* crc) { return UnitObjData::Checksum(crc); }

// ---- CharacterData ----------------------------------------------------------------------------------------

CharacterData::CharacterData()
    : RoleData(), m_healingStat(), m_forceDiscoveryStat(), m_escapeStat(), m_holderKey(), m_field34(0) {}

CharacterData::~CharacterData() = default;

int CharacterData::Read(istream* in) {
    int result = 0;
    if (RoleData::Read(in) && m_healingStat.Read(in) && m_forceDiscoveryStat.Read(in) && m_escapeStat.Read(in)) {
        result = m_holderKey.Read(in);
        if (result) ReadValue32(in, &m_field34);
    }
    return result;
}

int CharacterData::Write(ostream* out) {
    int result = 0;
    if (RoleData::Write(out) && m_healingStat.Write(out) && m_forceDiscoveryStat.Write(out) &&
        m_escapeStat.Write(out)) {
        result = m_holderKey.Write(out);
        if (result) WriteValue32(out, &m_field34);
    }
    return result;
}

int CharacterData::Checksum(Crc32* crc) {
    int result = 0;
    if (RoleData::Checksum(crc) && m_healingStat.Checksum(crc) && m_forceDiscoveryStat.Checksum(crc) &&
        m_escapeStat.Checksum(crc)) {
        result = m_holderKey.Checksum(crc);
        if (result) crc->AddInt(m_field34);
    }
    return result;
}
