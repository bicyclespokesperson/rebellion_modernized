#include "galaxy/game_setup_seed.h"

#include <new>

#include "data/probability_table.h"
#include "data/seed_table_entry.h"
#include "data/sd_records.h"
#include "events/command_classes.h"
#include "events/event_post.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_root.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"

// The helpers the setup stages share: rule rows applied to the systems of a sector, the creation of the objects a row
// names, and the setup stage counter.


extern int g_setupStage;  // 0x6b2bb4, the setup stage
extern int g_setupComplete;  // 0x6b90e0

namespace {

constexpr uint32_t kViewSideShift = 6;
constexpr uint32_t kPopulated = 0x1;
constexpr uint32_t kNeverBeenControlled = 0x8;
constexpr int kStageDone = 0x1a;
constexpr FamilyRange kSystemFamilies(0x90, 0x98);
constexpr FamilyRange kSectorFamilies(0x80, 0x90);

int bothTrue(int result, int ok) { return result != 0 && ok != 0 ? 1 : 0; }

uint32_t controlSideOf(GameObjNotifier* object) { return (object->m_flags >> kViewSideShift) & 3; }

int isSideNumber(int value) { return value >= 1 && value <= 3; }

}  // namespace

// ---- seed rules ----------------------------------------------------------------------------------------------

int ApplyLoyaltyRuleToSector(GameObjNotifier* sector, const LoyaltyRule* rule, NotifyContext* ctx) {
    int ok = 1;
    if (rule->m_amount != 0) {
        ChildIterator systems(sector, &kSystemFamilies, 1);
        for (systems.Begin(); systems.Current() != nullptr; systems.Advance()) {
            const uint32_t result = ApplyLoyaltyRuleToSystem(static_cast<System*>(systems.Current()), rule, ctx);
            ok = bothTrue(result != 0, ok);
        }
    }
    return ok;
}

uint32_t ApplyLoyaltyRuleToSystem(System* system, const LoyaltyRule* rule, NotifyContext* ctx) {
    uint32_t result = 1;
    const int sideOk = rule->m_controlSide == 0 || isSideNumber(rule->m_controlSide);
    const int kindOk = isSideNumber(rule->m_unitSide) && rule->m_unitSide != 3;
    const int directionOk = isSideNumber(rule->m_direction);
    if (!(sideOk && kindOk && directionOk && rule->m_amount != 0)) return result;
    IntPair range;
    const_cast<LoyaltyRule*>(rule)->GetFamilyRange(&range);
    const uint32_t systemFamily = system->GetFamily();
    const int inRange = !(systemFamily < range.m_first || range.m_second <= systemFamily);
    if (inRange && (controlSideOf(system) == static_cast<uint32_t>(rule->m_controlSide) || rule->m_controlSide == 0)) {
        int amount = rule->m_amount;
        if (rule->m_direction != 1) amount = -amount;
        const int target = rule->m_threshold;
        if ((system->m_flags & kPopulated) != 0 && (system->m_flags & kNeverBeenControlled) == 0 &&
            system->System::GetLoyaltyFor(rule->m_unitSide) < target) {
            void* memory = ::operator new(sizeof(LoyaltyEvent373));
            LoyaltyEvent373* command = memory ? new (memory) LoyaltyEvent373() : nullptr;
            result = command != nullptr;
            if (result != 0) result = command->m_flag;
            if (result != 0) {
                ObjectKey systemKey;
                system->GetKey(&systemKey);
                command->m_key = ObjectKey(systemKey);
                command->m_delta = amount;
                new (&command->m_context) NotifyContext(*ctx);
                QueueEvent(command);
            } else if (command) delete command;
        }
    }
    return result;
}

GameObjNotifier* GetNextSectorMaster(GameObjNotifier* sector) {
    return static_cast<GameObj*>(sector)->FindNextSibling(&kSectorFamilies, 1);
}

GameObjNotifier* NextSystemMaster(GameObjNotifier* system) {
    return static_cast<GameObj*>(system)->FindNextSibling(&kSystemFamilies, 1);
}

// ---- creating the objects of table rows --------------------------------------------------------------------

int GameRoot::PlaceSystem(ObjectKey* systemKey, int unusedA, int initArg, int level, NotifyContext* ctx) {
    (void)unusedA;
    DatRecord* record = FindDataRecord(AsLookupKey(systemKey));
    GameObjNotifier* sector = nullptr;
    int ok = 0;
    if (record != nullptr) {
        const ObjectKey sectorKey = ObjectKey::Make(family::kSector, static_cast<SystemSD*>(record)->m_sectorId);
        sector = GetMasterInFamilies_80_90(&sectorKey);
        ok = sector != nullptr;
    }
    GameObj* system = nullptr;
    if (sector != nullptr && record != nullptr) {
        system = static_cast<GameObj*>(sector)->CreateChild(systemKey, initArg, ctx, 1, 1);
        ok = bothTrue(system != nullptr, ok);
    }
    if (system != nullptr) {
        const int levelSet = (static_cast<System*>(system))->System::SetDataFlag1(level);
        const int firstOk = bothTrue(levelSet, ok);
        const int announced = static_cast<GameObj*>(system)->Virtual47(reinterpret_cast<intptr_t>(ctx));
        const int secondOk = bothTrue(announced, firstOk);
        const int flagged = (static_cast<System*>(system))->System::SetDataFlag2(1);
        ok = bothTrue(flagged, secondOk);
        ObjectKey recordKey;
        if (system->m_record == nullptr) {
            ObjectKey unset;
            recordKey = ObjectKey(unset);
        } else {
            recordKey = ObjectKey::Make(system->m_record->m_third & 0xff, system->m_record->m_id);
        }
        if (recordKey.m_value == m_key150.m_value) {
            ObjectKey ownKey;
            system->GetKey(&ownKey);
            m_key154 = ObjectKey(ownKey);
        }
    }
    return ok;
}

int GameRoot::SumSeedValues(int factor, ObjectRefList* list, int* total) {
    int ok = 1;
    *total = 0;
    for (AvlNode* node = list->First(); node != nullptr; node = node->m_successor) {
        const ObjectKey key = static_cast<ObjectRefNode*>(node)->m_key;
        DatRecord* record = FindDataRecord(AsLookupKey(&key));
        ok = bothTrue(record != nullptr, ok);
        if (record != nullptr) *total += static_cast<SdRecord*>(record)->GetField4c();
    }
    *total = factor * *total;
    return ok;
}

int GameRoot::SeedFromTableRows(GameObjNotifier* parent, int tableType, int first, int last, NotifyContext* ctx) {
    int ok = 1;
    for (; first <= last; ++first) {
        TableEntry* row = FindTableRecord(tableType, first);
        ok = bothTrue(row != nullptr, ok);
        if (row != nullptr && row->m_field20 == first) {
            ObjectRefList* list = &static_cast<SeedTableEntry*>(row)->m_units;
            ok = bothTrue(CreateChildrenFromList(parent, TableRecordFirstValue(row), list, ctx), ok);
        }
    }
    return ok;
}

int GameRoot::CreateChildrenFromList(GameObjNotifier* parent, int count, ObjectRefList* list, NotifyContext* ctx) {
    int ok = 1;
    GameObjNotifier* target = nullptr;
    if (count > 0) {
        do {
            AvlNode* node = list->First();
            if (node != nullptr) {
                ObjectRefNode* entry = static_cast<ObjectRefNode*>(node);
                target = parent;
                if (reinterpret_cast<NullableKey*>(&entry->m_key)->OrNull() != nullptr) {
                    target = static_cast<GameObj*>(parent)->CreateChild(&entry->m_key, controlSideOf(parent), ctx, 1, 1);
                    ok = bothTrue(target != nullptr, ok);
                }
            }
            for (AvlNode* next = node->m_successor; next != nullptr; next = next->m_successor) {
                if (target != nullptr) {
                    ObjectRefNode* entry = static_cast<ObjectRefNode*>(next);
                    ok = bothTrue(static_cast<GameObj*>(target)->CreateChildren(1, &entry->m_key, controlSideOf(target), ctx, 1, 1), ok);
                }
            }
            --count;
        } while (count != 0);
    }
    return ok;
}

// ---- the setup stage counter ---------------------------------------------------------------------------------

int GetSetupStage() { return g_setupStage; }

void SetSetupStage(int stage) {
    g_setupStage = stage;
    g_setupComplete = stage == kStageDone;
}

void AdvanceSetupStage() { SetSetupStage(GetSetupStage() + 1); }

int AllocateObjectId() {
    GameRoot* root = static_cast<GameRoot*>(GetGameRootObjectCached());
    if (root == nullptr) return 0;
    const int id = root->m_unk6c;
    root->m_unk6c = id + 1;
    return id;
}

int g_setupStage{};
