#include <new>

#include "ai/ai_agent_bases.h"
#include "ai/ai_common.h"
#include "ai/ai_planner.h"
#include "data/random.h"
#include "ai/ai_side_controller.h"
#include "core/checked_new.h"
#include "economy/side.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"

using ai::InFamilies;

namespace {

constexpr uint32_t kSectorFirst = 0x80;
constexpr uint32_t kSectorLast = 0x90;
constexpr uint32_t kSystemFirst = 0x90;
constexpr uint32_t kSystemLast = 0x98;

struct SectorMasks {
    uint32_t requireA, requireB, requireC, forbidA, forbidB, forbidC;

    bool Matches(const AiSectorEval* sector) const {
        const uint32_t a = sector->m_flags24.m_value;
        const uint32_t b = sector->m_flags28.m_value;
        const uint32_t c = sector->m_flags2c.m_value;
        return (requireA & a) == requireA && (forbidA & a) == 0 && (requireB & b) == requireB && (forbidB & b) == 0 &&
               (requireC & c) == requireC && (forbidC & c) == 0;
    }
};

struct FleetMasks {
    uint32_t require, forbid;

    bool Matches(const AiFleetEval* fleet) const {
        const uint32_t flags = fleet->m_flags.m_value;
        return (require & flags) == require && (forbid & flags) == 0;
    }
};

bool ParentKeyOf(int view, ObjectKey* key) {
    GameObjNotifier* object = GetObjectInFamilies_01_ff(view, key);
    if (!object || !object->m_parent) return false;
    ObjectKey parentKey;
    *key = *GetObjectKey(object->m_parent, &parentKey);
    return true;
}

// Climbs from the object `target` names to its ancestor of a family in [first, last); false if the chain ends elsewhere.
bool ResolveToFamily(int view, const ObjectKey* target, uint32_t first, uint32_t last, ObjectKey* resolved) {
    ObjectKey key = *target;
    while (!InFamilies(key.Family(), first, last)) {
        if (!ParentKeyOf(view, &key)) break;
    }
    *resolved = key;
    return InFamilies(key.Family(), first, last);
}

int16_t PackedX(int position) { return static_cast<int16_t>(position); }
int16_t PackedY(int position) { return static_cast<int16_t>(static_cast<uint32_t>(position) >> 16); }

void AddItem(AiRankedList* list, const ObjectKey& key, int score) { list->AddItem(NewOrNull<AiRankedItem>(key, score)); }

template <class Record>
ObjectKey KeyOf(Record* record) {
    ObjectKey key;
    record->GetKey(&key);
    return key;
}

template <class Record>
Record* Next(Record* record) {
    return static_cast<Record*>(record->m_successor);
}

}  // namespace

AiRankedList* AiSideController::RankSectors(uint32_t requireA, uint32_t requireB, uint32_t requireC, uint32_t forbidA,
                                            uint32_t forbidB, uint32_t forbidC, uint32_t statIndex, int descending) {
    const SectorMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSectorEval* sector = static_cast<AiSectorEval*>(m_sectors.First()); sector; sector = Next(sector)) {
        if (masks.Matches(sector)) AddItem(list, KeyOf(sector), sector->m_stat[statIndex & 0xffff]);
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankSectorsByDistance(const ObjectKey* target, uint32_t requireA, uint32_t requireB,
                                                      uint32_t requireC, uint32_t forbidA, uint32_t forbidB,
                                                      uint32_t forbidC, int descending) {
    const SectorMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = nullptr;
    ObjectKey sectorKey;
    if (!ResolveToFamily(m_side, target, kSectorFirst, kSectorLast, &sectorKey)) return list;
    AiSectorEval* origin = m_sectors.FindEval(sectorKey);
    if (!origin) return list;
    const int16_t originX = origin->m_x;
    const int16_t originY = origin->m_y;
    list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSectorEval* sector = static_cast<AiSectorEval*>(m_sectors.First()); sector; sector = Next(sector)) {
        if (!masks.Matches(sector)) continue;
        AddItem(list, KeyOf(sector), DistanceSquared(originX, originY, sector->m_x, sector->m_y));
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankSectorsByX3c(uint32_t requireA, uint32_t requireB, uint32_t requireC, uint32_t forbidA,
                                                 uint32_t forbidB, uint32_t forbidC, int descending) {
    const SectorMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSectorEval* sector = static_cast<AiSectorEval*>(m_sectors.First()); sector; sector = Next(sector)) {
        if (!masks.Matches(sector)) continue;
        AddItem(list, KeyOf(sector), sector->m_x3c == 0 ? 100 : sector->m_x3c);
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankSectorsByAge(uint32_t requireA, uint32_t requireB, uint32_t requireC, uint32_t forbidA,
                                                 uint32_t forbidB, uint32_t forbidC, int descending) {
    const SectorMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSectorEval* sector = static_cast<AiSectorEval*>(m_sectors.First()); sector; sector = Next(sector)) {
        if (!masks.Matches(sector)) continue;
        ObjectKey key = KeyOf(sector);
        AddItem(list, key, sector->ScoreByAge());
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankSectorsRandomly(uint32_t requireA, uint32_t requireB, uint32_t requireC,
                                                    uint32_t forbidA, uint32_t forbidB, uint32_t forbidC, int descending) {
    const SectorMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSectorEval* sector = static_cast<AiSectorEval*>(m_sectors.First()); sector; sector = Next(sector)) {
        if (!masks.Matches(sector)) continue;
        ObjectKey key = KeyOf(sector);
        AddItem(list, key, GameplayRandomNext(0x14));
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankSystemsOfSector(const ObjectKey* sector, uint32_t requireA, uint32_t requireB,
                                                    uint32_t requireC, uint32_t forbidA, uint32_t forbidB, uint32_t forbidC,
                                                    uint32_t statIndex, int descending) {
    AiSectorEval* eval = m_sectors.FindEval(*sector);
    if (!eval) return nullptr;
    return eval->RankSystemsByStat(requireA, requireB, requireC, forbidA, forbidB, forbidC, statIndex, descending);
}

AiRankedList* AiSideController::RankSystemsOfSectorByAge(const ObjectKey* sector, uint32_t requireA, uint32_t requireB,
                                                         uint32_t requireC, uint32_t forbidA, uint32_t forbidB,
                                                         uint32_t forbidC, int descending) {
    AiSectorEval* eval = m_sectors.FindEval(*sector);
    if (!eval) return nullptr;
    return eval->RankSystemsByAge(requireA, requireB, requireC, forbidA, forbidB, forbidC, descending);
}

AiRankedList* AiSideController::RankSystemsOfSectorByDistance(const ObjectKey* sector, const ObjectKey* target,
                                                              uint32_t requireA, uint32_t requireB, uint32_t requireC,
                                                              uint32_t forbidA, uint32_t forbidB, uint32_t forbidC,
                                                              int descending) {
    AiSectorEval* eval = m_sectors.FindEval(*sector);
    if (!eval) return nullptr;
    return eval->RankSystemsByDistance(target, requireA, requireB, requireC, forbidA, forbidB, forbidC, descending);
}

AiRankedList* AiSideController::RankSystemsOfSectorRandomly(const ObjectKey* sector, uint32_t requireA, uint32_t requireB,
                                                            uint32_t requireC, uint32_t forbidA, uint32_t forbidB,
                                                            uint32_t forbidC, int descending) {
    AiSectorEval* eval = m_sectors.FindEval(*sector);
    if (!eval) return nullptr;
    return eval->RankSystemsRandomly(requireA, requireB, requireC, forbidA, forbidB, forbidC, descending);
}

AiRankedList* AiSideController::RankFleets(uint32_t requireFlags, uint32_t forbidFlags, uint32_t statIndex,
                                           int descending) {
    const FleetMasks masks{requireFlags, forbidFlags};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiFleetEval* fleet = static_cast<AiFleetEval*>(m_fleets.First()); fleet; fleet = Next(fleet)) {
        if (masks.Matches(fleet)) AddItem(list, KeyOf(fleet), fleet->m_stat[statIndex & 0xffff]);
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankFleetsBySectorDistance(const ObjectKey* sector, uint32_t requireFlags,
                                                           uint32_t forbidFlags, int descending) {
    const FleetMasks masks{requireFlags, forbidFlags};
    AiRankedList* list = nullptr;
    ObjectKey sectorKey;
    if (!ResolveToFamily(m_side, sector, kSectorFirst, kSectorLast, &sectorKey)) return list;
    AiSectorEval* origin = m_sectors.FindEval(sectorKey);
    if (!origin) return list;
    const int16_t originX = origin->m_x;
    const int16_t originY = origin->m_y;
    list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiFleetEval* fleet = static_cast<AiFleetEval*>(m_fleets.First()); fleet; fleet = Next(fleet)) {
        if (!masks.Matches(fleet)) continue;
        ObjectKey fleetKey = KeyOf(fleet);
        int position;
        GetPositionOf(&position, &fleetKey);
        AddItem(list, KeyOf(fleet), DistanceSquared(originX, originY, PackedX(position), PackedY(position)));
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankFleetsBySystemDistance(const ObjectKey* system, uint32_t requireFlags,
                                                           uint32_t forbidFlags, int descending) {
    const FleetMasks masks{requireFlags, forbidFlags};
    AiRankedList* list = nullptr;
    ObjectKey systemKey;
    if (!ResolveToFamily(m_side, system, kSystemFirst, kSystemLast, &systemKey)) return list;
    AiSystemEval* origin = m_systems.FindEval(systemKey);
    if (!origin) return list;
    const int16_t originX = PackedX(origin->m_position);
    const int16_t originY = PackedY(origin->m_position);
    list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiFleetEval* fleet = static_cast<AiFleetEval*>(m_fleets.First()); fleet; fleet = Next(fleet)) {
        if (!masks.Matches(fleet)) continue;
        ObjectKey fleetKey = KeyOf(fleet);
        int position;
        GetPositionOf(&position, &fleetKey);
        AddItem(list, KeyOf(fleet), DistanceSquared(originX, originY, PackedX(position), PackedY(position)));
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankFleetsByX34(uint32_t minX34, uint32_t requireFlags, uint32_t forbidFlags,
                                                int descending) {
    const FleetMasks masks{requireFlags, forbidFlags};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiFleetEval* fleet = static_cast<AiFleetEval*>(m_fleets.First()); fleet; fleet = Next(fleet)) {
        if (minX34 < static_cast<uint32_t>(fleet->m_x34) && masks.Matches(fleet)) AddItem(list, KeyOf(fleet), fleet->m_x34);
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankShipsOfFleet(const ObjectKey* fleet, uint32_t requireFlags, uint32_t forbidFlags,
                                                 uint32_t statIndex, int descending) {
    AiFleetEval* eval = m_fleets.FindEval(*fleet);
    if (!eval) return nullptr;
    return eval->RankShips(requireFlags, forbidFlags, statIndex, descending);
}

AiRankedList* AiSideController::RankFleetsByTarget(int x30, uint32_t requireFlags, uint32_t forbidFlags,
                                                   uint32_t statIndex, int descending) {
    const FleetMasks masks{requireFlags, forbidFlags};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiFleetEval* fleet = static_cast<AiFleetEval*>(m_fleets.First()); fleet; fleet = Next(fleet)) {
        if (fleet->m_x30 == x30 && masks.Matches(fleet)) AddItem(list, KeyOf(fleet), fleet->m_stat[statIndex & 0xffff]);
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankFleetsByTargetAndSystemDistance(const ObjectKey* system, int x30,
                                                                    uint32_t requireFlags, uint32_t forbidFlags,
                                                                    int descending) {
    const FleetMasks masks{requireFlags, forbidFlags};
    AiRankedList* list = nullptr;
    ObjectKey systemKey;
    if (!ResolveToFamily(m_side, system, kSystemFirst, kSystemLast, &systemKey)) return list;
    AiSystemEval* origin = m_systems.FindEval(systemKey);
    if (!origin) return list;
    const int16_t originX = PackedX(origin->m_position);
    const int16_t originY = PackedY(origin->m_position);
    list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiFleetEval* fleet = static_cast<AiFleetEval*>(m_fleets.First()); fleet; fleet = Next(fleet)) {
        if (fleet->m_x30 != x30 || !masks.Matches(fleet)) continue;
        ObjectKey fleetKey = KeyOf(fleet);
        int position;
        GetPositionOf(&position, &fleetKey);
        AddItem(list, KeyOf(fleet), DistanceSquared(originX, originY, PackedX(position), PackedY(position)));
    }
    list->Number();
    return list;
}

AiRankedList* AiSideController::RankCharacters(uint32_t requireFlags, uint32_t requireCaps, uint32_t forbidFlags,
                                               uint32_t forbidCaps, uint32_t valueIndex, int minValue, int maxValue,
                                               int descending) {
    return m_characters.RankByValue(requireFlags, requireCaps, forbidFlags, forbidCaps, valueIndex, minValue, maxValue,
                                    descending);
}

void AiSideController::MarkFleetSystemChanged(const ObjectKey* fleetKey) {
    if (!InFamilies(fleetKey->Family(), 8, 0x10)) return;
    AiFleetEval* fleet = m_fleets.FindEval(*fleetKey);
    if (!fleet) return;
    ObjectKey systemKey = fleet->m_systemKey;
    if (!InFamilies(systemKey.Family(), kSystemFirst, kSystemLast)) return;
    AiSystemEval* system = m_systems.FindEval(systemKey);
    if (system) system->MarkChanged();
}

int AiSideController::ChooseClass(ObjectKey* out, int recordType, uint32_t requiredFlags, int mode, int descending) {
    return m_classes.ChooseFromSide(out, GetSideObject(m_side, m_side), recordType, requiredFlags, mode, descending);
}

AiRankedList* AiSideController::RankClasses(int recordType, uint32_t requiredFlags, uint32_t valueMask, int descending) {
    return m_classes.RankClasses(GetSideObject(m_side, m_side), recordType, requiredFlags, valueMask, descending);
}

ObjectKey* AiSideController::FindNearestSystemWithFleets(ObjectKey* out, const ObjectKey* fromSector, uint32_t requireA,
                                                         uint32_t requireB, uint32_t requireC, uint32_t forbidA,
                                                         uint32_t forbidB, uint32_t forbidC) {
    ObjectKey best;
    ObjectKey originKey;
    ObjectKey picked;
    int bestDistance = 2000000000;
    AiRankedSelector selector;
    AiSectorEval* origin = static_cast<AiSectorEval*>(m_sectors.Find(fromSector));
    if (origin) {
        originKey = KeyOf(origin);
        const int16_t originX = origin->m_x;
        const int16_t originY = origin->m_y;
        for (AiSectorEval* sector = static_cast<AiSectorEval*>(m_sectors.First()); sector; sector = Next(sector)) {
            if (sector->m_key.m_value == originKey.m_value) continue;
            const int distance = DistanceSquared(originX, originY, sector->m_x, sector->m_y);
            if (distance >= bestDistance) continue;
            ObjectKey sectorKey = KeyOf(sector);
            selector.Add(RankSystemsOfSectorRandomly(&sectorKey, requireA, requireB, requireC, forbidA, forbidB, forbidC, 1));
            ObjectKey scratch;
            picked = *selector.PickBest(&scratch);
            selector.Reset();
            if (InFamilies(picked.Family(), kSystemFirst, kSystemLast)) {
                best = picked;
                bestDistance = distance;
            }
        }
    }
    *out = best;
    return out;
}

void AiSideController::RetirePlanner(uint32_t id) {
    IdNode* planner = m_planners.DetachById(id);
    if (!planner) return;
    MarkDirty();
    m_idlePlanners.AddWithId(planner);
}

void AiSideController::CountPlanner(AiMissionPlanner* planner) {
    if (planner) ++m_plannerCount[static_cast<uint16_t>(planner->GetKindIndex())];
}

void AiSideController::UncountPlanner(AiMissionPlanner* planner) {
    if (planner) --m_plannerCount[static_cast<uint16_t>(planner->GetKindIndex())];
}

void AiSideController::CountAgent(AiAgentSimple* agent) {
    if (agent) ++m_agentCount[static_cast<uint16_t>(agent->GetCountSlot())];
}

void AiSideController::RetireAgent(uint32_t id) {
    IdNode* agent = m_agents.DetachById(id);
    if (!agent) return;
    MarkDirty();
    m_idleAgents.AddWithId(agent);
}

int* AiSideController::GetPositionOf(int* out, const ObjectKey* key) {
    int position = 0;
    ObjectKey systemKey;
    if (ResolveToFamily(m_side, key, kSystemFirst, kSystemLast, &systemKey)) {
        AiSystemEval* system = m_systems.FindEval(systemKey);
        if (system) position = system->m_position;
    }
    *out = position;
    return out;
}

int AiSideController::DistanceSquared(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

void AiSideController::AccumulateSector(const int* p, const int* flags24, const int* flags2c,
                                        const int* flags28, int) {
    auto add = [this, p](int totalOffset, int sectorOffset) { Stat(totalOffset) += p[sectorOffset / 4]; };
    add(0x1ac, 0xc);
    add(0x1b0, 0x10);
    add(0x1b4, 0x14);
    add(0x1d0, 0x38);
    add(0x1d4, 0x3c);
    add(0x1d8, 0x8c);
    add(0x1dc, 0x90);
    add(0x1e0, 0x94);
    add(0x1e8, 0x24);
    add(0x1ec, 0x28);
    add(0x1f0, 0xa0);
    add(0x1f4, 0x98);
    add(0x1f8, 0xc0);
    add(0x1cc, 0x7c);
    add(0x1fc, 0x2c);
    add(0x200, 0x80);
    add(0x204, 0x11c);
    add(0x21c, 0xf4);
    add(0x220, 0x118);
    add(0x224, 0x130);
    add(0x214, 0xf8);
    add(0x218, 0x108);
    if (Stat(0x20c) < p[0xc4 / 4]) Stat(0x20c) = p[0xc4 / 4];
    if (Stat(0x208) < p[0xc8 / 4]) Stat(0x208) = p[0xc8 / 4];
    if (Stat(0x2fc) < p[0xd0 / 4]) Stat(0x2fc) = p[0xd0 / 4];
    const uint32_t c = *flags2c;
    if (c & 4) {
        ++Stat(0x1b8);
        if (c & 0x1000000) m_flags.m_value |= 0x10;
        if (c & 0x2000000) m_flags.m_value |= 0x20;
        if (c & 0x200) m_flags.m_value |= 0x80;
        if (*flags28 & 0x8000000) m_flags.m_value |= 0x100;
    }
    if (c & 8) {
        ++Stat(0x1bc);
        if (p[0x30 / 4] < Stat(0x210) || Stat(0x210) == 0) Stat(0x210) = p[0x30 / 4];
    }
    if (c & 0x10) ++Stat(0x1c0);
    if (c & 0x80000000) m_flags.m_value |= 0x400;
    if ((*flags24 & 0x40000000) == 0) ++Stat(0x1c4);
    if ((c & 2) == 0) ++Stat(0x1c8);
}

void AiSideController::AccumulateFleet(const ObjectKey*, const int* s, const int* flags) {
    auto add = [this, s](int totalOffset, int fleetOffset) { Stat(totalOffset) += s[fleetOffset / 4]; };
    const uint32_t f = *flags;
    if (f & 1) {
        if (f & 4) {
            add(0x268, 0x48);
            add(0x26c, 0x4c);
            add(0x270, 0x50);
            add(0x274, 0x64);
            add(0x278, 0x68);
            add(0x27c, 0x28);
            add(0x280, 0x18);
        }
        add(0x25c, 0x2c);
        add(0x260, 0x6c);
        add(0x258, 0x64);
        add(0x24c, 0x38);
        add(0x250, 0x3c);
        add(0x254, 0x40);
        if (f & 0x10) m_flags.m_value |= 4;
        if (f & 0x20) m_flags.m_value |= 8;
        add(0x264, 0x44);
        return;
    }
    ++Stat(0x284);
    add(0x288, 0x64);
    add(0x28c, 0x28);
    add(0x290, 0x68);
    if (Stat(0x294) < s[0x64 / 4]) {
        Stat(0x294) = s[0x64 / 4];
        Stat(0x298) = s[0x68 / 4];
        Stat(0x29c) = s[0x28 / 4];
    }
}

void AiSideController::AccumulateCharacter(const ObjectKey*, const int* values, const int* flags,
                                           const int* capabilities) {
    const uint32_t f = *flags;
    const uint32_t caps = *capabilities;
    if ((f & 2) == 0) return;
    Stat(0x2c4) += values[0x24 / 4];
    if ((f & 0x20000000) != 0) {
        if (f & 0x40) m_flags.m_value |= 0x200;
        if (caps & 0x2000) ++Stat(0x2c8);
        if (caps & 0x800) ++Stat(0x2cc);
        if (caps & 0x1000) ++Stat(0x2d0);
        return;
    }
    if ((caps & 0x10000000) == 0) {
        if (caps & 0x20000000) {
            ++Stat(0x2e0);
            if (caps & 0x800000) ++Stat(0x2f0);
        }
    } else {
        ++Stat(0x2dc);
        if (caps & 0x800000) ++Stat(0x2ec);
    }
    if (caps & 0x40000000) {
        ++Stat(0x2e4);
        if (caps & 0x800000) ++Stat(0x2f4);
    }
    if (caps & 0x80000000) {
        ++Stat(0x2e8);
        if (caps & 0x800000) ++Stat(0x2f8);
    }
}

IntPair* GetSidePairAt58(Side* side, IntPair* out) { return new (out) IntPair(side->m_maintState); }
