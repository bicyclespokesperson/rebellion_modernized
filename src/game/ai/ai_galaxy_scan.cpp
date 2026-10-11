#include "ai/ai_galaxy_scan.h"

#include "ai/ai_class_records.h"
#include "ai/ai_common.h"
#include "ai/ai_galaxy_analysis.h"
#include "core/checked_new.h"
#include "economy/defense_facility.h"
#include "economy/manu_mgr.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "events/game_clock_hooks.h"
#include "fleet/fight_squad.h"
#include "fleet/troop_reg.h"
#include "galaxy/sector.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"

using ai::InFamilies;
using ai::KeyInFamilies;

namespace {

ObjectKey KeyOfObject(GameObjNotifier* object) {
    ObjectKey key;
    GetObjectKey(object, &key);
    return key;
}

uint32_t StateOf(GameObjNotifier* object) { return static_cast<GameObj*>(object)->m_stateFlags; }

constexpr uint32_t kSystemPopulated = 1;
constexpr uint32_t kSystemExplored = 2;
constexpr uint32_t kSystemUprising = 4;
constexpr uint32_t kSystemBlockade = 0x20;
constexpr uint32_t kObjectUsable = 1;
constexpr uint32_t kObjectDestroyed = 8;
constexpr int kCorePlanetId = 0x109;

struct SystemMasks {
    uint32_t requireA, requireB, forbidA, forbidB;

    bool Matches(const AiSystemScan* system) const {
        const uint32_t a = system->m_flagsA.m_value;
        const uint32_t b = system->m_flagsB.m_value;
        return (a & requireA) == requireA && (forbidA & a) == 0 && (b & requireB) == requireB && (forbidB & b) == 0;
    }
};

int DistanceSquared(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

}  // namespace

// ---- AiSystemScan -------------------------------------------------------------------------------------------------------

AiSystemScan::AiSystemScan() : ObjectKeyNode(), m_flagsA(0), m_flagsB(0) {
    m_controlSide = 0;
    m_sector = nullptr;
    m_classes = nullptr;
    m_x38 = -1;
    m_scanDay = 0;
    m_x = 0;
    m_y = 0;
    for (int& value : m_stat) value = 0;
    m_dirty = 0;
}

AiSystemScan::AiSystemScan(System* system) : ObjectKeyNode(KeyOfObject(system)), m_flagsA(0), m_flagsB(0) {
    m_controlSide = 0;
    m_x38 = -1;
    m_scanDay = 0;
    uint32_t position;
    system->GetRecordField4c(&position);
    m_x = static_cast<int16_t>(position);
    m_y = static_cast<int16_t>(position >> 16);
    m_sector = nullptr;
    m_classes = nullptr;
    for (int& value : m_stat) value = 0;
    m_dirty = 1;
}

AiSystemScan::~AiSystemScan() {
    m_sector = nullptr;
    m_classes = nullptr;

}

void AiSystemScan::Load(istream* in) {
    ObjectKeyNode::Load(in);
    ReadValue32(in, &m_dirty);
    ReadValue32(in, &m_controlSide);
    ReadValue32(in, &m_x38);
    ReadValue32(in, &m_scanDay);
    m_flagsA.Read(in);
    m_flagsB.Read(in);
    ReadValue16(in, &m_x);
    ReadValue16(in, &m_y);
    for (int& value : m_stat) ReadValue32(in, &value);
    m_dirty = 1;
}

void AiSystemScan::Save(ostream* out) {
    ObjectKeyNode::Save(out);
    WriteValue32(out, &m_dirty);
    WriteValue32(out, &m_controlSide);
    WriteValue32(out, &m_x38);
    WriteValue32(out, &m_scanDay);
    m_flagsA.Write(out);
    m_flagsB.Write(out);
    WriteValue16(out, &m_x);
    WriteValue16(out, &m_y);
    for (int& value : m_stat) WriteValue32(out, &value);
}

void AiSystemScan::MarkDirty() {
    m_dirty = 1;
    m_sector->MarkDirty();
}

void AiSystemScan::Scan(int view) {
    if (m_dirty == 0) return;
    m_dirty = 0;
    for (int i = 0; i < kStatCount - 1; ++i) m_stat[i] = 0;
    ObjectKey ownKey;
    GetKey(&ownKey);
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(view, &ownKey));
    uint32_t& flagsA = reinterpret_cast<uint32_t&>(m_flagsA.m_value);
    uint32_t& flagsB = reinterpret_cast<uint32_t&>(m_flagsB.m_value);
    flagsA = 0;
    const uint32_t ownSide = view == 1 ? 1 : (view == 2 ? 2 : 0);
    const uint32_t otherSide = view == 1 ? 2 : 1;
    m_controlSide = static_cast<int>(system->ControlSide());
    Stat(0x98) = 0;
    if (system->m_stateFlags & kObjectDestroyed) flagsB |= 8;
    if (static_cast<uint32_t>(m_controlSide) == ownSide)
        flagsA |= 4;
    else if (static_cast<uint32_t>(m_controlSide) == otherSide)
        flagsA |= 8;
    else
        flagsA |= 0x10;
    if (KeyInFamilies(m_key, 0x90, 0x92)) flagsB |= 4;
    if ((system->m_flags & kSystemExplored) == 0) return;

    flagsA |= 0x20000000;
    Stat(0x44) = system->m_energy;
    Stat(0x48) = system->m_rawMaterial;
    Stat(0x70) = system->m_energy - system->m_energyAllocated;
    if (0 < Stat(0x70)) flagsA |= 0x20;
    Stat(0x6c) = system->m_rawMaterial - system->m_rawMaterialAllocated;
    if (0 < Stat(0x6c) && (flagsA & 0x20)) flagsA |= 0x40;
    if ((system->m_flags & kSystemBlockade) != 0) {
        flagsA |= 2;
        if ((flagsA & 0x18) != 0) m_scanDay = GameHookDay();
    }
    if ((flagsA & 4) != 0) Stat(0x40) = 100;

    auto classOf = [this](GameObjNotifier* object) -> AiClassRecordBase* {
        ObjectKey recordKey;
        GetObjectRecordKey(object, &recordKey);
        return m_classes->Find(&recordKey);
    };

    uint32_t troopsUsable = 0;
    {
        TroopRegIterator troops(system, 3);
        troops.Begin();
        while (troops.m_current) {
            AiClassRecordBase* troopClass = classOf(troops.m_current);
            troopsUsable |= StateOf(troops.m_current) & kObjectUsable;
            ++Stat(0x74);
            Stat(0x7c) += troopClass->m_valueB;
            troops.Advance();
        }
    }
    if ((system->m_flags & kSystemPopulated) == 0) return;

    if ((m_key.m_value & 0xffffff) == kCorePlanetId) flagsB |= 0x20000000;
    if (CountAllFacilities(system, m_controlSide, 1) != 0) flagsA |= 0x80000000;
    if ((system->m_flags & kSystemUprising) != 0) flagsA |= 1;
    flagsA |= 0x40000000;
    Stat(0x40) = system->GetLoyaltyFor(view);
    Stat(0x4c) = CountProductionFacilityType2C(system, m_controlSide, 3);
    Stat(0x50) = CountProductionFacilityType2D(system, m_controlSide, 3);

    auto scanManufacturing = [&](auto& facilities, int& count, int managerSlot, uint32_t reservedFlag, uint32_t idleFlag,
                                 uint32_t idleManyFlag) {
        uint32_t usable = 0;
        facilities.Begin();
        while (facilities.m_current) {
            AiClassRecordBase* facilityClass = classOf(facilities.m_current);
            usable |= StateOf(facilities.m_current) & kObjectUsable;
            ++count;
            Stat(0x84) += facilityClass->m_valueB;
            facilities.Advance();
        }
        if (0 < count && usable != 0 && (flagsA & 3) == 0) {
            ManuMgr* manager = static_cast<ManuMgr*>(system->FindManufacturingManagerBySlot123(managerSlot));
            if (manager) {
                if (manager->m_reserved != 0) flagsA |= reservedFlag;
                if (manager->m_remainingGameObjCount == 0) {
                    flagsA |= idleFlag;
                    if (1 < count && idleManyFlag != 0) flagsA |= idleManyFlag;
                }
            }
        }
    };

    {
        ManufacturingFacility28Iterator facilities(system, 3);
        scanManufacturing(facilities, Stat(0x54), 0, 0x400, 0x80, 0x400);
    }
    {
        ManufacturingFacility29Iterator facilities(system, 3);
        scanManufacturing(facilities, Stat(0x58), 2, 0x800, 0x100, 0x400);
    }
    {
        ManufacturingFacility2AIterator facilities(system, 3);
        scanManufacturing(facilities, Stat(0x5c), 1, 0x1000, 0x200, 0);
    }

    uint32_t defenseUsable = 0;
    {
        DefenseFacility24Iterator defenses(system, 3);
        defenses.Begin();
        while (defenses.m_current) {
            AiClassRecordBase* defenseClass = classOf(defenses.m_current);
            defenseUsable |= StateOf(defenses.m_current) & kObjectUsable;
            ++Stat(0x60);
            Stat(0x80) += defenseClass->m_valueB;
            defenses.Advance();
        }
    }
    if (defenseUsable != 0 && 1 < Stat(0x60)) flagsA |= 0x400;
    {
        DefenseFacility25Iterator defenses(system, 3);
        defenses.Begin();
        while (defenses.m_current) {
            AiClassRecordBase* defenseClass = classOf(defenses.m_current);
            ++Stat(0x64);
            Stat(0x80) += defenseClass->m_valueB;
            defenses.Advance();
        }
    }
    defenseUsable = 0;
    {
        DefenseFacility23Iterator defenses(system, 3);
        defenses.Begin();
        while (defenses.m_current) {
            AiClassRecordBase* defenseClass = classOf(defenses.m_current);
            defenseUsable |= StateOf(defenses.m_current) & kObjectUsable;
            ++Stat(0x68);
            Stat(0x80) += defenseClass->m_valueB;
            defenses.Advance();
        }
    }
    {
        DefenseFacility22Iterator defenses(system, 3);
        defenses.Begin();
        while (defenses.m_current) {
            AiClassRecordBase* defenseClass = classOf(defenses.m_current);
            defenseUsable |= StateOf(defenses.m_current) & kObjectUsable;
            ++Stat(0x68);
            Stat(0x80) += defenseClass->m_valueB;
            defenses.Advance();
        }
    }
    if (defenseUsable != 0 && 1 < Stat(0x68)) flagsA |= 0x400;
    uint32_t squadsUsable = 0;
    {
        FightSquadIterator squads(system, 3);
        squads.Begin();
        while (squads.m_current) {
            AiClassRecordBase* squadClass = classOf(squads.m_current);
            squadsUsable |= StateOf(squads.m_current) & kObjectUsable;
            ++Stat(0x78);
            Stat(0x88) += squadClass->m_valueB;
            squads.Advance();
        }
    }
    if (squadsUsable != 0 && 0 < Stat(0x78)) flagsA |= 0x400;

    if ((flagsA & 4) != 0) {
        Stat(0x8c) = 1;
        if (0 < 0x3c - Stat(0x40)) Stat(0x8c) = (0x45 - Stat(0x40)) / 10 + 1;
        if ((flagsA & 1) != 0) Stat(0x8c) <<= 1;
        if (troopsUsable != 0 && Stat(0x8c) < Stat(0x74)) flagsA |= 0x400;
        int needed = (Stat(0x50) + Stat(0x4c)) / 2 + Stat(0x5c) + Stat(0x54) + Stat(0x58);
        if (needed < Stat(0x8c)) needed = Stat(0x8c);
        Stat(0x90) = Stat(0x74) - needed;
        if (Stat(0x90) < 0) flagsA |= 0x10000000;
        Stat(0x94) = Stat(0x88) + Stat(0x84) + Stat(0x80) + Stat(0x7c);
    }
}

// ---- AiSectorScan -------------------------------------------------------------------------------------------------------

AiSectorScan::AiSectorScan() : ObjectKeyNode(), m_flagsA(0), m_flagsB(0) {
    m_view = 0;
    m_x2c = 0;
    m_analysis = nullptr;
    m_x = 0;
    m_y = 0;
    for (AiSystemScan*& system : m_systems) system = nullptr;
    m_dirty = 0;
}

AiSectorScan::AiSectorScan(int view) : ObjectKeyNode(), m_flagsA(0), m_flagsB(0) {
    m_analysis = nullptr;
    m_view = view;
    for (AiSystemScan*& system : m_systems) system = nullptr;
    m_dirty = 1;
}

AiSectorScan::~AiSectorScan() {
    m_analysis = nullptr;
    for (AiSystemScan*& system : m_systems) system = nullptr;

}

void AiSectorScan::Attach(Sector* sector, AiSystemScanSet* systems) {
    m_key = KeyOfObject(sector);
    m_x2c = 0;
    uint32_t position;
    sector->GetPosition(&position);
    m_x = static_cast<int16_t>(position);
    m_y = static_cast<int16_t>(position >> 16);
    if (sector->GetGroup() == 1) m_flagsB.m_value |= 4;
    SystemIterator iterator(sector, 4);
    iterator.Begin();
    int index = 0;
    while (iterator.m_current) {
        ObjectKey systemKey = KeyOfObject(iterator.m_current);
        SetSystem(static_cast<uint16_t>(index), &systemKey, systems);
        iterator.Advance();
        ++index;
    }
    m_dirty = 1;
}

void AiSectorScan::ReadRecord(istream* in, AiSystemScanSet* systems) {
    ObjectKey key;
    ObjectKeyNode::Load(in);
    ReadValue32(in, &m_dirty);
    ReadValue32(in, &m_view);
    m_flagsA.Read(in);
    m_flagsB.Read(in);
    ReadValue16(in, &m_x2c);
    ReadValue16(in, &m_x);
    ReadValue16(in, &m_y);
    for (uint16_t index = 0; index < kSystemCount; ++index) {
        key.Read(in);
        SetSystem(index, &key, systems);
    }
    for (int& value : m_stat) ReadValue32(in, &value);
    m_dirty = 1;
}

// BUG(original): the position is written before the first halfword but read after it.
void AiSectorScan::Save(ostream* out) {
    ObjectKeyNode::Save(out);
    WriteValue32(out, &m_dirty);
    WriteValue32(out, &m_view);
    m_flagsA.Write(out);
    m_flagsB.Write(out);
    WriteValue16(out, &m_x);
    WriteValue16(out, &m_y);
    WriteValue16(out, &m_x2c);
    for (AiSystemScan* system : m_systems) {
        ObjectKey key;
        system->GetKey(&key);
        key.Write(out);
    }
    for (int& value : m_stat) WriteValue32(out, &value);
}

AiRankedList* AiSectorScan::RankSystemsByStat(uint32_t requireA, uint32_t requireB, uint32_t forbidA, uint32_t forbidB,
                                              uint32_t statIndex, int descending) {
    const SystemMasks masks{requireA, requireB, forbidA, forbidB};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSystemScan* system : m_systems) {
        if (!masks.Matches(system)) continue;
        ObjectKey key;
        AiRankedItem* item = NewOrNull<AiRankedItem>(*system->GetKey(&key), system->m_stat[statIndex & 0xffff]);
        list->AddItem(item);
    }
    list->Number();
    return list;
}

AiRankedList* AiSectorScan::RankSystemsByDistance(const ObjectKey* target, uint32_t requireA, uint32_t requireB,
                                                  uint32_t forbidA, uint32_t forbidB, int descending) {
    const SystemMasks masks{requireA, requireB, forbidA, forbidB};
    AiRankedList* list = nullptr;
    ObjectKey key = *target;
    for (;;) {
        if (InFamilies(key.Family(), 0x90, 0x98)) break;
        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_view, &key);
        if (!object || !object->m_parent) break;
        ObjectKey parentKey;
        key = *GetObjectKey(object->m_parent, &parentKey);
    }
    if (InFamilies(key.Family(), 0x90, 0x98)) {
        AiSystemScan* targetSystem = m_analysis->m_systemScans.FindScan(key);
        if (targetSystem) {
            list = NewOrNull<AiRankedList>(descending);
            if (list) {
                for (AiSystemScan* system : m_systems) {
                    if (!masks.Matches(system)) continue;
                    const int distance = DistanceSquared(targetSystem->m_x, targetSystem->m_y, system->m_x, system->m_y);
                    ObjectKey systemKey;
                    AiRankedItem* item = NewOrNull<AiRankedItem>(*system->GetKey(&systemKey), distance);
                    list->AddItem(item);
                }
                list->Number();
            }
        }
    }
    return list;
}

void AiSectorScan::MarkDirty() { m_dirty = 1; }

int AiSectorScan::Update(int view) {
    if (m_dirty == 0) return 0;
    m_dirty = 0;
    m_flagsA.m_value = 0;
    for (int& value : m_stat) value = 0;
    for (AiSystemScan* system : m_systems) {
        system->Scan(view);
        Accumulate(system->m_stat, &system->m_flagsA.m_value, &system->m_flagsB.m_value);
    }
    if (Stat(0x60) == 10) m_flagsA.m_value |= 1;
    if (Stat(0x64) == 10) m_flagsA.m_value |= 2;
    return 1;
}

void AiSectorScan::SetSystem(uint16_t index, const ObjectKey* key, AiSystemScanSet* systems) {
    AiSystemScan* system = systems->FindScan(*key);
    if (index < kSystemCount) {
        m_systems[index] = system;
        system->m_sector = this;
    }
}

void AiSectorScan::Accumulate(const int* stats, const int* flagsA, const int* flagsB) {
    uint32_t& mine = reinterpret_cast<uint32_t&>(m_flagsA.m_value);
    if (*flagsA & 0x20000000) ++Stat(0x60);
    if (*flagsA & 0x40000000) ++Stat(0x64);
    if ((*flagsA & 4) == 0) {
        if ((*flagsA & 8) == 0) {
            mine |= 0x40;
            ++Stat(0x70);
            Stat(0xbc) += stats[0];
        } else {
            mine |= 0x20;
            ++Stat(0x6c);
            Stat(0xc0) += stats[0];
        }
    } else {
        mine |= 0x10;
        ++Stat(0x68);
        Stat(0x78) += stats[3];
        Stat(0x7c) += stats[4];
        Stat(0x80) += stats[5];
        Stat(0x84) += stats[6];
        Stat(0x88) += stats[7];
        Stat(0x8c) += stats[8];
        Stat(0x90) += stats[9];
        Stat(0x94) += stats[10];
        Stat(0x98) += stats[0xe];
        Stat(0x9c) += stats[0xb];
        Stat(0xa0) += stats[0xc];
        Stat(0xa4) += stats[0xf];
        Stat(0xa8) += stats[0x10];
        Stat(0xb0) += stats[0x12];
        Stat(0xac) += stats[0x11];
        Stat(0xb4) += stats[0x15];
        Stat(0xb8) += stats[0];
        if (*flagsA & 0x20) mine |= 0x80;
        if (*flagsA & 0x40) mine |= 0x100;
        if (*flagsA & 0x80) mine |= 0x1000;
        if (*flagsA & 0x100) mine |= 0x2000;
        if (*flagsA & 0x200) mine |= 0x4000;
        if ((*flagsA & 0x400) == 0) mine |= 0x10000;
        if ((*flagsA & 0x800) == 0) mine |= 0x20000;
        if ((*flagsA & 0x1000) == 0) mine |= 0x40000;
        if (*flagsA & 0x10000000) mine |= 0x1000000;
        if (*flagsA & 2) mine |= 0x10000000;
        if (*flagsA & 1) {
            mine |= 0x20000000;
            Stat(0xc4) -= stats[0x14];
        }
        if (*flagsA & 0x400) mine |= 0x8000;
    }
    if (*flagsB & 8) ++Stat(0x74);
    if (*flagsB & 0x20000000) reinterpret_cast<uint32_t&>(m_flagsB.m_value) |= 0x1000000;
    if (*flagsA & 0x80000000) mine |= 0x2000000;
}

// ---- AiSectorScanSet ----------------------------------------------------------------------------------------------------

namespace {

uint32_t CountOf(AvlTree& tree) { return static_cast<uint16_t>(tree.Count()); }

}  // namespace

AiSectorScanSet::AiSectorScanSet() : ObjectKeySet(), m_view(0), m_cursor(nullptr), m_cursorObject(nullptr) {}

AiSectorScanSet::~AiSectorScanSet() {
    m_cursorObject = nullptr;
    m_cursor = nullptr;

}

AiSectorScan* AiSectorScanSet::FindScan(ObjectKey key) { return static_cast<AiSectorScan*>(Find(&key)); }

void AiSectorScanSet::LoadAll(istream* in, AiSystemScanSet* systems, AiGalaxyAnalysis* analysis) {
    ObjectKey key;
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (; count != 0; --count) ReadOne(in, systems, analysis);
    uint32_t flag;
    ReadValue32(in, &flag);
    if (flag == 0) {
        m_cursorObject = nullptr;
    } else {
        key.Read(in);
        m_cursorObject = GetObjectInFamilies_80_90(m_view, &key);
    }
    ReadValue32(in, &flag);
    if (flag == 0) {
        m_cursor = nullptr;
    } else {
        key.Read(in);
        m_cursor = FindScan(key);
    }
}

void AiSectorScanSet::Save(ostream* out) {
    ObjectKey key;
    WriteValue32(out, &m_view);
    uint32_t count = CountOf(*this);
    WriteValue32(out, &count);
    for (ObjectKeyNode* node = First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) node->Save(out);
    if (!m_cursorObject) {
        uint32_t none = 0;
        WriteValue32(out, &none);
    } else {
        GetObjectKey(m_cursorObject, &key);
        uint32_t family = key.m_value >> 24;
        WriteValue32(out, &family);
        key.Write(out);
    }
    if (!m_cursor) {
        uint32_t none = 0;
        WriteValue32(out, &none);
    } else {
        m_cursor->GetKey(&key);
        uint32_t family = key.m_value >> 24;
        WriteValue32(out, &family);
        key.Write(out);
    }
}

int AiSectorScanSet::BuildStep(AiSystemScanSet* systems, AiGalaxyAnalysis* analysis) {
    int finished = 0;
    if (!m_cursorObject) {
        Clear();
        m_cursorObject = GetFirstObjectInFamilies_80_90(m_view);
        return finished;
    }
    m_cursor = NewOrNull<AiSectorScan>(m_view);
    if (m_cursor) {
        m_cursor->Attach(static_cast<Sector*>(m_cursorObject), systems);
        m_cursor->m_analysis = analysis;
        Add(m_cursor);
    }
    const FamilyRange range(0x80, 0x90);
    m_cursorObject = static_cast<GameObj*>(m_cursorObject)->FindNextSibling(&range, 1);
    if (!m_cursorObject) {
        finished = 1;
        m_cursor = nullptr;
    }
    return finished;
}

void AiSectorScanSet::NoteSector(const ObjectKey* key) {
    ObjectKey copy = *key;
    AiSectorScan* scan = FindScan(copy);
    if (scan) scan->MarkDirty();
}

void AiSectorScanSet::ReadOne(istream* in, AiSystemScanSet* systems, AiGalaxyAnalysis* analysis) {
    AiSectorScan* scan = NewOrNull<AiSectorScan>();
    if (scan) {
        scan->ReadRecord(in, systems);
        scan->m_analysis = analysis;
        Add(scan);
    }
}

// ---- AiSystemScanSet ----------------------------------------------------------------------------------------------------

AiSystemScanSet::AiSystemScanSet()
    : ObjectKeySet(), m_view(0), m_cursor(nullptr), m_cursorObject(nullptr), m_classes(nullptr) {}

AiSystemScanSet::~AiSystemScanSet() {
    m_cursorObject = nullptr;
    m_cursor = nullptr;
    m_classes = nullptr;

}

AiSystemScan* AiSystemScanSet::FindScan(ObjectKey key) { return static_cast<AiSystemScan*>(Find(&key)); }

void AiSystemScanSet::Load(istream* in) {
    ObjectKey key;
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (; count != 0; --count) ReadOne(in);
    uint32_t flag;
    ReadValue32(in, &flag);
    if (flag == 0) {
        m_cursorObject = nullptr;
    } else {
        key.Read(in);
        m_cursorObject = GetObjectInFamilies_90_98(m_view, &key);
    }
    ReadValue32(in, &flag);
    if (flag == 0) {
        m_cursor = nullptr;
    } else {
        key.Read(in);
        m_cursor = FindScan(key);
    }
}

void AiSystemScanSet::Save(ostream* out) {
    ObjectKey key;
    WriteValue32(out, &m_view);
    uint32_t count = CountOf(*this);
    WriteValue32(out, &count);
    for (ObjectKeyNode* node = First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) node->Save(out);
    if (!m_cursorObject) {
        uint32_t none = 0;
        WriteValue32(out, &none);
    } else {
        GetObjectKey(m_cursorObject, &key);
        uint32_t family = key.m_value >> 24;
        WriteValue32(out, &family);
        key.Write(out);
    }
    if (!m_cursor) {
        uint32_t none = 0;
        WriteValue32(out, &none);
    } else {
        m_cursor->GetKey(&key);
        uint32_t family = key.m_value >> 24;
        WriteValue32(out, &family);
        key.Write(out);
    }
}

int AiSystemScanSet::BuildStep() {
    int finished = 0;
    if (!m_cursorObject) {
        Clear();
        m_cursorObject = GetFirstObjectInFamilies_90_98(m_view);
        return finished;
    }
    ObjectKey key;
    GetObjectKey(m_cursorObject, &key);
    m_cursor = FindScan(key);
    if (!m_cursor) {
        m_cursor = NewOrNull<AiSystemScan>(static_cast<System*>(m_cursorObject));
        if (m_cursor) {
            m_cursor->m_dirty = 1;
            m_cursor->m_classes = m_classes;
            Add(m_cursor);
        }
    }
    const FamilyRange range(0x90, 0x98);
    m_cursorObject = static_cast<GameObj*>(m_cursorObject)->FindNextSibling(&range, 1);
    if (!m_cursorObject) {
        finished = 1;
        m_cursor = nullptr;
    }
    return finished;
}

void AiSystemScanSet::NoteSystem(const ObjectKey* key) {
    ObjectKey copy = *key;
    AiSystemScan* scan = FindScan(copy);
    if (scan) scan->MarkDirty();
}

void AiSystemScanSet::ReadOne(istream* in) {
    AiSystemScan* scan = NewOrNull<AiSystemScan>();
    if (scan) {
        scan->Load(in);
        scan->m_classes = m_classes;
        Add(scan);
    }
}
