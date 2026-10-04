#include "fleet/capship_data.h"

#include "core/utility_classes.h"
#include "core/vptr_store.h"
#include "data/gnprtb.h"
#include "data/random.h"
#include "gameobj/object_family.h"

namespace {
constexpr FamilyRange kCombatUnitFamilies(0x10, 0x20);
}  // namespace

CapShipData::CapShipData() : m_word20(0), m_word24(0), m_word28(0) {}

CapShipData::~CapShipData() { REB_KEEP_VPTR_STORE(); }

int CapShipData::Read(istream* in) {
    const int result = CombatUnitData::Read(in);
    if (result) {
        ReadValue32(in, &m_word20);
        ReadValue32(in, &m_word24);
        ReadValue32(in, &m_word28);
    }
    return result;
}

int CapShipData::Write(ostream* out) {
    const int result = CombatUnitData::Write(out);
    if (result) {
        WriteValue32(out, &m_word20);
        WriteValue32(out, &m_word24);
        WriteValue32(out, &m_word28);
    }
    return result;
}

int CapShipData::Checksum(Crc32* crc) {
    const int result = CombatUnitData::Checksum(crc);
    if (result) {
        crc->AddInt(m_word20);
        crc->AddInt(m_word24);
        crc->AddInt(m_word28);
    }
    return result;
}

CombatUnitIterator::CombatUnitIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kCombatUnitFamilies, stateFilter) {}

CombatUnitIterator::~CombatUnitIterator() { REB_KEEP_VPTR_STORE(); }

int BindGnprtb08() {
    const int first = BindGnprtbParameter(0x800, &g_gnprtb0800);
    const int second = BindGnprtbParameter(0x801, &g_gnprtb0801);
    int ok = second != 0 && first != 0;
    ok = BindGnprtbParameter(0x802, &g_gnprtb0802) != 0 && ok;
    ok = BindGnprtbParameter(0x803, &g_gnprtb0803) != 0 && ok;
    ok = BindGnprtbParameter(0x804, &g_gnprtb0804) != 0 && ok;
    ok = BindGnprtbParameter(0x805, &g_gnprtb0805) != 0 && ok;
    return ok;
}

int RollPercentChance(int percent, int* roll) {
    *roll = PercentChance(percent);
    return 1;
}
