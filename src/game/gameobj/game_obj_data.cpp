#include "gameobj/game_obj_data.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"

GameObjData::GameObjData() : m_field04(0), m_lockCount(0), m_key(), m_stat() {}

GameObjData::~GameObjData() = default;

int GameObjData::Read(istream* in) {
    ReadValue32(in, &m_field04);
    ReadValue32(in, &m_lockCount);
    int ok = m_key.Read(in);
    if (ok) ok = m_stat.Read(in);
    return ok;
}

int GameObjData::Write(ostream* out) {
    WriteValue32(out, &m_field04);
    WriteValue32(out, &m_lockCount);
    int ok = m_key.Write(out);
    if (ok) ok = m_stat.Write(out);
    return ok;
}

int GameObjData::Checksum(Crc32* crc) {
    crc->AddInt(m_field04);
    crc->AddInt(m_lockCount);
    int ok = m_key.Checksum(crc);
    if (ok) ok = m_stat.Checksum(crc);
    return ok;
}

// ---- SystemData -----------------------------------------------------------------------------------------------

SystemData::SystemData()
    : GameObjData(), m_word48(), m_word54(), m_word60(0), m_word64(0), m_word68(), m_word74(0), m_word78(0),
      m_word7c(0), m_flags80(0) {}

SystemData::~SystemData() = default;

namespace {

template <class Stats, class Visit>
int visitAll(Stats& stats, Visit visit) {
    int ok = 1;
    for (auto& stat : stats) {
        ok = visit(stat);
        if (!ok) return ok;
    }
    return ok;
}

}  // namespace

int SystemData::Read(istream* in) {
    int ok = GameObjData::Read(in);
    if (!ok) return 0;
    ok = visitAll(m_stats, [in](TrackedStat& stat) { return stat.Read(in); });
    if (!ok) return 0;
    for (int i = 0; i < 3; ++i) {
        ReadValue32(in, &m_word48[i]);
        ReadValue32(in, &m_word54[i]);
        ReadValue32(in, &m_word68[i]);
    }
    ReadValue32(in, &m_word60);
    ReadValue32(in, &m_word64);
    ReadValue32(in, &m_word74);
    ReadValue32(in, &m_word78);
    ReadValue32(in, &m_word7c);
    ReadValue32(in, &m_flags80);
    return ok;
}

int SystemData::Write(ostream* out) {
    int ok = GameObjData::Write(out);
    if (!ok) return 0;
    ok = visitAll(m_stats, [out](TrackedStat& stat) { return stat.Write(out); });
    if (!ok) return 0;
    for (int i = 0; i < 3; ++i) {
        WriteValue32(out, &m_word48[i]);
        WriteValue32(out, &m_word54[i]);
        WriteValue32(out, &m_word68[i]);
    }
    WriteValue32(out, &m_word60);
    WriteValue32(out, &m_word64);
    WriteValue32(out, &m_word74);
    WriteValue32(out, &m_word78);
    WriteValue32(out, &m_word7c);
    WriteValue32(out, &m_flags80);
    return ok;
}

int SystemData::Checksum(Crc32* crc) {
    int ok = GameObjData::Checksum(crc);
    if (!ok) return 0;
    ok = visitAll(m_stats, [crc](TrackedStat& stat) { return stat.Checksum(crc); });
    if (!ok) return 0;
    for (int i = 0; i < 3; ++i) {
        crc->AddInt(m_word48[i]);
        crc->AddInt(m_word54[i]);
        crc->AddInt(m_word68[i]);
    }
    crc->AddInt(m_word60);
    crc->AddInt(m_word64);
    crc->AddInt(m_word74);
    crc->AddInt(m_word78);
    crc->AddInt(m_word7c);
    crc->AddInt(static_cast<int>(m_flags80));
    return ok;
}

GameObjData* NewSystemData() { return NewOrNull<SystemData>(); }
