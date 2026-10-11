#include "character/character_mgr.h"
#include "core/icf_stubs.h"
#include "ai/ai_common.h"
#include "core/utility_classes.h"
#include "data/game_math.h"
#include "data/random.h"
#include "data/sd_records.h"
#include "economy/facility.h"
#include "economy/manu_mgr.h"
#include "economy/side.h"
#include "galaxy/game_setup_seed.h"
#include "galaxy/sector.h"
#include "galaxy/system_rules.h"
#include "galaxy/system_rules_1e.h"
#include "galaxy/system_type90.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/game_root.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"
#include "galaxy/game_setup_seed.h"
#include "rules/loyalty_params.h"
#include "rules/rules_mgr.h"
#include "rules/event_rules.h"

// The stages of the new-game setup that fill the universe (GameRoot::StepSetup, 0x5136d0, calls them in this order):
// the root's children (side roots, managers, sides, the singleton rules objects), the sectors, the split of the
// systems into pools, the placement of the pooled systems one by one, the initial forces of every system and side,
// and finally the clean-up of the two side views.

// ---- helpers and classes of other modules ------------------------------------------------------------------

int GetSideObjectForIndex(int side, GameObjNotifier** out);
int ApplyEvent315(ObjectKey* key, int kind, int value, NotifyContext* ctx);

namespace {

constexpr size_t kPastAllFacilityOffset = 0x58;

constexpr uint32_t kViewSideShift = 6;
constexpr uint32_t kYavinKey = 0x92000121;
constexpr uint32_t kCoruscantKey = 0x90000109;
constexpr FamilyRange kSectorFamilies(0x80, 0x90);
constexpr FamilyRange kSystemFamilies(0x90, 0x98);
constexpr FamilyRange kSystemFamily90(0x90, 0x92);
constexpr FamilyRange kFacilityFamilies(0x20, 0x30);
constexpr FamilyRange kMineFamilies(0x2c, 0x2d);
constexpr FamilyRange kFleetFamilies(0x08, 0x10);
constexpr FamilyRange kCapitalShipFamilies(0x14, 0x1c);
constexpr FamilyRange kAllFamilies(1, 0xff);
constexpr FamilyRange kStationFamilies(0x28, 0x30);

int bothTrue(int result, int ok) { return result != 0 && ok != 0 ? 1 : 0; }

GameObj* asGameObj(GameObjNotifier* object) { return static_cast<GameObj*>(object); }
System* asSystem(GameObjNotifier* object) { return static_cast<System*>(object); }

uint32_t controlSideOf(GameObjNotifier* object) { return (object->m_flags >> kViewSideShift) & 3; }

ObjectKey keyOfRecord(const DatRecord* record) { return ObjectKey::Make(record->m_third & 0xff, record->m_id); }

GameObj* createChild(GameObj* parent, uint32_t family, uint32_t id, int initArg, NotifyContext* ctx) {
    const ObjectKey key = ObjectKey::Make(family, id);
    return parent->CreateChild(&key, initArg, ctx, 1, 1);
}

GameObjNotifier* masterSystem(uint32_t key) {
    const ObjectKey systemKey = ObjectKey::Make(key >> 24, key);
    return GetMasterInFamilies_90_98(&systemKey);
}

GameObjNotifier* nextSystem(GameObjNotifier* system) { return NextSystemMaster(system); }

int totalFacilityCount(GameObjNotifier* system) {
    int total = 0;
    ChildIterator facilities(system, controlSideOf(system), &kFacilityFamilies, 3);
    for (facilities.Begin(); facilities.Current() != nullptr; facilities.Advance()) total += ReturnOne();
    return total;
}

int totalMineOutput(GameObjNotifier* system) {
    int total = 0;
    ChildIterator mines(system, controlSideOf(system), &kMineFamilies, 3);
    for (mines.Begin(); mines.Current() != nullptr; mines.Advance())
        total += static_cast<Facility*>(mines.Current())->Virtual114();
    return total;
}

// Seeds the headquarters system from its table rows and raises its energy and raw material to what its facilities
// and mines produce.
int seedHeadquarters(GameRoot* root, GameObjNotifier* headquarters, int tableType, int first, int last, NotifyContext* ctx,
                     int ok) {
    ok = bothTrue(root->SeedFromTableRows(headquarters, tableType, first, last, ctx), ok);
    System* system = asSystem(headquarters);
    const int energy = totalFacilityCount(system);
    if (system->m_energy < energy) ok = bothTrue(system->SetEnergy(energy, ctx), ok);
    const int rawMaterial = totalMineOutput(system);
    if (system->m_rawMaterial < rawMaterial) ok = bothTrue(system->SetRawMaterial(rawMaterial, ctx), ok);
    return ok;
}

}  // namespace

// ---- stage 8: the root object itself -----------------------------------------------------------------------

int GameRoot::SetupRootObject(NotifyContext* ctx) {
    const ObjectKey rootKey = ObjectKey::Make(family::kGameRoot, 1);
    DatRecord* record = FindDataRecord(AsLookupKey(&rootKey));
    int ok = record != nullptr;
    if (record != nullptr) {
        ok = bothTrue(SetRecord(record), ok);
        ok = bothTrue(DestroyRandomGenerator(), ok);
        ok = bothTrue(CreateRandomGenerator(), ok);
        ok = bothTrue(GameObj::CreateViews(3, ctx), ok);
        ok = bothTrue(SetCreated(1, ctx), ok);
        ok = bothTrue(SetCompleted(1, ctx), ok);
    }
    return ok;
}

// ---- stage 9: the side roots, managers and sides -------------------------------------------------------------

int GameRoot::CreateRootChildren(NotifyContext* ctx) {
    m_master_f2000003 = createChild(this, family::kSideRoot, 2, 3, ctx);
    m_master_f8000006 = createChild(this, family::kMissionManager, 8, 3, ctx);
    m_master_f9000007 = createChild(this, 0xf9, 9, 3, ctx);
    m_master_fa000008 = createChild(this, family::kCharacterManager, 10, 3, ctx);
    m_master_f300000f = createChild(this, family::kSide, 3, 1, ctx);
    m_master_f3000010 = createChild(this, family::kSide, 3, 2, ctx);
    m_master_98000440 = createChild(this, 0x98, 0x440, 1, ctx);
    m_master_98000441 = createChild(this, 0x98, 0x441, 3, ctx);
    m_master_98000442 = createChild(this, 0x98, 0x442, 3, ctx);
    m_master_98000480 = createChild(this, 0x98, 0x480, 2, ctx);
    m_master_98000481 = createChild(this, 0x98, 0x481, 2, ctx);
    const int allCreated = m_master_f2000003 && m_master_f8000006 && m_master_f9000007 && m_master_fa000008 &&
                           m_master_f300000f && m_master_f3000010 && m_master_98000440 && m_master_98000441 &&
                           m_master_98000442 && m_master_98000480 && m_master_98000481;
    if (allCreated) {
        static_cast<CharacterMgr*>(m_master_fa000008)->m_galaxySize = m_settings.m_field04;
    }
    return allCreated;
}

// ---- stage 0xa: the sectors of the chosen galaxy size --------------------------------------------------------

int GameRoot::CreateSectors(int galaxySize, NotifyContext* ctx) {
    int ok = 1;
    NotifyContext sectorContext(*ctx);
    sectorContext.m_flag04 = 1;
    sectorContext.m_flag08 = 2;
    for (DatRecord* record = GetFirstRecordOfType(0x80); record != nullptr; record = static_cast<DatRecord*>(record->m_successor)) {
        if (static_cast<SectorSD*>(record)->m_galaxySize <= galaxySize) {
            const ObjectKey key = keyOfRecord(record);
            ok = bothTrue(CreateChild(&key, 3, &sectorContext, 1, 1) != nullptr, ok);
        }
    }
    return ok;
}

// ---- stage 0xb: split the systems into the pools the placement stage draws from ---------------------------------

int GameRoot::DistributeSystems(NotifyContext* ctx) {
    (void)ctx;
    uint32_t yavinId = 0;
    uint32_t coruscantId = 0;
    int systemsPlaced = 0;
    int result = CheckObjectDatabase();
    if (result == 0) return 0;

    ObjectKeyValueSet sectorKeys;
    for (GameObjNotifier* sector = GetFirstMasterInFamilies_80_90(); sector != nullptr;
         sector = asGameObj(sector)->FindNextSibling(&kSectorFamilies, 1)) {
        ObjectKey key;
        sector->GetKey(&key);
        sectorKeys.AddKeyValue(&key, 0);
    }

    ObjectRefList core;
    for (DatRecord* record = GetFirstRecordOfType(0x90); record != nullptr; record = static_cast<DatRecord*>(record->m_successor)) {
        const ObjectKey sectorKey = ObjectKey::Make(family::kSector, static_cast<SystemSD*>(record)->m_sectorId);
        GameObjNotifier* sector = nullptr;
        if (sectorKeys.Find(&sectorKey) != nullptr) sector = GetMasterInFamilies_80_90(&sectorKey);
        if (sector == nullptr) continue;
        ++systemsPlaced;
        ObjectRefList& pool = static_cast<Sector*>(sector)->Sector::GetGroup() == 1 ? core : m_refLists[5];
        const ObjectKey systemKey = keyOfRecord(record);
        ObjectRefNode* node = pool.AddKey(systemKey);
        if (systemKey.m_value == kYavinKey) {
            yavinId = node->m_id;
        } else if (systemKey.m_value == kCoruscantKey) {
            coruscantId = node->m_id;
        }
    }

    const int coreCount = core.Size();
    const int percentA = SideSharePercent(1, 0);
    const int percentC = SideSharePercent(2, 0);
    const int percentB = SideSharePercent(1, 1);
    const int percentD = SideSharePercent(2, 1);
    int toPool0x100 = ScalePercent(coreCount, percentA);
    int toPool0x110 = ScalePercent(coreCount, percentB);
    int toPool0xe0 = ScalePercent(coreCount, percentC);
    int toPool0xf0 = ScalePercent(coreCount, percentD);
    int toPool0x120 = coreCount - toPool0x100 - toPool0x110 - toPool0xe0 - toPool0xf0;

    const int farCount = m_refLists[5].Size();
    toPool0xe0 -= 1;
    int remaining = coreCount - 1;

    IdNode* node = core.DetachById(coruscantId);
    node->SetId(0);
    m_refLists[6].Add(static_cast<ObjectRefNode*>(node));

    node = m_refLists[5].DetachById(yavinId);
    node->SetId(0);
    m_refLists[6].Add(static_cast<ObjectRefNode*>(node));

    int pick = GameRandom(farCount - 2);
    AvlNode* cursor = m_refLists[5].First();
    for (; pick != 0; --pick) {
        if (cursor == nullptr) break;
        cursor = cursor->m_successor;
    }
    uint32_t chosenId = farCount;
    if (pick == 0 && cursor != nullptr) {
        chosenId = static_cast<ObjectRefNode*>(cursor)->m_id;
        ObjectKey chosenKey;
        static_cast<ObjectRefNode*>(cursor)->GetKey(&chosenKey);
        m_key150 = ObjectKey(chosenKey);
    }
    node = m_refLists[5].DetachById(chosenId);
    node->SetId(0);
    m_refLists[6].Add(static_cast<ObjectRefNode*>(node));

    while (remaining != 0) {
        --remaining;
        int skip = GameRandom(remaining);
        AvlNode* entry = core.First();
        bool found = true;
        for (; skip != 0; --skip) {
            if (entry == nullptr) {
                found = false;
                break;
            }
            entry = entry->m_successor;
        }
        if (!found || entry == nullptr) continue;
        ObjectRefNode* refNode = static_cast<ObjectRefNode*>(core.DetachById(static_cast<ObjectRefNode*>(entry)->m_id));
        if (toPool0x100 != 0) {
            m_refLists[2].Add(refNode);
            --toPool0x100;
        } else if (toPool0x110 != 0) {
            m_refLists[3].Add(refNode);
            --toPool0x110;
        } else if (toPool0xe0 != 0) {
            m_refLists[0].Add(refNode);
            --toPool0xe0;
        } else if (toPool0xf0 != 0) {
            m_refLists[1].Add(refNode);
            --toPool0xf0;
        } else {
            m_refLists[4].Add(refNode);
            --toPool0x120;
        }
    }

    int pooled = m_refLists[2].Size();
    pooled += m_refLists[3].Size();
    pooled += m_refLists[0].Size();
    pooled += m_refLists[1].Size();
    pooled += m_refLists[4].Size();
    pooled += m_refLists[5].Size();
    pooled += m_refLists[6].Size();
    result = 0;
    if (toPool0x100 == 0 && toPool0x110 == 0 && toPool0xe0 == 0 && toPool0xf0 == 0 && toPool0x120 == 0)
        result = pooled == systemsPlaced ? 1 : 0;
    result = bothTrue(static_cast<Side*>(m_master_f300000f)->Side::SetRdScalePercent(systemsPlaced), result);
    result = bothTrue(static_cast<Side*>(m_master_f3000010)->Side::SetRdScalePercent(systemsPlaced), result);
    return result;
}

// ---- stage 0xc: place the pooled systems, a few per call -----------------------------------------------------

int GameRoot::PlaceNextPooledSystem(int* done, NotifyContext* ctx) {
    int result = 1;
    *done = 0;
    int rounds = 0;
    do {
        if (*done != 0) return result;
        AvlNode* node = nullptr;
        ObjectRefList* pool = nullptr;
        int unitsA = 0;
        int unitsB = 0;
        int unitsC = 0;
        if (m_refLists[2].First() != nullptr) {
            pool = &m_refLists[2];
            unitsA = 1;
            unitsB = 1;
            unitsC = 0;
        } else if (m_refLists[3].First() != nullptr) {
            pool = &m_refLists[3];
            unitsA = 1;
            unitsB = 1;
            unitsC = 1;
        } else if (m_refLists[0].First() != nullptr) {
            pool = &m_refLists[0];
            unitsA = 1;
            unitsB = 2;
            unitsC = 0;
        } else if (m_refLists[1].First() != nullptr) {
            pool = &m_refLists[1];
            unitsA = 1;
            unitsB = 2;
            unitsC = 1;
        } else if (m_refLists[4].First() != nullptr) {
            pool = &m_refLists[4];
            unitsA = 1;
            unitsB = 3;
            unitsC = 0;
        } else if (m_refLists[5].First() != nullptr) {
            pool = &m_refLists[5];
            unitsA = 0;
            unitsB = 3;
            unitsC = 0;
        } else if (m_refLists[6].First() != nullptr) {
            pool = &m_refLists[6];
            const uint32_t firstKey = static_cast<ObjectRefNode*>(m_refLists[6].First())->m_key.m_value;
            unitsC = 0;
            if (firstKey != kYavinKey && firstKey == kCoruscantKey) {
                unitsA = 1;
                unitsB = 2;
            } else {
                unitsA = 0;
                unitsB = 1;
            }
        }
        if (pool != nullptr) {
            node = pool->First();
            ObjectKey key(static_cast<ObjectRefNode*>(node)->m_key);
            result = bothTrue(PlaceSystem(&key, unitsA, unitsB, unitsC, ctx), result);
            pool->DeleteById(static_cast<ObjectRefNode*>(node)->m_id);
        }
        *done = node == nullptr;
        ++rounds;
    } while (rounds < 0x28);
    return result;
}

// ---- stages 0xd-0xf: the rules object and the per-system initialization walk ----------------------------------

int GameRoot::BuildRulesObject(NotifyContext* ctx) { return static_cast<RulesMgr*>(m_master_f9000007)->BuildNeighbourTable(ctx); }

int GameRoot::BeginSystemWalk(NotifyContext* ctx) {
    (void)ctx;
    m_nextSystem = GetFirstMasterInFamilies_90_98();
    return 1;
}

int GameRoot::InitializeNextSystem(int* done, NotifyContext* ctx) {
    int ok = 1;
    *done = m_nextSystem == nullptr;
    if (*done == 0) {
        ok = static_cast<System*>(m_nextSystem)->Virtual115(ctx) != 0;
        m_nextSystem = nextSystem(m_nextSystem);
    }
    return ok;
}

// ---- stage 0x10: the two headquarters systems ----------------------------------------------------------------

int GameRoot::PlaceHeadquarters(NotifyContext* ctx) {
    const int core = PlaceCoreSystem(ctx);
    const int coruscant = PlaceCoruscantSystem(ctx);
    return coruscant != 0 && core != 0 ? 1 : 0;
}

int GameRoot::PlaceCoreSystem(NotifyContext* ctx) {
    GameObjNotifier* headquarters = GetMasterInFamilies_90_98(&m_key154);
    int tableType = 0;
    int first = 0;
    int last = 0;
    int ok = bothTrue(GetCoreSystemRows(&tableType, &first, &last), headquarters != nullptr);
    if (headquarters != nullptr) {
        ok = seedHeadquarters(this, headquarters, tableType, first, last, ctx, ok);
        const ObjectKey managerKey = ObjectKey::Make(0x20, 5);
        m_master_20000005 = GetMasterObject(&managerKey);
    }
    return ok;
}

int GameRoot::PlaceCoruscantSystem(NotifyContext* ctx) {
    GameObjNotifier* headquarters = masterSystem(kCoruscantKey);
    int ok = headquarters != nullptr;
    if (headquarters != nullptr) {
        int tableType = 0;
        int first = 0;
        int last = 0;
        const int tablesOk = bothTrue(GetCoruscantSystemRows(&tableType, &first, &last), ok);
        ok = seedHeadquarters(this, headquarters, tableType, first, last, ctx, tablesOk);
    }
    return ok;
}

// ---- stage 0x12: initial forces ----------------------------------------------------------------------------

int GameRoot::SeedInitialForces(NotifyContext* ctx) {
    int ok = SeedSystemTroops(ctx);
    ok = bothTrue(SeedYavinForces(ctx), ok);
    ok = bothTrue(SeedKey154Forces(ctx), ok);
    ok = bothTrue(SeedCoruscantForces(ctx), ok);
    ok = bothTrue(SeedStartingFleet(ctx), ok);
    ok = bothTrue(SeedCoruscantFleet(ctx), ok);
    ok = bothTrue(SeedAllianceReserves(ctx), ok);
    return SeedEmpireReserves(ctx) != 0 && ok != 0 ? 1 : 0;
}

int GameRoot::SeedSystemTroops(NotifyContext* ctx) {
    int ok = 1;
    for (GameObjNotifier* object = GetFirstMasterInFamilies_90_92(); object != nullptr;
         object = asGameObj(object)->FindNextSibling(&kSystemFamily90, 1)) {
        System* system = asSystem(object);
        if (system->System::GetDataFlagBit0() == 0) continue;
        const uint32_t side = controlSideOf(system);
        const int owned = side == 1 || side == 2;
        ok = bothTrue(owned, ok);
        if (!owned) continue;
        ObjectKey troopKey;
        if (side == 1) {
            troopKey = ObjectKey::Make(family::kTroopRegiment, 2);
        } else if (side == 2) {
            troopKey = ObjectKey::Make(family::kTroopRegiment, 8);
        }
        const int unitSide = side == 1 ? 1 : side == 2 ? 2 : 0;
        const int count = ComputeShortfallUnits(system->System::GetLoyaltyFor(unitSide));
        if (count != 0) ok = bothTrue(system->CreateChildren(count, &troopKey, side, ctx, 1, 1), ok);
    }
    return ok;
}

int GameRoot::SeedYavinForces(NotifyContext* ctx) {
    GameObjNotifier* system = masterSystem(kYavinKey);
    int tableType = 0;
    int first = 0;
    int last = 0;
    int ok = bothTrue(GetYavinForceRows(&tableType, &first, &last), system != nullptr);
    if (system != nullptr) ok = bothTrue(SeedFromTableRows(system, tableType, first, last, ctx), ok);
    return ok;
}

int GameRoot::SeedKey154Forces(NotifyContext* ctx) {
    GameObjNotifier* system = GetMasterInFamilies_90_98(&m_key154);
    int tableType = 0;
    int first = 0;
    int last = 0;
    int ok = GetKey154ForceRows(&tableType, &first, &last);
    if (system != nullptr) {
        const int placed = SeedFromTableRows(system, tableType, first, last, ctx);
        return placed != 0 && ok != 0 ? 1 : 0;
    }
    return ok;
}

int GameRoot::SeedCoruscantForces(NotifyContext* ctx) {
    GameObjNotifier* system = masterSystem(kCoruscantKey);
    int tableType = 0;
    int first = 0;
    int last = 0;
    int ok = bothTrue(GetCoruscantForceRows(&tableType, &first, &last), system != nullptr);
    if (system != nullptr) ok = bothTrue(SeedFromTableRows(system, tableType, first, last, ctx), ok);
    return ok;
}

int GameRoot::SeedStartingFleet(NotifyContext* ctx) {
    int atYavin = 1;
    const int rolled = RollStartingFleetAtYavin(&atYavin);
    GameObjNotifier* system = atYavin == 0 ? GetMasterInFamilies_90_98(&m_key154) : masterSystem(kYavinKey);
    int ok = bothTrue(rolled, system != nullptr);
    GameObj* fleet = nullptr;
    if (system != nullptr) {
        fleet = createChild(asGameObj(system), family::kFleet, 4, controlSideOf(system), ctx);
        ok = bothTrue(fleet != nullptr, ok);
    }
    int tableType = 0;
    int first = 0;
    int last = 0;
    int tablesOk = bothTrue(GetStartingFleetRows(&tableType, &first, &last), ok);
    if (fleet != nullptr) tablesOk = bothTrue(SeedFromTableRows(fleet, tableType, first, last, ctx), tablesOk);
    return tablesOk;
}

int GameRoot::SeedCoruscantFleet(NotifyContext* ctx) {
    GameObjNotifier* system = masterSystem(kCoruscantKey);
    GameObj* fleet = nullptr;
    int ok = system != nullptr;
    if (system != nullptr) {
        fleet = createChild(asGameObj(system), family::kFleet, 4, controlSideOf(system), ctx);
        ok = bothTrue(fleet != nullptr, ok);
    }
    int tableType = 0;
    int first = 0;
    int last = 0;
    int result = bothTrue(GetCoruscantFleetRows(&tableType, &first, &last), ok);
    if (system != nullptr) result = bothTrue(SeedFromTableRows(fleet, tableType, first, last, ctx), result);
    return result;
}

int GameRoot::SeedAllianceReserves(NotifyContext* ctx) { return SeedSideReserves(1, ctx) != 0; }

int GameRoot::SeedEmpireReserves(NotifyContext* ctx) { return SeedSideReserves(2, ctx) != 0; }

// Deals the side's reserve units out over its systems, round after round, until its budget is spent: each round draws a
// unit group from the rules, picks a random system of the side and creates the units there (capital ships go into a
// fleet at the system).
int GameRoot::SeedSideReserves(int side, NotifyContext* ctx) {
    const uint32_t viewSide = side == 1 ? 1 : side == 2 ? 2 : 0;
    GameObjNotifier* sideObject = GetGameRootSideMemberThunk(side);
    int budget = 0;
    int result = sideObject != nullptr;
    if (sideObject != nullptr) {
        const Side* sideState = static_cast<Side*>(sideObject);
        const int available = sideState->m_maintState.m_first - sideState->m_maintState.m_second;
        result = side == 2 ? ScaleEmpireReserveBudget(available, m_settings.m_field04, &budget)
                           : ScaleAllianceReserveBudget(available, m_settings.m_field04, &budget);
    }
    for (;;) {
        int count = 0;
        ObjectRefList units;
        int ok;
        if (side == 2) {
            ok = bothTrue(PickEmpireReserveUnits(&count, &units), result);
        } else {
            ok = bothTrue(PickAllianceReserveUnits(&count, &units), result);
        }
        int unitsTotal = 0;
        result = bothTrue(SumSeedValues(count, &units, &unitsTotal), ok);
        const int previous = result;
        budget -= unitsTotal;
        const int affordable = budget >= 0;
        if (affordable) {
            int systemsOfSide = 0;
            for (GameObjNotifier* system = GetFirstMasterInFamilies_90_98(); system != nullptr; system = nextSystem(system))
                systemsOfSide += controlSideOf(system) == viewSide;
            const int skip = GameRandom(systemsOfSide - 1);
            GameObjNotifier* target = GetFirstMasterInFamilies_90_98();
            while (target != nullptr && controlSideOf(target) != viewSide) target = nextSystem(target);
            int step = 0;
            if (target == nullptr) {
                result = 0;
            } else {
                do {
                    if (step >= skip) break;
                    target = nextSystem(target);
                    while (target != nullptr && controlSideOf(target) != viewSide) target = nextSystem(target);
                    ++step;
                } while (target != nullptr);
                result = target != nullptr && previous != 0 ? 1 : 0;
            }
            if (target != nullptr) {
                const int beforePlacement = result;
                ObjectRefNode* first = static_cast<ObjectRefNode*>(units.First());
                bool placed = false;
                if (first != nullptr && first->m_key.Family() >= 0x14 && first->m_key.Family() < 0x1c) {
                    GameObjNotifier* fleet = nullptr;
                    ChildIterator fleets(target, viewSide, &kFleetFamilies, 4);
                    for (fleets.Begin(); fleets.Current() != nullptr; fleets.Advance()) {
                        if (fleet != nullptr) break;
                        ChildIterator ships(fleets.Current(), &kCapitalShipFamilies, 1);
                        if (ships.HasCurrent()) fleet = fleets.Current();
                    }
                    if (fleet == nullptr) {
                        fleet = createChild(asGameObj(target), family::kFleet, 4, viewSide, ctx);
                        result = bothTrue(fleet != nullptr, result);
                    }
                    if (fleet != nullptr) {
                        result = bothTrue(CreateChildrenFromList(fleet, count, &units, ctx), result);
                    }
                    placed = true;
                }
                if (!placed) result = bothTrue(CreateChildrenFromList(target, count, &units, ctx), beforePlacement);
            }
        }
        if (!affordable) return result;
    }
}

// ---- stages 0x11 / 0x13 / 0x14 / 0x15 / 0x17 / 0x18 ------------------------------------------------------------

int GameRoot::SetupSideObjects(NotifyContext* ctx) {
    GameObjNotifier* side = nullptr;
    int ok = GetSideObjectForIndex(1, &side) != 0;
    if (side != nullptr) ok = bothTrue(static_cast<Side*>(side)->Side::RecomputeMaintenance(ctx), ok);
    ok = bothTrue(GetSideObjectForIndex(2, &side), ok);
    if (side != nullptr) {
        if (static_cast<Side*>(side)->Side::RecomputeMaintenance(ctx) == 0 || ok == 0) return 0;
        return 1;
    }
    return ok;
}

int GameRoot::SetupCharacterManagerA(NotifyContext* ctx) {
    CharacterMgr* manager = GetCharacterManager();
    int ok = manager != nullptr;
    if (manager != nullptr) {
        if (manager->CreateRecordCharacters(ctx) == 0 || !ok) return 0;
        return 1;
    }
    return ok;
}

int GameRoot::SetupCharacterManagerB(NotifyContext* ctx) {
    CharacterMgr* manager = GetCharacterManager();
    int ok = manager != nullptr;
    if (manager != nullptr) {
        if (manager->PlaceStartingCharacters(ctx) == 0 || !ok) return 0;
        return 1;
    }
    return ok;
}

int GameRoot::InitializeAllObjects(NotifyContext* ctx) {
    int ok = 1;
    for (GameObjNotifier* object = GetFirstMasterInFamilies_01_ff(); object != nullptr;
         object = asGameObj(object)->FindNextSibling(&kAllFamilies, 1)) {
        ok = bothTrue(NumberUnitName(static_cast<GameObj*>(object), ctx), ok);
        ok = bothTrue(static_cast<GameObj*>(object)->Virtual48(reinterpret_cast<intptr_t>(ctx)), ok);
        const uint32_t objectFamily = object->GetFamily();
        if (objectFamily >= kStationFamilies.m_first && objectFamily < kStationFamilies.m_second) {
            // BUG(original): AllFacility is 0x58 bytes, so this reads the word after the object (heap contents); the offset is the original's, not sizeof(AllFacility).
            const int level = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(object) + kPastAllFacilityOffset);
            ObjectKey key;
            object->GetKey(&key);
            ok = bothTrue(ApplyEvent315(&key, 4, level, ctx), ok);
        }
    }
    return ok;
}

int GameRoot::FinishSideViews(NotifyContext* ctx) {
    GameRoot* allianceView = static_cast<GameRoot*>(GetViewObject(1));
    const int first = allianceView->FinishSideView(ctx);
    GameRoot* empireView = static_cast<GameRoot*>(GetViewObject(2));
    const int second = empireView->FinishSideView(ctx);
    return second != 0 && first != 0 ? 1 : 0;
}

int GameRoot::FinishSideView(NotifyContext* ctx) {
    NotifyContext sideContext(*ctx);
    sideContext.m_flag04 = 2;
    sideContext.m_flag08 = 2;
    NotifyContext updateContext(*ctx);
    updateContext.m_flag04 = 1;
    updateContext.m_flag08 = 2;
    int ok = UpdateAllChildren(this, &updateContext) != 0;
    ChildIterator sectors(this, &kSectorFamilies, 4);
    for (sectors.Begin(); sectors.Current() != nullptr; sectors.Advance()) {
        ok = bothTrue(UpdateSystemFamilyChildren(static_cast<GameObj*>(sectors.Current()), &sideContext), ok);
        ChildIterator systems(sectors.Current(), &kSystemFamilies, 4);
        for (systems.Begin(); systems.Current() != nullptr; systems.Advance())
            ok = bothTrue(UpdateManuMgrChildren(static_cast<GameObj*>(systems.Current()), &sideContext), ok);
        ok = bothTrue(UpdateSystemType90Children(static_cast<GameObj*>(sectors.Current()), &updateContext), ok);
        ChildIterator firstSystems(sectors.Current(), &kSystemFamily90, 4);
        for (firstSystems.Begin(); firstSystems.Current() != nullptr; firstSystems.Advance())
            ok = bothTrue(UpdateFacilityChildren(static_cast<GameObj*>(firstSystems.Current()), &updateContext), ok);
    }
    GameObjNotifier* yavin = masterSystem(kYavinKey);
    int result = bothTrue(yavin != nullptr, ok);
    if (yavin != nullptr) result = bothTrue(static_cast<System*>(yavin)->System::SyncViewForEvent(10, 2, ctx), result);
    return result;
}

int g_gnprtb1E00{};
int g_gnprtb1E01{};
int g_gnprtb1E02{};
int g_gnprtb1E03{};
int g_gnprtb1E04{};
int g_gnprtb1E05{};
int g_gnprtb1E06{};
int g_gnprtb1E07{};
int g_gnprtb1E08{};
int g_gnprtb1E09{};
int g_gnprtb1E0A{};
int g_gnprtb1E0B{};
int g_gnprtb1E0C{};
int g_gnprtb1E0D{};
int g_gnprtb1E0F{};
int g_gnprtb1E11{};
int g_gnprtb1E12{};
int g_gnprtb1E13{};
int g_gnprtb1E14{};
int g_gnprtb1E15{};
int g_gnprtb1E16{};
int g_gnprtb1E17{};
int g_gnprtb1E18{};
int g_gnprtb1E19{};
int g_gnprtb1E1A{};
int g_gnprtb1E1B{};
int g_gnprtb1E1C{};
int g_gnprtb1E1D{};
int g_gnprtb1E1E{};
int g_gnprtb1E1F{};
int g_gnprtb1E20{};
int g_gnprtb1E21{};
int g_gnprtb1E22{};
int g_gnprtb1E23{};
int g_gnprtb1E24{};
int g_gnprtb1E25{};
int g_gnprtb1E26{};
int g_gnprtb1E27{};
int g_gnprtb1E28{};
int g_gnprtb1E29{};
int g_gnprtb1E2A{};
int g_gnprtb1E2B{};
int g_gnprtb1E2C{};
int g_gnprtb1E2D{};
int g_gnprtb1E2E{};
int g_gnprtb1E2F{};
int g_gnprtb1E30{};
int g_gnprtb1E31{};
int g_gnprtb1E32{};
int g_gnprtb1E33{};
int g_gnprtb1E34{};
int g_gnprtb1E35{};
int g_gnprtb1E36{};
int g_gnprtb1E37{};
int g_gnprtb1E38{};
int g_gnprtb1E39{};
int g_gnprtb1E50{};
int g_gnprtb1E51{};
int g_gnprtb1E52{};
int g_gnprtb1E53{};
int g_gnprtb1E54{};
int g_gnprtb1E55{};
int g_gnprtb1E56{};
int g_gnprtb1E57{};
int g_sdprtb1E00Side1{};
int g_sdprtb1E00Side2{};
int g_sdprtb1E01Side1{};
int g_sdprtb1E01Side2{};
int g_sdprtb1E02Side1{};
int g_sdprtb1E02Side2{};
int g_sdprtb1E03Side1{};
int g_sdprtb1E03Side2{};
int g_sdprtb1E04Side1{};
int g_sdprtb1E04Side2{};
int g_sdprtb1E05Side1{};
int g_sdprtb1E05Side2{};
int g_systemBlockadeStatLevelFleetSide{};
int g_systemBlockadeStatLevelOther{};
