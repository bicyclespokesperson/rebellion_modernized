#include "ai/ai_records.h"

#include "ai/ai_common.h"
#include "ai/ai_members.h"
#include "ai/ai_side_controller.h"
#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "events/game_clock_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "strategy_ui/advisor_notices.h"

using ai::InFamilies;

namespace {

ObjectKey KeyOfObject(GameObjNotifier* object) {
    ObjectKey key;
    GetObjectKey(object, &key);
    return key;
}

}  // namespace

AiFleetEval::AiFleetEval()
    : ObjectKeyNode(), m_x2c(), m_systemKey(), m_flags(0), m_shipKeys(), m_stat() {
    m_dirty = 0;
    m_view = 0;
    m_cursor = nullptr;
    m_ships = nullptr;
    m_owner = nullptr;
    m_control = 0;
    m_combatDay = 0;
    m_x30 = 0;
    m_x34 = 0;
}

AiFleetEval::AiFleetEval(const int* view, GameObjNotifier* fleet, AiShipRecordSet* ships, AiSideController* owner)
    : ObjectKeyNode(KeyOfObject(fleet)), m_systemKey(), m_x2c(), m_flags(0), m_shipKeys() {
    m_view = *view;
    m_cursor = nullptr;
    m_control = fleet->ControlSide();
    m_combatDay = 0;
    m_x30 = 0;
    m_x34 = 0;
    m_ships = ships;
    m_owner = owner;
    m_dirty = 1;
    m_flags.m_value |= 4;
    for (int& value : m_stat) value = 0;
}

AiFleetEval::~AiFleetEval() {
    m_cursor = nullptr;
    m_ships = nullptr;
    m_owner = nullptr;

}

void AiFleetEval::ReadRecord(istream* in, AiShipRecordSet* ships, AiSideController* owner) {
    ObjectKey cursorKey;
    m_owner = owner;
    m_ships = ships;
    ObjectKeyNode::Load(in);
    ReadValue32(in, &m_view);
    ReadValue32(in, &m_control);
    ReadValue32(in, &m_combatDay);
    ReadValue32(in, &m_dirty);
    ReadValue32(in, &m_x30);
    ReadValue32(in, &m_x34);
    m_systemKey.Read(in);
    m_x2c.Read(in);
    m_flags.Read(in);
    m_shipKeys.Clear();
    uint32_t count = 0;
    ReadValue32(in, &count);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) return;
    cursorKey.Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) return;
    for (uint32_t i = 0; i < count; ++i) {
        ObjectKeyNode* node = NewOrNull<ObjectKeyNode>();
        if (!node) {
            iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
            return;
        }
        node->Load(in);
        if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || !m_shipKeys.Add(node)) {
            delete node;
            iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
            return;
        }
    }
    m_cursor = m_shipKeys.Find(&cursorKey);
    for (int& value : m_stat) ReadValue32(in, &value);
    m_dirty = 1;
}

void AiFleetEval::Save(ostream* out) {
    ObjectKey cursorKey;
    ObjectKeyNode::Save(out);
    WriteValue32(out, &m_view);
    WriteValue32(out, &m_control);
    WriteValue32(out, &m_combatDay);
    WriteValue32(out, &m_dirty);
    WriteValue32(out, &m_x30);
    WriteValue32(out, &m_x34);
    m_systemKey.Write(out);
    m_x2c.Write(out);
    m_flags.Write(out);
    if (!WriteCount32(out, m_shipKeys.Count())) return;
    if (m_cursor) m_cursor->GetKey(&cursorKey);
    cursorKey.Write(out);
    for (ObjectKeyNode* node = m_shipKeys.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) node->Save(out);
    for (int& value : m_stat) WriteValue32(out, &value);
}

void AiFleetEval::RemoveShip(ObjectKey* key) {
    if (!InFamilies(key->Family(), 0x14, 0x1c)) return;
    ObjectKeyNode* node = m_shipKeys.Detach(key);
    if (node) {
        delete node;
        MarkDirty();
    }
}

void AiFleetEval::AddShip(const ObjectKey* key) {
    if (!InFamilies(key->Family(), 0x14, 0x1c)) return;
    if (!m_shipKeys.Find(key)) {
        ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(*key);
        m_shipKeys.Add(node);
        MarkDirty();
    }
    ObjectKey shipKey = *key;
    m_ships->SetFleet(&shipKey, &m_key);
}

int AiFleetEval::HasShip(const ObjectKey* key) {
    int found = 0;
    ObjectKeyNode* node = m_shipKeys.First();
    while (node && !found) {
        ObjectKey nodeKey;
        node->GetKey(&nodeKey);
        if (key->m_value == nodeKey.m_value) found = 1;
        node = static_cast<ObjectKeyNode*>(node->m_successor);
    }
    return found;
}

AiRankedList* AiFleetEval::RankShips(uint32_t requireFlags, uint32_t forbidFlags, uint32_t statIndex, int descending) {
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (ObjectKeyNode* node = m_shipKeys.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey key;
        node->GetKey(&key);
        AiShipRecord* record = static_cast<AiShipRecord*>(m_ships->Find(&key));
        if (record && (requireFlags & record->m_flags.m_value) == requireFlags && (forbidFlags & record->m_flags.m_value) == 0) {
            ObjectKey recordKey;
            record->GetKey(&recordKey);
            AiRankedItem* item = NewOrNull<AiRankedItem>(recordKey, record->m_stat[statIndex & 0xffff]);
            list->AddItem(item);
        }
    }
    list->Number();
    return list;
}

void AiFleetEval::NoteCombat() {
    m_combatDay = GameHookDay();
    MarkDirty();
}

void AiFleetEval::MarkDirty() {
    m_dirty = 1;
    m_owner->MarkDirty();
}

int AiFleetEval::Refresh() {
    int refreshed = 0;
    if (m_dirty == 0) return refreshed;
    m_dirty = 0;
    m_flags.m_value &= 0xf000000c;
    for (int& value : m_stat) value = 0;
    GameObj* fleet = static_cast<GameObj*>(GetObjectInFamilies_08_10(m_view, &m_key));
    if (fleet && (fleet->m_stateFlags & 8) == 0) {
        const int expectedControl = m_view == 1 ? 1 : (m_view == 2 ? 2 : 0);
        if (m_control == expectedControl) m_flags.m_value |= 1;
        if (fleet->m_stateFlags & 0x10) m_flags.m_value |= 2;
        ObjectKey parentKey;
        m_systemKey = *GetObjectKey(fleet->m_parent, &parentKey);
        m_cursor = m_shipKeys.First();
        while (m_cursor) {
            ObjectKey key;
            m_cursor->GetKey(&key);
            AiShipRecord* record = static_cast<AiShipRecord*>(m_ships->Find(&key));
            if (record) {
                record->Refresh();
                AddShipStats(record->m_stat, &record->m_flags.m_value);
            }
            m_cursor = static_cast<ObjectKeyNode*>(m_cursor->m_successor);
        }
        if (0 < Stat(0xa8)) m_flags.m_value |= 0x80000;
        if (0 < Stat(0xac)) m_flags.m_value |= 0x100000;
    }
    refreshed = 1;
    return refreshed;
}

void AiFleetEval::AddShipStats(const int* stats, const int* flags) {
    ++Stat(0x50);
    const uint32_t f = *flags;
    if (f & 0x100) m_flags.m_value |= 0x20000;
    if (f & 0x200) m_flags.m_value |= 0x40000;
    if (f & 0x400000) m_flags.m_value |= 0x4000000;
    if (f & 0x20000000) m_flags.m_value |= 0x8000000;
    if ((f & 0x800) == 0) {
        if ((f & 0x2000) == 0) m_flags.m_value |= 0x4000;
    } else {
        m_flags.m_value |= 0x800;
    }
    if (f & 0x10000) m_flags.m_value |= 0x200;
    if (f & 0x2000) m_flags.m_value |= 0x400;
    if (f & 0x8000) m_flags.m_value |= 0x100;
    if (f & 0x80) m_flags.m_value |= 0x10;
    if (f & 0x400) m_flags.m_value |= 0x20;
    if ((f & 0x801000) == 0) {
        m_flags.m_value |= 0x8000;
    } else {
        if (f & 0x800000) m_flags.m_value |= 0x2000;
        if (f & 0x1000) m_flags.m_value |= 0x1000;
    }
    if ((f & 0x801000) == 0) {
        if (f & 1) ++Stat(0x98);
        if (f & 2) ++Stat(0x9c);
        if (f & 4) ++Stat(0xa0);
    }
    if (f & 0x80000) m_flags.m_value |= 0x1000000;
    if (f & 0x40000) {
        m_flags.m_value |= 0x200000;
        Stat(0xb0) += stats[0x12];
    }
    if (f & 0x100000) m_flags.m_value |= 0x400000;
    if (f & 0x4000000) m_flags.m_value |= 0x2000000;
    if (f & 0x200000) m_flags.m_value |= 0x800000;
    if ((f & 0x8000000) == 0) ++Stat(0x94);
    if ((f & 0x2000) == 0) {
        Stat(0x58) += stats[0xe];
        Stat(0x5c) += stats[0xf];
        Stat(0x60) += stats[3];
        Stat(0x68) += stats[1];
        Stat(0x6c) += stats[2];
        Stat(0x54) += stats[0];
        Stat(0x64) += stats[0x10];
        Stat(0x70) += stats[5];
        Stat(0x78) += stats[7];
        Stat(0x74) += stats[6];
        Stat(0x84) += stats[10];
        Stat(0x7c) += stats[8];
        Stat(0x80) += stats[9];
        Stat(0xa4) += stats[0x11];
        Stat(0xb4) += stats[0x13];
        Stat(0xb8) += stats[0x14];
    }
    Stat(0x88) += stats[0xb];
    Stat(0x8c) += stats[0xc];
    Stat(0x90) += stats[0xd];
    if (f & 4) Stat(0xbc) += stats[10];
    if ((f & 0x2800) == 0) {
        Stat(0xa8) += stats[10];
        Stat(0xa8) -= stats[8];
        Stat(0xac) += stats[7];
        Stat(0xac) -= stats[5];
    }
}

UiNotice* g_uiHandlers[46]{};
int (*g_gameHook2ADC)(HookArg) = nullptr;
int (*g_gameHook2AE4)(HookArg) = nullptr;
int g_gnprtb1200{};
int g_gnprtb1201{};
