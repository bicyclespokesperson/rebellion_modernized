#include "ai/ai_class_records.h"

#include "ai/ai_common.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "data/sd_record_bases.h"
#include "data/sd_records.h"

namespace {

enum SpecialForceRecordKey : uint32_t {
    kSpecialForce1 = 0x3c000001,
    kSpecialForce2 = 0x3c000002,
    kSpecialForce3 = 0x3c000003,
    kSpecialForce4 = 0x3c000004,
    kSpecialForce5 = 0x3c000005,
    kSpecialForce6 = 0x3c000006,
    kSpecialForce7 = 0x3c000007,
    kSpecialForce8 = 0x3c000008,
};

ObjectKey AiRecordKey(DatRecord* record) {
    ObjectKey key;
    GetAiRecordKey(record, &key);
    return key;
}

uint32_t RecordKeyOf(DatRecord* record) { return AiRecordKey(record).m_value; }

int ViewOfRecord(const SdRecord* record) { return record->m_field44 == 0 ? 1 : 2; }

bool InRange(uint32_t value, uint32_t first, uint32_t last) { return value >= first && value < last; }

// Index of the first of four values that is not smaller than any other, as the original's nested comparisons pick it.
int IndexOfLargest(int v0, int v1, int v2, int v3) {
    if (!(v0 < v1 || v0 < v2 || v0 < v3)) return 0;
    if (!(v1 < v2 || v1 < v3)) return 1;
    if (!(v2 < v0 || v2 < v1 || v2 < v3)) return 2;
    if (v0 <= v3 && v2 <= v3 && v1 <= v3) return 3;
    return -1;
}

}  // namespace

// ---- AiClassRecordBase ----------------------------------------------------------------------------------------------

AiClassRecordBase::AiClassRecordBase(const ObjectKey& key)
    : AvlNode(), m_flags(0), m_valueA(0), m_valueB(0), m_valueC(0), m_view(0), m_key() {
    m_key = key;
}

AiClassRecordBase::~AiClassRecordBase() { REB_KEEP_VPTR_STORE(); }

// ---- AiSpecialForceClass --------------------------------------------------------------------------------------------

AiSpecialForceClass::AiSpecialForceClass(DatRecord* record) : AiClassRecordBase(AiRecordKey(record)) {
    Fill(static_cast<SpecialForceSD*>(record));
}

AiSpecialForceClass::~AiSpecialForceClass() { REB_KEEP_VPTR_STORE(); }

int AiSpecialForceClass::GetValue(const PersistentInt* mask) {
    const uint32_t bits = mask->m_value;
    if (bits & 0x1) return m_valueB;
    if (bits & 0x4) return m_valueA;
    if (bits & 0x100000) return m_diplomacy;
    if (bits & 0x200000) return m_espionage;
    if (bits & 0x400000) return m_combat;
    if (bits & 0x800000) return m_leadership;
    return 0;
}

void AiSpecialForceClass::Fill(SpecialForceSD* record) {
    m_view = ViewOfRecord(record);
    m_diplomacy = record->m_fields[0];
    m_espionage = record->m_fields[2];
    m_combat = record->m_fields[10];
    m_leadership = record->m_fields[12];
    m_valueA = record->GetField48();
    m_valueB = record->GetField4c();
    m_valueC = record->m_field50;

    const uint32_t key = RecordKeyOf(record);
    if (key == kSpecialForce1 || key == kSpecialForce5)
        m_flags.m_value |= ai_class_flag::kSpecialForceType1;
    else if (key == kSpecialForce2 || key == kSpecialForce6)
        m_flags.m_value |= ai_class_flag::kSpecialForceType2;
    else if (key == kSpecialForce3 || key == kSpecialForce7)
        m_flags.m_value |= ai_class_flag::kSpecialForceType3;
    else if (key == kSpecialForce4 || key == kSpecialForce8)
        m_flags.m_value |= ai_class_flag::kSpecialForceType4;
}

// ---- AiManufacturingClass -------------------------------------------------------------------------------------------

AiManufacturingClass::AiManufacturingClass(DatRecord* record) : AiClassRecordBase(AiRecordKey(record)), m_processingRate(0) {
    Fill(static_cast<ManufacturingFacilitySD*>(record));
}

AiManufacturingClass::~AiManufacturingClass() { REB_KEEP_VPTR_STORE(); }

int AiManufacturingClass::GetValue(const PersistentInt* mask) {
    const uint32_t bits = mask->m_value;
    if (bits & 0x4000) return m_processingRate;
    if (bits & 0x1) return m_valueB;
    if (bits & 0x4) return m_valueA;
    return 0;
}

void AiManufacturingClass::Fill(ManufacturingFacilitySD* record) {
    m_processingRate = record->m_processingRate;
    m_valueA = record->GetField48();
    m_valueB = record->GetField4c();
    m_valueC = record->m_field50;
    if (record->m_field44 == 0)
        m_view = 1;
    else if (record->m_field40 == 0)
        m_view = 2;
    else
        m_view = 0;
    const uint32_t family = record->m_third;
    if (family == 0x28) m_flags.m_value |= 0x4000;
    if (family == 0x29) m_flags.m_value |= 0x8000;
    if (family == 0x2a) m_flags.m_value |= 0x10000;
}

// ---- AiDefenseClass -------------------------------------------------------------------------------------------------

AiDefenseClass::AiDefenseClass(DatRecord* record) : AiClassRecordBase(AiRecordKey(record)), m_attackStrength(0), m_shieldStrength(0) {
    Fill(static_cast<DefenseFacilitySD*>(record));
}

AiDefenseClass::~AiDefenseClass() { REB_KEEP_VPTR_STORE(); }

int AiDefenseClass::GetValue(const PersistentInt* mask) {
    const uint32_t bits = mask->m_value;
    if (bits & 0x10000) return m_shieldStrength;
    if (bits & 0x40000) return m_attackStrength;
    if (bits & 0x1) return m_valueB;
    if (bits & 0x4) return m_valueA;
    return 0;
}

void AiDefenseClass::Fill(DefenseFacilitySD* record) {
    m_attackStrength = record->m_attackStrength;
    m_shieldStrength = record->m_shieldStrength;
    m_valueA = record->GetField48();
    m_valueB = record->GetField4c();
    m_valueC = record->m_field50;
    if (record->m_field44 == 0)
        m_view = 1;
    else if (record->m_field40 == 0)
        m_view = 2;
    else
        m_view = 0;
    const uint32_t family = record->m_third;
    if (family == 0x24) m_flags.m_value |= 0x20000;
    if (family == 0x25) m_flags.m_value |= 0x40000;
    if (family == 0x23) m_flags.m_value |= 0x80000;
    if (family == 0x22) m_flags.m_value |= 0x80000;
}

// ---- AiTroopClass ---------------------------------------------------------------------------------------------------

AiTroopClass::AiTroopClass(DatRecord* record)
    : AiClassRecordBase(AiRecordKey(record)), m_attackStrength(0), m_defenseStrength(0), m_bombardmentDefense(0), m_detection(0) {
    Fill(static_cast<TroopSD*>(record));
}

AiTroopClass::~AiTroopClass() { REB_KEEP_VPTR_STORE(); }

int AiTroopClass::GetValue(const PersistentInt* mask) {
    const uint32_t bits = mask->m_value;
    if (bits & 0x1000) return m_attackStrength;
    if (bits & 0x2000) return m_defenseStrength;
    if (bits & 0x1) return m_valueB;
    if (bits & 0x2) return m_detection;
    if (bits & 0x4) return m_valueA;
    return 0;
}

void AiTroopClass::Fill(TroopSD* record) {
    m_attackStrength = record->m_attackStrength;
    m_defenseStrength = record->m_defenseStrength;
    m_bombardmentDefense = record->m_bombardmentDefense;
    m_detection = record->m_field5c;
    m_valueA = record->GetField48();
    m_valueB = record->GetField4c();
    m_valueC = record->m_field50;
    m_view = ViewOfRecord(record);
    if (m_defenseStrength <= m_attackStrength) m_flags.m_value |= 0x1000;
    if (m_attackStrength <= m_defenseStrength) m_flags.m_value |= 0x2000;
}

// ---- AiFighterClass -------------------------------------------------------------------------------------------------

AiFighterClass::AiFighterClass(DatRecord* record)
    : AiClassRecordBase(AiRecordKey(record)), m_x30(0), m_x34(0), m_x38(0), m_x3c(0), m_x40(0), m_x44(0), m_x48(0), m_squadronSize(0) {
    Fill(static_cast<SdRecordGrid*>(static_cast<SdRecord*>(record)));
}

AiFighterClass::~AiFighterClass() { REB_KEEP_VPTR_STORE(); }

int AiFighterClass::ScaleX30(int amount) { return (m_x30 * amount) / m_squadronSize; }
int AiFighterClass::ScaleX34(int amount) { return (m_x34 * amount) / m_squadronSize; }
int AiFighterClass::ScaleX38(int amount) { return (m_x38 * amount) / m_squadronSize; }

int AiFighterClass::GetValue(const PersistentInt* mask) {
    const uint32_t bits = mask->m_value;
    if (bits & 0x200) return m_x30;
    if (bits & 0x400) return m_x34;
    if (bits & 0x800) return m_x40;
    if (bits & 0x1) return m_valueB;
    if (bits & 0x2) return m_x44;
    if (bits & 0x4) return m_valueA;
    if (bits & 0x10000) return m_x38;
    if (bits & 0x8000) return m_x48;
    if (bits & 0x20000) return m_x3c;
    return 0;
}

void AiFighterClass::Fill(SdRecordGrid* record) {
    FighterSD* fighter = static_cast<FighterSD*>(record);
    m_squadronSize = fighter->m_squadronSize;
    m_x38 = (m_squadronSize * 0x19) / 10;
    m_x3c = record->m_field68;
    m_x40 = fighter->m_bombardmentDefense;
    m_x48 = fighter->m_torpedoes * m_squadronSize;
    int firepower = static_cast<int>(record->GetGridValue(0, 1));
    int secondary = static_cast<int>(record->GetGridValue(0, 2));
    firepower = firepower * 100 + (secondary * 100) / 6;
    if (0 < firepower) {
        const int divisorA = static_cast<int>(record->GetGridValue(0, 2));
        const int divisorB = static_cast<int>(record->GetGridValue(0, 1));
        m_x30 = (firepower * m_squadronSize) / (divisorA + divisorB);
    }
    const int grid02 = static_cast<int>(record->GetGridValue(0, 2));
    if (0 < grid02) {
        const int divisor = static_cast<int>(record->GetGridValue(0, 2));
        const int value = (grid02 * m_x3c * m_squadronSize * 10) / divisor;
        m_x34 = value;
        const int bonus = static_cast<int>(record->m_field60) * value;
        m_x34 = bonus / 8 + value;
    }
    m_valueA = record->GetField48();
    m_valueB = record->GetField4c();
    m_valueC = record->m_field50;
    m_x44 = record->m_field5c;
    m_flags.m_value |= 0x200000;
    m_view = ViewOfRecord(record);
}

// ---- AiCapitalShipClass ---------------------------------------------------------------------------------------------

AiCapitalShipClass::AiCapitalShipClass(DatRecord* record)
    : AiClassRecordBase(AiRecordKey(record)), m_x30(0), m_x34(0), m_x38(0), m_x3c(0), m_x40(0), m_x44(0), m_x48(0), m_x4c(0),
      m_x50(0), m_x54(0), m_x58(0), m_x5c(0) {
    Fill(static_cast<SdRecordGrid*>(static_cast<SdRecord*>(record)));
}

AiCapitalShipClass::~AiCapitalShipClass() { REB_KEEP_VPTR_STORE(); }

int AiCapitalShipClass::ScaleA(int amount) {
    int result = 0;
    if (0 < m_x50) result = (m_x30 * amount) / m_x50;
    return result;
}

int AiCapitalShipClass::ScaleB(int amount) {
    int result = 0;
    if (0 < m_x50) result = (m_x34 * amount) / m_x50;
    return result;
}

int AiCapitalShipClass::ScaleC(int amount) {
    int result = 0;
    if (0 < m_x54) result = (m_x3c * amount) / m_x54;
    return result;
}

int AiCapitalShipClass::GetValue(const PersistentInt* mask) {
    const uint32_t bits = mask->m_value;
    if (bits & 0x200) return m_x30;
    if (bits & 0x80000) return m_x58;
    if (bits & 0x400) return m_x34;
    if (bits & 0x800) return m_x38;
    if (bits & 0x80) return m_x44;
    if (bits & 0x100) return m_x48;
    if (bits & 0x1) return m_valueB;
    if (bits & 0x2) return m_x4c;
    if (bits & 0x4) return m_valueA;
    if (bits & 0x10000) return m_x3c;
    return 0;
}

void AiCapitalShipClass::Fill(SdRecordGrid* record) {
    CapitalShipSD* ship = static_cast<CapitalShipSD*>(record);
    m_x30 = 0;
    m_x34 = 0;
    m_x3c = 0;
    m_x40 = ship->m_hull;
    m_x44 = ship->m_fighterCapacity;
    m_x48 = ship->m_troopCapacity;
    m_x50 = ship->m_weaponRechargeRate;
    m_x54 = ship->m_shieldRechargeRate;
    int divisor = 0;

    if (InRange(record->m_third, 0x18, 0x1c)) {
        m_flags.m_value |= 0x80;
    } else {
        if (0 < ship->m_gravityWellProjector) m_flags.m_value |= 0x400;
        m_flags.m_value |= 0x100000;
        m_x3c = ship->m_shieldRechargeRate * 100;

        const int g00 = static_cast<int>(record->GetGridValue(0, 0));
        const int g10 = static_cast<int>(record->GetGridValue(1, 0));
        const int g20 = static_cast<int>(record->GetGridValue(2, 0));
        const int g30 = static_cast<int>(record->GetGridValue(3, 0));
        int v0 = g00 * 100 + static_cast<int>(record->GetGridValue(0, 1)) * 100;
        int v1 = g10 * 100 + static_cast<int>(record->GetGridValue(1, 1)) * 100;
        int v2 = g20 * 100 + static_cast<int>(record->GetGridValue(2, 1)) * 100;
        int v3 = g30 * 100 + static_cast<int>(record->GetGridValue(3, 1)) * 100;
        if ((m_flags.m_value & 0x400) == 0 && (0 < v0 || 0 < v1 || 0 < v2 || 0 < v3)) m_flags.m_value |= 1;
        v0 += (static_cast<int>(record->GetGridValue(0, 2)) * 100) / 6;
        v1 += (static_cast<int>(record->GetGridValue(1, 2)) * 100) / 6;
        v2 += (static_cast<int>(record->GetGridValue(2, 2)) * 100) / 6;
        v3 += (static_cast<int>(record->GetGridValue(3, 2)) * 100) / 6;
        if (0 < v0 || 0 < v1 || 0 < v2 || 0 < v3) {
            const int row = IndexOfLargest(v0, v1, v2, v3);
            const int chosen[4] = {v0, v1, v2, v3};
            if (row >= 0) {
                divisor = static_cast<int>(record->GetGridValue(row, 0) + record->GetGridValue(row, 1) +
                                           record->GetGridValue(row, 2));
                m_x30 = chosen[row];
            }
            m_x30 = (ship->m_weaponRechargeRate * m_x30) / divisor;
        }

        const int s0 = static_cast<int>(record->GetGridValue(0, 2));
        const int s1 = static_cast<int>(record->GetGridValue(1, 2));
        const int s2 = static_cast<int>(record->GetGridValue(2, 2));
        const int s3 = static_cast<int>(record->GetGridValue(3, 2));
        if ((m_flags.m_value & 0x400) == 0 && m_x48 == 0 && (0 < s0 * 10 || 0 < s1 * 10 || 0 < s2 * 10 || 0 < s3 * 10))
            m_flags.m_value |= 2;
        const int w0 = s0 * 10 + (static_cast<int>(record->GetGridValue(0, 0)) * 10) / 2;
        const int w1 = s1 * 10 + (static_cast<int>(record->GetGridValue(1, 0)) * 10) / 2;
        const int w2 = s2 * 10 + (static_cast<int>(record->GetGridValue(2, 0)) * 10) / 2;
        const int w3 = s3 * 10 + (static_cast<int>(record->GetGridValue(3, 0)) * 10) / 2;
        if (!(w0 < 1 && w1 < 1 && w2 < 1 && w3 < 1)) {
            const int row = IndexOfLargest(w0, w1, w2, w3);
            const int chosen[4] = {w0, w1, w2, w3};
            if (row >= 0) {
                divisor = static_cast<int>(record->GetGridValue(row, 0) + record->GetGridValue(row, 2));
                m_x34 = chosen[row];
            }
            m_x34 = (static_cast<int>(record->m_field68) * ship->m_weaponRechargeRate * m_x34) / divisor;
        }
    }

    m_x38 = ship->m_bombardmentModifier != 0;
    m_valueA = record->GetField48();
    m_valueB = record->GetField4c();
    m_valueC = record->m_field50;
    m_x4c = record->m_field5c;
    const int scaled = m_x30 * 10;
    m_x58 = scaled / m_valueB;
    m_x5c = scaled / m_valueA;
    const uint32_t field44 = record->m_field44;
    if (ship->m_hull < 0x44c)
        m_flags.m_value |= 0x10;
    else if (ship->m_hull < 2000)
        m_flags.m_value |= 0x20;
    else
        m_flags.m_value |= 0x40;
    m_view = field44 == 0 ? 1 : 2;
    if (m_x48 != 0) {
        const uint32_t before = m_flags.m_value;
        m_flags.m_value = before | 0x200;
        if ((before & 0x403) == 0 && (before & 0x100000) != 0) m_flags.m_value = before | 0x204;
        if (m_flags.m_value & 1) m_flags.m_value |= 8;
    }
    if (m_x44 != 0) m_flags.m_value |= 0x100;
    if (m_x38 != 0) m_flags.m_value |= 0x800;
}
