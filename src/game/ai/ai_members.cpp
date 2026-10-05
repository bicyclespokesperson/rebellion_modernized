#include "ai/ai_members.h"

#include "ai/ai_agent_bases.h"
#include "ai/ai_character_records.h"
#include "ai/ai_common.h"
#include "ai/ai_planner_decl.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "ai/ai_session_random.h"
#include "economy/side.h"
#include "galaxy/sector.h"
#include "galaxy/system.h"
#include "gameobj/object_name_node.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"

using ai::InFamilies;

namespace {

constexpr uint32_t kDirty = 0x80000000;

uint32_t CountOf(AvlTree& tree) { return static_cast<uint16_t>(tree.Count()); }

// Shared by the sets that save their id-keyed nodes with the node's own Write (AiSystemRecordSet, AiTaskRecordSet).
template <class Set>
void SaveIdNodes(Set* set, ostream* out) {
    WriteValue32(out, &set->m_view);
    uint32_t count = CountOf(*set);
    WriteValue32(out, &count);
    for (AvlNode* node = set->First(); node; node = node->m_successor) static_cast<AiTaskRecord*>(node)->Write(out);
}

template <class Set>
ObjectKeyNode* FirstNode(Set& set) {
    return set.First();
}

}  // namespace

// ---- AiClassTable -----------------------------------------------------------------------------------------------------

AiClassTable::AiClassTable()
    : AvlTree(), m_maxShipTier(0), m_maxTroopTier(0), m_maxFacilityTier(0), m_majorCharacters(0), m_capitalTier(0),
      m_troopTier(0), m_facilityTier(0), m_bestDefense(0), m_x28(0), m_x2c(0), m_x30(0), m_x34(0), m_x38(0), m_x3c(0) {}

AiClassTable::~AiClassTable() { REB_KEEP_VPTR_STORE(); }

int AiClassTable::IsLess(AvlNode* a, AvlNode* b) {
    if (!a || !b) return 0;
    const uint32_t keyA = static_cast<AiClassRecordBase*>(a)->m_key.m_value;
    const uint32_t keyB = static_cast<AiClassRecordBase*>(b)->m_key.m_value;
    const uint32_t familyA = keyA >> 24;
    const uint32_t familyB = keyB >> 24;
    if (familyA < familyB || (familyA == familyB && (keyA & 0xffffff) < (keyB & 0xffffff))) return 1;
    return 0;
}

void AiClassTable::LoadValues(istream* in) {
    ReadValue32(in, &m_capitalTier);
    ReadValue32(in, &m_troopTier);
    ReadValue32(in, &m_facilityTier);
}

void AiClassTable::SaveValues(ostream* out) {
    WriteValue32(out, &m_capitalTier);
    WriteValue32(out, &m_troopTier);
    WriteValue32(out, &m_facilityTier);
}

AiClassRecordBase* AiClassTable::Find(const ObjectKey* key) {
    AvlNode* node = m_root;
    bool found = false;
    const uint32_t id = key->m_value & 0xffffff;
    const uint32_t family = key->m_value >> 24;
    while (node && !found) {
        const uint32_t nodeKey = static_cast<AiClassRecordBase*>(node)->m_key.m_value;
        const uint32_t nodeFamily = nodeKey >> 24;
        if (family < nodeFamily || (family == nodeFamily && id < (nodeKey & 0xffffff))) {
            node = node->m_left;
        } else if (nodeFamily < family || (family == nodeFamily && (nodeKey & 0xffffff) < id)) {
            node = node->m_right;
        } else {
            found = true;
        }
    }
    return static_cast<AiClassRecordBase*>(node);
}

void AiClassTable::Build(int side) {
    auto addClasses = [&](int recordType, uint32_t firstFamily, uint32_t lastFamily, int* maxTier, bool matchSide, auto make) {
        for (DatRecord* record = GetFirstRecordOfType(recordType); record;
             record = static_cast<DatRecord*>(record->m_successor)) {
            if (!InFamilies(record->m_third, firstFamily, lastFamily)) continue;
            AiClassRecordBase* entry = make(record);
            if (!entry) continue;
            Insert(entry);
            if (maxTier && (!matchSide || entry->m_view == side) && *maxTier < entry->m_valueC) *maxTier = entry->m_valueC;
        }
    };
    addClasses(0x14, 0x14, 0x1c, &m_maxShipTier, true, [](DatRecord* r) -> AiClassRecordBase* { return NewOrNull<AiCapitalShipClass>(r); });
    addClasses(0x1c, 0x1c, 0x20, &m_maxShipTier, true, [](DatRecord* r) -> AiClassRecordBase* { return NewOrNull<AiFighterClass>(r); });
    addClasses(0x10, 0x10, 0x14, &m_maxTroopTier, true, [](DatRecord* r) -> AiClassRecordBase* { return NewOrNull<AiTroopClass>(r); });
    addClasses(0x22, 0x22, 0x28, &m_maxFacilityTier, false, [](DatRecord* r) -> AiClassRecordBase* { return NewOrNull<AiDefenseClass>(r); });
    addClasses(0x28, 0x28, 0x2c, &m_maxFacilityTier, false, [](DatRecord* r) -> AiClassRecordBase* { return NewOrNull<AiManufacturingClass>(r); });
    addClasses(0x3c, 0x3c, 0x40, nullptr, false, [](DatRecord* r) -> AiClassRecordBase* { return NewOrNull<AiSpecialForceClass>(r); });
    for (DatRecord* record = GetFirstRecordOfType(0x38); record; record = static_cast<DatRecord*>(record->m_successor)) {
        if (!InFamilies(record->m_third, 0x38, 0x3c)) continue;
        const uint32_t sideBit = static_cast<SdRecord*>(record)->m_field44;
        if (sideBit == 0) {
            if (side == 1) ++m_majorCharacters;
        } else if (side == 2) {
            ++m_majorCharacters;
        }
    }
    SetCapitalShipTier(m_capitalTier, side);
    SetTroopTier(m_troopTier, side);
    SetFacilityTier(m_facilityTier, side);
}

void AiClassTable::SetCapitalShipTier(int tier, int side) {
    ObjectKey unused;
    unused.m_value = 0;
    m_capitalTier = tier;
    m_x28 = Select(&unused, side, tier, 0x200000, 0x400, 1);
    m_x2c = Select(&unused, side, m_capitalTier, 1, 0x200, 1);
    m_x30 = Select(&unused, side, m_capitalTier, 2, 0x400, 1);
    m_x34 = Select(&unused, side, m_capitalTier, 0x100000, 0x80, 1);
    m_x38 = Select(&unused, side, m_capitalTier, 0x100000, 0x100, 1);
    m_x3c = Select(&unused, side, m_capitalTier, 0x800, 0x800, 1);
}

void AiClassTable::SetTroopTier(int tier, int side) {
    ObjectKey unused;
    unused.m_value = 0;
    m_troopTier = tier;
    m_bestDefense = Select(&unused, side, tier, 0x2000, 0x2000, 1);
}

void AiClassTable::SetFacilityTier(int tier, int) { m_facilityTier = tier; }

int AiClassTable::Select(ObjectKey* out, int view, int maxTier, uint32_t flagMask, uint32_t valueMask, int descending) {
    AiRankedSelector selector;
    ObjectKey key;
    int result = 0;
    out->m_value = 0;
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (list) {
        for (AvlNode* node = First(); node; node = node->m_successor) {
            AiClassRecordBase* entry = static_cast<AiClassRecordBase*>(node);
            if (entry->m_view == view && (flagMask & entry->m_flags.m_value) != 0 && entry->m_valueC <= maxTier) {
                key = entry->m_key;
                PersistentInt mask(static_cast<int>(valueMask));
                AiRankedItem* item = NewOrNull<AiRankedItem>(key, entry->GetValue(&mask));
                list->AddItem(item);
            }
        }
        list->Number();
        selector.Add(list);
        ObjectKey picked;
        key = *selector.PickBest(&picked);
        *out = key;
    }
    if ((out->m_value & 0xff000000) != 0) {
        AiClassRecordBase* entry = Find(out);
        PersistentInt mask(static_cast<int>(valueMask));
        result = entry->GetValue(&mask);
    }
    return result;
}

namespace {

// Drops the names of `list` whose class record is missing or lacks one of the requested flags.
void FilterByClassFlags(AiClassTable* table, ObjectNameList* list, uint32_t requiredFlags) {
    AvlNode* node = list->First();
    while (node) {
        ObjectKey key;
        ObjectNameNode* name = static_cast<ObjectNameNode*>(node);
        GetObjectNameNodeKey(name, &key);
        AiClassRecordBase* entry = table->Find(&key);
        if (!entry || (entry->m_flags.m_value & requiredFlags) != requiredFlags) {
            const uint32_t id = static_cast<IdNode*>(node)->m_id;
            node = node->m_successor;
            list->DeleteById(id);
        } else {
            node = node->m_successor;
        }
    }
}

}  // namespace

int AiClassTable::ChooseFromSide(ObjectKey* out, GameObjNotifier* side, int recordType, uint32_t requiredFlags, int mode,
                                 int descending) {
    ObjectNameList list;
    int result = 0;
    out->m_value = 0;
    if (side && static_cast<Side*>(side)->CollectRdRecords(recordType, &list)) FilterByClassFlags(this, &list, requiredFlags);

    if (mode == 0) {
        const int count = static_cast<int>(CountOf(list));
        const int choice = SessionRandomNext(count);
        int index = 0;
        AvlNode* node = list.First();
        if (node) {
            do {
                if (choice <= index) break;
                node = node->m_successor;
                ++index;
            } while (node);
            if (node) {
                ObjectKey key;
                GetObjectNameNodeKey(static_cast<ObjectNameNode*>(node), &key);
                *out = key;
            }
        }
    } else {
        AiRankedSelector selector;
        ObjectKey key;
        AiRankedList* ranked = NewOrNull<AiRankedList>(descending);
        if (ranked) {
            for (AvlNode* node = list.First(); node; node = node->m_successor) {
                ObjectKey nameKey;
                GetObjectNameNodeKey(static_cast<ObjectNameNode*>(node), &nameKey);
                AiClassRecordBase* entry = Find(&nameKey);
                if (entry) {
                    key = entry->m_key;
                    PersistentInt mask(mode);
                    AiRankedItem* item = NewOrNull<AiRankedItem>(key, entry->GetValue(&mask));
                    if (item) ranked->AddItem(item);
                }
            }
            ranked->Number();
            selector.Add(ranked);
            ObjectKey picked;
            key = *selector.PickBest(&picked);
            *out = key;
        }
    }
    if ((out->m_value & 0xff000000) != 0) {
        AiClassRecordBase* entry = Find(out);
        result = entry->m_valueB;
    }
    return result;
}

AiRankedList* AiClassTable::RankClasses(GameObjNotifier* side, int recordType, uint32_t requiredFlags, uint32_t valueMask,
                                        int descending) {
    ObjectNameList list;
    ObjectKey key;
    if (side && static_cast<Side*>(side)->CollectRdRecords(recordType, &list)) FilterByClassFlags(this, &list, requiredFlags);
    AiRankedList* ranked = NewOrNull<AiRankedList>(descending);
    if (ranked) {
        for (AvlNode* node = list.First(); node; node = node->m_successor) {
            ObjectKey nameKey;
            GetObjectNameNodeKey(static_cast<ObjectNameNode*>(node), &nameKey);
            AiClassRecordBase* entry = Find(&nameKey);
            if (entry) {
                key = entry->m_key;
                PersistentInt mask(static_cast<int>(valueMask));
                AiRankedItem* item = NewOrNull<AiRankedItem>(key, entry->GetValue(&mask));
                if (item) ranked->AddItem(item);
            }
        }
        ranked->Number();
    }
    return ranked;
}

// ---- AiTaskRecordSet ----------------------------------------------------------------------------------------------------

AiTaskRecordSet::AiTaskRecordSet() : IdAvlTree(), m_view(0), m_owner(nullptr) {}

AiTaskRecordSet::~AiTaskRecordSet() {
    m_owner = nullptr;
    REB_KEEP_VPTR_STORE();
}

void AiTaskRecordSet::Load(istream* in) {
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        AiTaskRecord* record = NewOrNull<AiTaskRecord>();
        if (record) {
            record->m_owner = m_owner;
            record->Read(in);
            AddWithId(record);
        }
    }
}

void AiTaskRecordSet::Save(ostream* out) { SaveIdNodes(this, out); }

uint32_t AiTaskRecordSet::CreateRecord() {
    uint32_t id = 0;
    AiTaskRecord* record = NewOrNull<AiTaskRecord>(m_view, m_owner);
    if (record) id = AddWithId(record);
    return id;
}

uint32_t AiTaskRecordSet::DeleteFirstUnassigned() {
    uint32_t id = 0;
    AiTaskRecord* record = static_cast<AiTaskRecord*>(First());
    if (record) {
        if (!record->IsFinished()) {
            id = record->m_id;
            record->Release();
            DeleteById(id);
        }
    }
    return id;
}

// ---- AiAgentSet -------------------------------------------------------------------------------------------------------

AiAgentSet::AiAgentSet() : IdAvlTree(), m_view(0) {}

AiAgentSet::~AiAgentSet() { REB_KEEP_VPTR_STORE(); }

void AiAgentSet::Load(istream* in) {
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        int kind;
        ReadValue32(in, &kind);
        AiAgentBase* agent = CreateAgent(kind);
        if (agent) {
            agent->m_characters = m_characters;
            agent->m_owner = m_owner;
            agent->Read(in);
            AddWithId(agent);
        }
    }
}

void AiAgentSet::Save(ostream* out) {
    WriteValue32(out, &m_view);
    uint32_t count = CountOf(*this);
    WriteValue32(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) {
        AiAgentBase* agent = static_cast<AiAgentBase*>(node);
        int kind = agent->GetKind();
        WriteValue32(out, &kind);
        agent->Write(out);
    }
}

uint32_t AiAgentSet::AddAgent(int kind) {
    AiAgentBase* agent = CreateAgent(kind);
    if (agent) {
        const uint32_t id = AddWithId(agent);
        agent->m_characters = m_characters;
        agent->m_owner = m_owner;
        return id;
    }
    return kind;
}

void AiAgentSet::ReleaseAgent(uint32_t id) {
    if (id == 0) return;
    AiAgentBase* agent = static_cast<AiAgentBase*>(FindById(id));
    if (!agent) return;
    if (agent->GetAgentClass() == 1) {
        AiAgentTargeted* targeted = static_cast<AiAgentTargeted*>(agent);
        AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(targeted->m_systemRecordId));
        if (record) record->RemoveAgent(targeted);
    }
    agent->m_flags.m_value |= kDirty;
}

AiAgentBase* AiAgentSet::CreateAgent(int kind) {
    switch (kind) {
        case 1: {
            void* memory = ::operator new(0x4c);
            if (memory) return FUN_0047a430(memory, m_view, m_owner, m_characters);
            break;
        }
        case 3: {
            void* memory = ::operator new(0x5c);
            if (memory) return FUN_0047a3c0(memory, m_view, m_owner, m_characters);
            break;
        }
        case 4: {
            void* memory = ::operator new(0x5c);
            if (memory) return FUN_0047a000(memory, m_view, m_owner, m_characters);
            break;
        }
        case 5: {
            void* memory = ::operator new(0x5c);
            if (memory) return FUN_00479f70(memory, m_view, m_owner, m_characters);
            break;
        }
        case 6: {
            void* memory = ::operator new(0x4c);
            if (memory) return FUN_00479a60(memory, m_view, m_owner, m_characters);
            break;
        }
    }
    return nullptr;
}

// ---- AiSystemRecordSet ------------------------------------------------------------------------------------------------

AiSystemRecordSet::AiSystemRecordSet() : IdAvlTree(), m_view(0), m_owner(nullptr) {}

AiSystemRecordSet::~AiSystemRecordSet() {
    m_owner = nullptr;
    REB_KEEP_VPTR_STORE();
}

void AiSystemRecordSet::Load(istream* in) {
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        AiSystemRecord* record = NewOrNull<AiSystemRecord>();
        if (record) {
            record->m_owner = m_owner;
            record->Read(in);
            AddWithId(record);
        }
    }
}

void AiSystemRecordSet::Save(ostream* out) {
    WriteValue32(out, &m_view);
    uint32_t count = CountOf(*this);
    WriteValue32(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) static_cast<AiSystemRecord*>(node)->Write(out);
}

AiSystemRecord* AiSystemRecordSet::FindByTarget(const ObjectKey* key) {
    AiSystemRecord* record = static_cast<AiSystemRecord*>(First());
    bool found = false;
    while (record && !found) {
        if (record->m_systemKey.m_value == key->m_value) {
            found = true;
        } else {
            record = static_cast<AiSystemRecord*>(record->m_successor);
        }
    }
    return record;
}

uint32_t AiSystemRecordSet::CreateRecord() {
    uint32_t id = 0;
    AiSystemRecord* record = NewOrNull<AiSystemRecord>(m_view, m_owner);
    if (record) id = AddWithId(record);
    return id;
}

uint32_t AiSystemRecordSet::DeleteFirstIdle() {
    bool found = false;
    uint32_t id = 0;
    for (AiSystemRecord* record = static_cast<AiSystemRecord*>(First()); record;
         record = static_cast<AiSystemRecord*>(record->m_successor)) {
        if (found) break;
        const bool hasSector = InFamilies(record->m_sectorKey.m_value >> 24, 0x80, 0x90);
        if (!hasSector) {
            id = record->m_id;
        } else {
            AiSectorEval* sector = m_owner->m_sectors.FindEval(record->m_sectorKey);
            if (sector && sector->Stat(0x18c) > 0) {
                id = record->m_id;
                found = true;
            }
        }
        found = found || !hasSector;
    }
    if (found) {
        AiSystemRecord* record = static_cast<AiSystemRecord*>(FindById(id));
        if (record) {
            record->Reset();
            DeleteById(id);
            return id;
        }
    }
    return id;
}

// ---- AiPlannerSet -----------------------------------------------------------------------------------------------------

AiPlannerSet::AiPlannerSet() : IdAvlTree(), m_view(0) {}

AiPlannerSet::~AiPlannerSet() { REB_KEEP_VPTR_STORE(); }

void AiPlannerSet::Load(istream* in) {
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        int kind;
        int subtype;
        ReadValue32(in, &kind);
        ReadValue32(in, &subtype);
        AiMissionPlanner* planner = CreatePlanner(kind, subtype);
        if (planner) {
            planner->m_characters = m_characters;
            planner->m_owner = m_owner;
            planner->Read(in);
            AddWithId(planner);
        }
    }
}


void AiPlannerSet::Save(ostream* out) {
    WriteValue32(out, &m_view);
    uint32_t count = CountOf(*this);
    WriteValue32(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) {
        AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(node);
        uint32_t kind = planner->m_missionKey.m_value >> 24;
        WriteValue32(out, &kind);
        int subtype = planner->GetSubtype();
        WriteValue32(out, &subtype);
        planner->Write(out);
    }
}

uint32_t AiPlannerSet::AddPlanner(int kind, int subtype) {
    AiMissionPlanner* planner = CreatePlanner(kind, subtype);
    if (planner) {
        const uint32_t id = AddWithId(planner);
        planner->m_characters = m_characters;
        planner->m_owner = m_owner;
        m_owner->MarkDirty();
        return id;
    }
    return subtype;
}

void AiPlannerSet::ReleasePlanner(uint32_t id) {
    if (id == 0) return;
    AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(FindById(id));
    if (!planner) return;
    planner->m_flags.m_value |= 0x10000000;
    if (planner->Virtual04() == 1) {
        AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(planner->m_systemRecordId));
        if (record) record->RemovePlanner(planner);
    } else if (planner->Virtual04() == 2) {
        m_owner->UncountPlanner(planner);
    }
}

AiMissionPlanner* AiPlannerSet::FindByTarget(const int* key) {
    AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(First());
    bool found = false;
    while (planner && !found) {
        if (static_cast<int>(planner->m_target.m_value) == *key)
            found = true;
        else
            planner = static_cast<AiMissionPlanner*>(planner->m_successor);
    }
    return planner;
}

void AiPlannerSet::NoteMissionObject(const ObjectKey* key) {
    if (FindByTarget(reinterpret_cast<const int*>(key))) return;
    if (!InFamilies(key->Family(), 0x50, 0x80)) return;
    GameObjNotifier* object = GetObjectInFamilies_40_80(m_view, key);
    if (!object) return;
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(object);
    if ((raw[0xa4] & 4) != 0) return;
    const uint32_t plannerId = *reinterpret_cast<const uint32_t*>(raw + 0x58);
    if (plannerId == 0) return;
    AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(FindById(plannerId));
    if (planner) FUN_0047adf0(planner, key);
}

void AiPlannerSet::NoteMission(const ObjectKey* key) {
    AiMissionPlanner* planner = FindByTarget(reinterpret_cast<const int*>(key));
    if (planner) FUN_0047ae50(planner);
}

void AiPlannerSet::CallSlot12(const int* key, int argument) {
    AiMissionPlanner* planner = FindByTarget(key);
    if (planner) planner->Virtual12(argument);
}

void AiPlannerSet::CallSlot3(const int* key, int argument) {
    AiMissionPlanner* planner = FindByTarget(key);
    if (planner) planner->Virtual03(argument);
}

AiMissionPlanner* AiPlannerSet::CreatePlanner(int kind, int subtype) {
    using Constructor = AiMissionPlanner*(__thiscall*)(void*, int, AiSideController*, AiCharacterRecordSet*);
    auto make = [&](size_t size, Constructor constructor) -> AiMissionPlanner* {
        void* memory = ::operator new(size);
        if (!memory) return nullptr;
        return constructor(memory, m_view, m_owner, m_characters);
    };
    switch (kind) {
        case 0x51:
            return make(0x70, FUN_00481460);
        case 0x52:
            return subtype == 6 ? make(0x74, FUN_00480d10) : make(0x7c, FUN_00480730);
        case 0x53:
            if (subtype == 1) return make(0x70, FUN_0047bba0);
            if (subtype == 2) return make(0x70, FUN_0047c170);
            if (subtype == 3) return make(0x70, FUN_0047c750);
            return nullptr;
        case 0x54:
            return subtype == 5 ? make(0x74, FUN_0047f550) : make(0x7c, FUN_0047f190);
        case 0x55:
            return make(0x70, FUN_0047d9a0);
        case 0x56:
            return make(0x70, FUN_0047fe50);
        case 0x57:
            return make(0x70, FUN_0047cda0);
        case 0x61:
            return make(0x70, FUN_0047e130);
        case 0x62:
            return subtype != 4 ? make(0x70, FUN_00483140) : make(0x80, FUN_00482b30);
        case 0x63:
            return make(0x80, FUN_00482b30);
        case 0x69:
            return make(0x7c, FUN_0047d380);
        case 0x6a:
            return make(0x70, FUN_00482020);
    }
    return nullptr;
}

// ---- AiFleetEvalSet ---------------------------------------------------------------------------------------------------

AiFleetEvalSet::AiFleetEvalSet() : ObjectKeySet(), m_view(0), m_owner(nullptr), m_ships(nullptr) {}

AiFleetEvalSet::~AiFleetEvalSet() {
    m_ships = nullptr;
    m_owner = nullptr;
    REB_KEEP_VPTR_STORE();
}

AiFleetEval* AiFleetEvalSet::FindEval(ObjectKey key) { return static_cast<AiFleetEval*>(Find(&key)); }

void AiFleetEvalSet::Load(istream* in) {
    ObjectKey scratch;
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        AiFleetEval* eval = NewOrNull<AiFleetEval>();
        if (eval) {
            eval->ReadRecord(in, m_ships, m_owner);
            AddEval(eval);
        }
    }
}

void AiFleetEvalSet::Save(ostream* out) {
    ObjectKey scratch;
    WriteValue32(out, &m_view);
    uint32_t count = CountOf(*this);
    WriteValue32(out, &count);
    for (ObjectKeyNode* node = First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) node->Save(out);
}

void AiFleetEvalSet::AddEval(AiFleetEval* eval) {
    ObjectKey key;
    eval->GetKey(&key);
    if (!FindEval(key)) Add(eval);
}

void AiFleetEvalSet::NoteFleet(const ObjectKey* key) {
    if (!InFamilies(key->Family(), 8, 0x10)) return;
    ObjectKey copy = *key;
    AiFleetEval* eval = FindEval(copy);
    if (eval) eval->MarkDirty();
}

void AiFleetEvalSet::NoteFleetCombat(const ObjectKey* key) {
    if (!InFamilies(key->Family(), 8, 0x10)) return;
    ObjectKey copy = *key;
    AiFleetEval* eval = FindEval(copy);
    if (eval) eval->NoteCombat();
}

void AiFleetEvalSet::RemoveShip(ObjectKey* fleetKey, const ObjectKey* shipKey) {
    if (!InFamilies(fleetKey->Family(), 8, 0x10)) return;
    ObjectKey copy = *fleetKey;
    AiFleetEval* eval = FindEval(copy);
    if (!eval) return;
    eval->RemoveShip(const_cast<ObjectKey*>(shipKey));
    if (CountOf(eval->m_shipKeys) == 0) {
        ObjectKey evalKey;
        eval->GetKey(&evalKey);
        DeleteIfEmpty(&evalKey);
    }
}

void AiFleetEvalSet::AddShip(const ObjectKey* fleetKey, const ObjectKey* shipKey) {
    if (!InFamilies(fleetKey->Family(), 8, 0x10)) return;
    ObjectKey copy = *fleetKey;
    AiFleetEval* eval = FindEval(copy);
    if (!eval) {
        GameObjNotifier* fleet = GetObjectInFamilies_08_10(m_view, fleetKey);
        if (!fleet) return;
        eval = NewOrNull<AiFleetEval>(&m_view, fleet, m_ships, m_owner);
        if (!eval) return;
        eval->MarkDirty();
        AddEval(eval);
    }
    eval->AddShip(shipKey);
}

void AiFleetEvalSet::DeleteIfEmpty(ObjectKey* fleetKey) {
    m_owner->MarkFleetSystemChanged(fleetKey);
    RemoveKey(fleetKey);
}

// ---- AiShipRecordSet --------------------------------------------------------------------------------------------------

AiShipRecordSet::AiShipRecordSet()
    : ObjectKeySet(), m_view(0), m_done(0), m_cursor(nullptr), m_fleets(nullptr), m_classes(nullptr), m_owner(nullptr) {}

AiShipRecordSet::~AiShipRecordSet() {
    m_cursor = nullptr;
    m_fleets = nullptr;
    m_classes = nullptr;
    m_owner = nullptr;
    REB_KEEP_VPTR_STORE();
}

void AiShipRecordSet::Load(istream* in) {
    ObjectKey scratch;
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t i = 0; i < count; ++i) {
        AiShipRecord* record = NewOrNull<AiShipRecord>();
        if (record) {
            record->ReadRecord(in, m_fleets, m_classes, m_owner);
            AddRecord(record);
        }
    }
    ReadValue32(in, &m_done);
}

void AiShipRecordSet::Save(ostream* out) {
    ObjectKey scratch;
    WriteValue32(out, &m_view);
    uint32_t count = CountOf(*this);
    WriteValue32(out, &count);
    for (ObjectKeyNode* node = First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) node->Save(out);
    WriteValue32(out, &m_done);
}

int AiShipRecordSet::AddRecord(AiShipRecord* record) {
    int result = 0;
    ObjectKey key;
    record->GetKey(&key);
    if (!Find(&key)) result = static_cast<int>(Add(record));
    return result;
}

int AiShipRecordSet::BuildStep() {
    int finished = 0;
    if (!m_cursor) {
        Clear();
        m_cursor = GetFirstObjectInFamilies_14_1c(m_view);
        return finished;
    }
    ObjectKey parentKey;
    if (m_cursor->m_parent) {
        ObjectKey tmp;
        parentKey = *GetObjectKey(m_cursor->m_parent, &tmp);
    }
    ObjectKey cursorKey;
    GetObjectKey(m_cursor, &cursorKey);
    if (!Find(&cursorKey)) {
        AiShipRecord* record = NewOrNull<AiShipRecord>(m_cursor, m_view, m_fleets, m_classes, m_owner);
        if (record) {
            if (!AddRecord(record)) {
                delete record;
            } else {
                record->ClearFleet();
                ObjectKey shipKey;
                record->GetKey(&shipKey);
                m_fleets->AddShip(&parentKey, &shipKey);
            }
        }
    }
    const FamilyRange range(0x14, 0x1c);
    m_cursor = static_cast<GameObj*>(m_cursor)->FindNextSibling(&range, 1);
    if (!m_cursor) {
        finished = 1;
        m_done = 1;
    }
    return finished;
}

void AiShipRecordSet::SetFleet(ObjectKey* shipKey, const ObjectKey* fleetKey) {
    if (!InFamilies(shipKey->Family(), 0x14, 0x1c)) return;
    AiShipRecord* record = static_cast<AiShipRecord*>(Find(shipKey));
    if (record) record->m_fleetKey = *fleetKey;
}

void AiShipRecordSet::AddOrRefresh(const ObjectKey* key) {
    ObjectKey parentKey;
    AiShipRecord* existing = static_cast<AiShipRecord*>(Find(key));
    if (existing) {
        existing->ClearFleet();
        return;
    }
    if (!m_done) return;
    GameObjNotifier* ship = GetObjectInFamilies_14_1c(m_view, key);
    if (!ship || (static_cast<GameObj*>(ship)->m_stateFlags & 8)) return;
    AiShipRecord* record = NewOrNull<AiShipRecord>(ship, m_view, m_fleets, m_classes, m_owner);
    if (!record) return;
    if (ship->m_parent) {
        ObjectKey tmp;
        parentKey = *GetObjectKey(ship->m_parent, &tmp);
    }
    if (!AddRecord(record)) {
        delete record;
    } else {
        record->ClearFleet();
        ObjectKey shipKey;
        record->GetKey(&shipKey);
        m_fleets->AddShip(&parentKey, &shipKey);
    }
}

void AiShipRecordSet::MoveShip(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to) {
    AiShipRecord* record = static_cast<AiShipRecord*>(Find(key));
    if (record) record->MoveToFleet(to, from);
}

void AiShipRecordSet::RemoveShip(const ObjectKey* key, const ObjectKey* fleet) {
    AiShipRecord* record = static_cast<AiShipRecord*>(Find(key));
    if (record && record->ChangeFleet(fleet)) RemoveKey(key);
}

// ---- AiSectorEvalSet --------------------------------------------------------------------------------------------------

AiSectorEvalSet::AiSectorEvalSet() : ObjectKeySet(), m_view(0), m_cursor(nullptr), m_cursorObject(nullptr) {}

AiSectorEvalSet::~AiSectorEvalSet() {
    m_cursorObject = nullptr;
    m_cursor = nullptr;
    REB_KEEP_VPTR_STORE();
}

AiSectorEval* AiSectorEvalSet::FindEval(ObjectKey key) { return static_cast<AiSectorEval*>(Find(&key)); }

void AiSectorEvalSet::LoadAll(istream* in, AiSystemEvalSet* systems, AiSideController* owner) {
    ObjectKey key;
    ReadValue32(in, &m_view);
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    if (count != 0) {
        do {
            ReadOne(in, systems, owner);
            --count;
        } while (count != 0);
    }
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
        m_cursor = FindEval(key);
    }
}

void AiSectorEvalSet::Save(ostream* out) {
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

int AiSectorEvalSet::BuildStep(AiSystemEvalSet* systems, AiSideController* owner) {
    int finished = 0;
    if (!m_cursorObject) {
        Clear();
        m_cursorObject = GetFirstObjectInFamilies_80_90(m_view);
        return finished;
    }
    m_cursor = NewOrNull<AiSectorEval>(m_view);
    if (m_cursor) {
        m_cursor->Attach(static_cast<Sector*>(m_cursorObject), systems);
        m_cursor->m_owner = owner;
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

void AiSectorEvalSet::NoteSector(const ObjectKey* key) {
    ObjectKey copy = *key;
    AiSectorEval* eval = FindEval(copy);
    if (eval) eval->MarkDirty();
}

void AiSectorEvalSet::ReadOne(istream* in, AiSystemEvalSet* systems, AiSideController* owner) {
    AiSectorEval* eval = NewOrNull<AiSectorEval>();
    if (eval) {
        eval->ReadRecord(in, systems);
        eval->m_owner = owner;
        Add(eval);
    }
}

// ---- AiSystemEvalSet --------------------------------------------------------------------------------------------------

AiSystemEvalSet::AiSystemEvalSet() : ObjectKeySet(), m_view(0), m_cursor(nullptr), m_cursorObject(nullptr), m_classes(nullptr) {}

AiSystemEvalSet::~AiSystemEvalSet() {
    m_cursorObject = nullptr;
    m_cursor = nullptr;
    m_classes = nullptr;
    REB_KEEP_VPTR_STORE();
}

void AiSystemEvalSet::Load(istream* in) {
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
        m_cursor = FindEval(key);
    }
}

void AiSystemEvalSet::Save(ostream* out) {
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

int AiSystemEvalSet::BuildStep() {
    if (!m_cursorObject) {
        Clear();
        m_cursorObject = GetFirstObjectInFamilies_90_98(m_view);
        return 0;
    }
    ObjectKey key;
    GetObjectKey(m_cursorObject, &key);
    m_cursor = FindEval(key);
    if (!m_cursor) {
        m_cursor = NewOrNull<AiSystemEval>(m_view, static_cast<System*>(m_cursorObject));
        if (m_cursor) {
            m_cursor->MarkDirty();
            m_cursor->m_classes = m_classes;
            Add(m_cursor);
        }
    }
    const FamilyRange range(0x90, 0x98);
    m_cursorObject = static_cast<GameObj*>(m_cursorObject)->FindNextSibling(&range, 1);
    if (!m_cursorObject) {
        m_cursor = nullptr;
        return 1;
    }
    return 0;
}

void AiSystemEvalSet::NoteSystem(const ObjectKey* key) {
    ObjectKey copy = *key;
    AiSystemEval* eval = FindEval(copy);
    if (eval) eval->MarkChanged();
}

void AiSystemEvalSet::NoteSystemCombat(const ObjectKey* key) {
    ObjectKey copy = *key;
    AiSystemEval* eval = FindEval(copy);
    if (eval) eval->NoteCombat();
}

void AiSystemEvalSet::MarkAllChanged() {
    for (ObjectKeyNode* node = First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor))
        static_cast<AiSystemEval*>(node)->MarkChanged();
}

void AiSystemEvalSet::ReadOne(istream* in) {
    AiSystemEval* eval = NewOrNull<AiSystemEval>();
    if (eval) {
        eval->Load(in);
        eval->m_classes = m_classes;
        Add(eval);
    }
}
