#include "ai/ai_side_controller.h"

#include "ai/ai_common.h"
#include "core/checked_new.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"

using ai::InFamilies;

namespace {

constexpr uint32_t kDirty = 0x80000000;

// The parent of the object `key` names, or false when there is none (or the object is unknown to the side's view).
bool ParentKeyOf(int view, ObjectKey* key) {
    GameObjNotifier* object = GetObjectInFamilies_01_ff(view, key);
    if (!object || !object->m_parent) return false;
    ObjectKey parentKey;
    *key = *GetObjectKey(object->m_parent, &parentKey);
    return true;
}

}  // namespace

AiSideController::AiSideController()
    : m_side(0), m_flags(0), m_bitsA(0), m_bitB(0x80), m_bitC(0x40), m_cursorFleetKey(), m_cursorCharacterKey() {
    m_systems.m_classes = &m_classes;
    m_ships.m_fleets = &m_fleets;
    m_ships.m_classes = &m_classes;
    m_ships.m_owner = this;
    m_fleets.m_owner = this;
    m_fleets.m_ships = &m_ships;
    m_characters.m_owner = this;
    m_planners.m_characters = &m_characters;
    m_planners.m_owner = this;
    m_agents.m_characters = &m_characters;
    m_agents.m_owner = this;
    m_systemRecords.m_owner = this;
    m_tasks.m_owner = this;
    m_idlePlanners.Clear();
    m_idleAgents.Clear();
    m_cursor = nullptr;
    m_step = 0;
    m_subStep = 0;
    for (int& stat : m_stat) stat = 0;
    m_missionTarget = 0;
    m_missionKind = 0;
    m_missionSubtype = 0;
    for (int& count : m_plannerCount) count = 0;
    m_agentSystem = 0;
    m_agentTarget = 0;
    for (int& count : m_agentCount) count = 0;
    for (int& budget : m_budget) budget = 0;
}

AiSideController::~AiSideController() { m_cursor = nullptr; }

void AiSideController::SetSide(int side) {
    m_side = side;
    m_systems.m_view = side;
    m_sectors.m_view = side;
    m_ships.m_view = side;
    m_fleets.m_view = side;
    m_characters.m_view = side;
    m_planners.m_view = side;
    m_agents.m_view = side;
    m_systemRecords.m_view = side;
    m_tasks.m_view = side;
}

void AiSideController::Load(istream* in) {
    ObjectKey key;
    ReadValue32(in, &m_side);
    ReadValue32(in, &m_step);
    ReadValue32(in, &m_subStep);
    m_flags.Read(in);
    ReadValue32(in, &m_bitB);
    ReadValue32(in, &m_bitC);
    ReadValue32(in, &m_bitsA);
    m_key14.Read(in);
    m_key18.Read(in);
    m_key1c.Read(in);
    m_key20.Read(in);
    m_key24.Read(in);
    m_key28.Read(in);
    m_classes.Clear();
    m_classes.LoadValues(in);
    m_classes.Build(m_side);
    m_systems.Load(in);
    m_sectors.LoadAll(in, &m_systems, this);
    m_ships.Load(in);
    m_fleets.Load(in);
    m_cursorFleetKey.Read(in);
    m_characters.Load(in);
    m_cursorCharacterKey.Read(in);
    m_planners.Load(in);
    m_agents.Load(in);
    m_systemRecords.Load(in);
    m_tasks.Load(in);
    for (int& stat : m_stat) ReadValue32(in, &stat);
    ReadValue32(in, &m_missionTarget);
    ReadValue32(in, &m_missionKind);
    ReadValue32(in, &m_missionSubtype);
    for (int& count : m_plannerCount) ReadValue32(in, &count);
    ReadValue32(in, &m_agentSystem);
    ReadValue32(in, &m_agentTarget);
    for (int& count : m_agentCount) ReadValue32(in, &count);
    for (int& budget : m_budget) ReadValue32(in, &budget);
    int cursorFamily;
    ReadValue32(in, &cursorFamily);
    if (cursorFamily == 0) {
        m_cursor = nullptr;
    } else {
        key.Read(in);
        m_cursor = m_sectors.FindEval(key);
    }
}

void AiSideController::Save(ostream* out) {
    WriteValue32(out, &m_side);
    WriteValue32(out, &m_step);
    WriteValue32(out, &m_subStep);
    m_flags.Write(out);
    WriteValue32(out, &m_bitB);
    WriteValue32(out, &m_bitC);
    WriteValue32(out, &m_bitsA);
    m_key14.Write(out);
    m_key18.Write(out);
    m_key1c.Write(out);
    m_key20.Write(out);
    m_key24.Write(out);
    m_key28.Write(out);
    m_classes.SaveValues(out);
    m_systems.Save(out);
    m_sectors.Save(out);
    m_ships.Save(out);
    m_fleets.Save(out);
    m_cursorFleetKey.Write(out);
    m_characters.Save(out);
    m_cursorCharacterKey.Write(out);
    m_planners.Save(out);
    m_agents.Save(out);
    m_systemRecords.Save(out);
    m_tasks.Save(out);
    for (int& stat : m_stat) WriteValue32(out, &stat);
    WriteValue32(out, &m_missionTarget);
    WriteValue32(out, &m_missionKind);
    WriteValue32(out, &m_missionSubtype);
    for (int& count : m_plannerCount) WriteValue32(out, &count);
    WriteValue32(out, &m_agentSystem);
    WriteValue32(out, &m_agentTarget);
    for (int& count : m_agentCount) WriteValue32(out, &count);
    for (int& budget : m_budget) WriteValue32(out, &budget);
    if (m_cursor == nullptr) {
        int none = 0;
        WriteValue32(out, &none);
    } else {
        ObjectKey key;
        m_cursor->GetKey(&key);
        int family = static_cast<int>(key.m_value >> 24);
        WriteValue32(out, &family);
        key.Write(out);
    }
}

void AiSideController::RaiseBudget(unsigned kind) {
    ++m_budget[kind & 0xffff];
    m_systems.MarkAllChanged();
}

void AiSideController::LowerBudget(unsigned kind) {
    --m_budget[kind & 0xffff];
    m_systems.MarkAllChanged();
}

void AiSideController::MarkDirty() { m_flags.m_value |= kDirty; }

void AiSideController::ResetBudget() {
    m_budget[3] = 1;
    m_budget[4] = 1;
    m_budget[0] = 3;
    m_budget[1] = 3;
    m_budget[2] = 2;
    m_budget[5] = 0;
    m_budget[6] = 0;
    m_budget[7] = 3;
    m_budget[8] = 9;
    m_budget[9] = 0;
    m_budget[10] = 6;
    m_budget[11] = 12;
}

void AiSideController::NoteValueA(const int* value) {
    Stat(0x184) = *value;
    m_flags.m_value |= kDirty;
}

void AiSideController::NoteValueB(int value) {
    Stat(0x188) = value;
    m_flags.m_value |= kDirty;
}

void AiSideController::NoteMask(uint32_t mask) {
    m_flags.m_value |= 2;
    Stat(0x190) = 0;
    Stat(0x194) = 0;
    if (m_side == 1) {
        if (mask & 0x300) Stat(0x190) = 1;
        if (mask & 0x1000) ++Stat(0x190);
        if (mask & 0x2000) ++Stat(0x190);
        if (mask & 1) Stat(0x194) = 1;
        if (mask & 0x10) ++Stat(0x194);
        if (mask & 0x20) ++Stat(0x194);
    } else {
        if (mask & 1) Stat(0x190) = 1;
        if (mask & 0x10) ++Stat(0x190);
        if (mask & 0x20) ++Stat(0x190);
        if (mask & 0x300) Stat(0x194) = 1;
        if (mask & 0x1000) ++Stat(0x194);
        if (mask & 0x2000) ++Stat(0x194);
    }
}

void AiSideController::NoteObject(const ObjectKey* key) {
    ObjectKey current = *key;
    for (;;) {
        const uint32_t family = current.Family();
        if (InFamilies(family, 0x90, 0x98)) {
            m_systems.NoteSystem(&current);
            return;
        }
        if (InFamilies(family, 0x14, 0x1c)) {
            m_ships.AddOrRefresh(&current);
            return;
        }
        if (InFamilies(family, 0x80, 0x90)) {
            m_sectors.NoteSector(&current);
            return;
        }
        if (InFamilies(family, 8, 0x10)) {
            m_fleets.NoteFleet(&current);
            return;
        }
        if (InFamilies(family, 0x30, 0x40)) {
            m_characters.AddOrRefresh(&current);
            return;
        }
        if (InFamilies(family, 0x40, 0x80)) {
            m_planners.NoteMissionObject(&current);
            return;
        }
        if (family == 0xf1 || family == 0xf2 || family == 0xf8 || InFamilies(family, 0x98, 0xa0)) return;
        if (!ParentKeyOf(m_side, &current)) return;
    }
}

void AiSideController::NoteObjectReplaced(const ObjectKey* oldKey) {
    ObjectKey current = *oldKey;
    for (;;) {
        const uint32_t family = current.Family();
        if (InFamilies(family, 0x90, 0x98)) {
            m_systems.NoteSystem(&current);
            return;
        }
        if (InFamilies(family, 0x14, 0x1c)) {
            m_ships.RemoveShip(&current, oldKey);
            return;
        }
        if (InFamilies(family, 8, 0x10)) {
            if (oldKey->m_value != current.m_value) m_fleets.NoteFleet(&current);
            return;
        }
        if (InFamilies(family, 0x30, 0x40)) {
            m_characters.RemoveCharacter(&current);
            return;
        }
        if (InFamilies(family, 0x40, 0x80)) {
            m_planners.NoteMission(&current);
            return;
        }
        if (InFamilies(family, 0x80, 0x90)) {
            m_sectors.NoteSector(&current);
            return;
        }
        if (family == 0xf1 || family == 0xf2 || family == 0xf8 || InFamilies(family, 0x98, 0xa0)) return;
        if (!ParentKeyOf(m_side, &current)) return;
    }
}

void AiSideController::NoteCombat(const ObjectKey* key) {
    ObjectKey current = *key;
    for (;;) {
        const uint32_t family = current.Family();
        if (InFamilies(family, 0x90, 0x98)) {
            m_systems.NoteSystemCombat(&current);
            return;
        }
        if (InFamilies(family, 8, 0x10)) {
            m_fleets.NoteFleetCombat(&current);
            return;
        }
        if (!ParentKeyOf(m_side, &current)) return;
    }
}

void AiSideController::NoteMoved(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to) {
    const uint32_t family = key->Family();
    if (InFamilies(family, 0x90, 0x98)) {
        m_systems.NoteSystem(key);
    } else if (InFamilies(family, 0x14, 0x1c)) {
        m_ships.MoveShip(key, from, to);
    } else if (InFamilies(family, 8, 0x10)) {
        m_fleets.NoteFleet(key);
    } else if (InFamilies(family, 0x30, 0x40)) {
        m_characters.SetRecordLocation(key, from, to);
    } else if (InFamilies(family, 0x40, 0x80)) {
    } else if (InFamilies(family, 0x80, 0x90)) {
        m_sectors.NoteSector(key);
    } else if (InFamilies(family, 0x20, 0x30)) {
        if (InFamilies(family, 0x20, 0x22)) m_flags.m_value |= 2;
    }
    NoteObject(from);
    NoteObject(to);
}

void AiSideController::NoteSystemChanged(const ObjectKey* key) {
    if (InFamilies(key->Family(), 0x90, 0x98)) m_flags.m_value |= 2;
}

void AiSideController::NoteShipChanged(const ObjectKey* key) {
    if (!InFamilies(key->Family(), 0x14, 0x1c)) return;
    AiShipRecord* record = static_cast<AiShipRecord*>(m_ships.Find(key));
    if (record) {
        record->m_flags.m_value |= 0x8000000;
        m_ships.AddOrRefresh(key);
    }
}

void AiSideController::NotePlannerSlot12(const int* key, int argument) { m_planners.CallSlot12(key, argument); }

void AiSideController::NotePlannerSlot3(const int* key, int argument) { m_planners.CallSlot3(key, argument); }

void AiSideController::NoteCharacterChanged(const ObjectKey* key) {
    m_characters.AddOrRefresh(key);
    AiCharacterRecord* record = static_cast<AiCharacterRecord*>(m_characters.Find(key));
    if (!record) return;
    ObjectKey location = record->m_location;
    if (InFamilies(location.Family(), 0x90, 0x98))
        m_systems.NoteSystem(&location);
    else if (InFamilies(location.Family(), 0x14, 0x1c))
        m_ships.AddOrRefresh(&location);
}

void AiSideController::NoteSystemTouched(const ObjectKey* key) {
    if (!InFamilies(key->Family(), 0x90, 0x98)) return;
    AiSystemEval* eval = m_systems.FindEval(*key);
    if (eval) eval->NoteTouched();
}

void AiSideController::NoteFacilityTier(int, int tier) {
    if (tier == m_classes.m_maxFacilityTier) m_flags.m_value |= 0x8000000;
    DatRecord* record = nullptr;
    if (GetTierRecord2a(m_side, tier, &record) != 0) {
        ObjectKey key;
        if (GetAiRecordKey(record, &key)->m_value == 0x25000004) m_flags.m_value |= 0x1000000;
    }
    m_classes.SetFacilityTier(tier, m_side);
}

void AiSideController::NoteTroopTier(int, int tier) {
    if (tier == m_classes.m_maxTroopTier) m_flags.m_value |= 0x4000000;
    m_classes.SetTroopTier(tier, m_side);
}

void AiSideController::NoteCapitalShipTier(int, int tier) {
    if (tier == m_classes.m_maxShipTier) m_flags.m_value |= 0x2000000;
    DatRecord* record = nullptr;
    if (GetTierRecord28(m_side, tier, &record) != 0) {
        ObjectKey key;
        const uint32_t recordKey = GetAiRecordKey(record, &key)->m_value;
        const uint32_t expected = m_side == 1 ? 0x1400004a : 0x14000082;
        if (recordKey == expected) m_flags.m_value |= 0x1000000;
    }
    m_classes.SetCapitalShipTier(tier, m_side);
}

void AiSideController::AdvanceBitCursor() {
    if (m_bitsA == 0) {
        m_bitB = m_bitC;
        m_bitC >>= 1;
        if (m_bitC == 0) m_bitC = 0x80;
        return;
    }
    uint32_t bit = m_bitB;
    while ((m_bitsA & bit) == 0) {
        m_bitB = bit >> 1;
        if ((bit >> 1) == 0) m_bitB = 0x80;
        bit = m_bitB;
    }
    m_bitsA = ~m_bitB & m_bitsA;
}

