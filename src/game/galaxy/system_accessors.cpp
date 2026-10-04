#include "character/character.h"
#include "data/dat_record.h"
#include "fleet/fleet.h"
#include "galaxy/system.h"
#include "gameobj/object_family.h"

// Non-virtual accessors and child counters of System. The per-family iterators of the original (FleetIterator, ...)
// belong to other modules; plain ChildIterators with the same family ranges are used here.

namespace {

constexpr uint32_t kDestroyedStateFlag = 0x8;
constexpr uint32_t kFleetFlag02 = 0x2;
constexpr uint32_t kFleetFlag04 = 0x4;
constexpr uint32_t kSystemDataFlagBit0 = 0x1;
constexpr uint32_t kSystemDataFlagBit3 = 0x8;
constexpr uint32_t kSideAlliance = 1;
constexpr uint32_t kSideEmpire = 2;
constexpr uint32_t kSideNone = 3;
constexpr uint32_t kStateUsable = 1;
constexpr uint32_t kStateExisting = 3;
constexpr uint32_t kStateAlways = 4;
constexpr uint32_t kRegimentKeyCounted = 0x10000006;
constexpr FamilyRange kDeathStars(0x18, 0x1c);

bool isSideOneToThree(int side) { return side >= 1 && side <= 3; }
bool isSideOneOrTwo(int side) { return isSideOneToThree(side) && side != static_cast<int>(kSideNone); }

int sideWord(const int (&words)[3], int side) { return isSideOneToThree(side) ? words[side - 1] : 0; }

#define SLOT_PAD10(p)                                                                                    \
    virtual void p##0() = 0; virtual void p##1() = 0; virtual void p##2() = 0; virtual void p##3() = 0;  \
    virtual void p##4() = 0; virtual void p##5() = 0; virtual void p##6() = 0; virtual void p##7() = 0;  \
    virtual void p##8() = 0; virtual void p##9() = 0;

// Foreign classes (a ship or character, a manufacturing manager): only the vtable slots this file calls (123-125,
// byte offsets 0x1ec-0x1f4) are named; slots 0-122 are padding.
struct ForeignSlots {
    SLOT_PAD10(a) SLOT_PAD10(b) SLOT_PAD10(c) SLOT_PAD10(d) SLOT_PAD10(e) SLOT_PAD10(f)
    SLOT_PAD10(g) SLOT_PAD10(h) SLOT_PAD10(i) SLOT_PAD10(j) SLOT_PAD10(k) SLOT_PAD10(l)
    virtual void m0() = 0;
    virtual void m1() = 0;
    virtual void m2() = 0;
    virtual int Slot123() = 0;  // 0x1ec
    virtual int Slot124() = 0;  // 0x1f0
    virtual int Slot125() = 0;  // 0x1f4
};

ForeignSlots* asForeign(GameObjNotifier* object) { return reinterpret_cast<ForeignSlots*>(object); }

int countChildren(System* system, uint32_t side, const FamilyRange* range, uint32_t stateFilter) {
    ChildIterator it(system, side, range, stateFilter);
    return it.Count();
}

int countChildrenOfFleets(System* system, uint32_t side, uint32_t stateFilter, const FamilyRange* range) {
    int total = 0;
    ChildIterator fleets(system, side, &family::kFleets, stateFilter);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
        ChildIterator ships(fleets.Current(), side, range, stateFilter);
        total += ships.Count();
    }
    return total;
}

uint32_t recordKeyOf(GameObjNotifier* object) {
    const DatRecord* record = object->m_record;
    return record ? (record->m_third << 24) | (record->m_id & 0xffffff) : 0;
}

// Keeps the higher-valued object in *best: the candidate replaces it unless the current one is at least as good.
template <int (ForeignSlots::*Value)()>
void keepBetter(GameObjNotifier** best, GameObjNotifier* candidate) {
    if (*best) {
        const int current = (asForeign(*best)->*Value)();
        const int offered = (asForeign(candidate)->*Value)();
        if (current >= offered) return;
    }
    *best = candidate;
}

int findFleetWithFlag(System* system, uint32_t flag, uint32_t side, GameObjNotifier** out) {
    *out = nullptr;
    ChildIterator fleets(system, side, &family::kFleets, kStateAlways);
    for (fleets.Begin(); fleets.Current() && !*out; fleets.Advance()) {
        Fleet* fleet = static_cast<Fleet*>(fleets.Current());
        if (!(fleet->m_stateFlags & kDestroyedStateFlag) && (fleet->m_fleetFlags & flag)) *out = fleet;
    }
    return 1;
}

}  // namespace

int System::GetLoyaltyFor(int side) { return side == static_cast<int>(kSideAlliance) ? m_loyalty : 100 - m_loyalty; }

int System::SetLoyaltyFor(int value, int side, NotifyContext* ctx) {
    return SetLoyalty(side == static_cast<int>(kSideAlliance) ? value : 100 - value, ctx);
}

int System::CountFighterSquadrons(uint32_t side, uint32_t stateFilter) {
    return countChildren(this, side, &family::kFighterSquadrons, stateFilter);
}

int System::CountTroopRegiments(uint32_t side, uint32_t stateFilter) {
    return countChildren(this, side, &family::kTroopRegiments, stateFilter);
}

int System::CountCapitalShips(uint32_t side, uint32_t stateFilter) {
    return countChildrenOfFleets(this, side, stateFilter, &family::kCapitalShips);
}

int System::CountDeathStars(uint32_t side, uint32_t stateFilter) {
    return countChildrenOfFleets(this, side, stateFilter, &kDeathStars);
}

int System::CountEmpireRegimentsOfKey6() {
    int count = 0;
    ChildIterator regiments(this, kSideEmpire, &family::kTroopRegiments, kStateUsable);
    for (regiments.Begin(); regiments.Current(); regiments.Advance())
        count += recordKeyOf(regiments.Current()) == kRegimentKeyCounted;
    return count;
}

TrackedStat* System::GetStat0() { return m_data ? &GetData()->m_stats[0] : nullptr; }
TrackedStat* System::GetStat3() { return m_data ? &GetData()->m_stats[3] : nullptr; }
TrackedStat* System::GetStat4() { return m_data ? &GetData()->m_stats[4] : nullptr; }
TrackedStat* System::GetStat5() { return m_data ? &GetData()->m_stats[5] : nullptr; }

uint32_t System::GetDataFlagBit0() { return m_data ? GetData()->m_flags80 & kSystemDataFlagBit0 : 0; }
uint32_t System::GetDataFlagBit3() { return m_data ? GetData()->m_flags80 & kSystemDataFlagBit3 : 1; }

int System::FindCharacterByCommandKind(int kind, GameObjNotifier** out) {
    *out = nullptr;
    CharacterIterator characters(this, kStateExisting, ControlSide());
    for (characters.Begin(); characters.Current() && !*out; characters.Advance()) {
        Character* character = static_cast<Character*>(characters.Current());
        if (character->m_commandKind == kind) *out = character;
    }
    return 1;
}

int System::FindCharacterUnused(GameObjNotifier** out, int, int) {
    *out = nullptr;
    CharacterIterator characters(this, kStateExisting);
    for (characters.Begin(); characters.Current(); characters.Advance()) {
    }
    return 1;
}

int System::FindBestShipOfCommandKind2(uint32_t side, GameObjNotifier** out) {
    int found = 1;
    *out = nullptr;
    ChildIterator fleets(this, side, &family::kFleets, kStateUsable);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
        GameObjNotifier* ship = nullptr;
        found = static_cast<Fleet*>(fleets.Current())->FindShipByNumber(2, &ship);
        if (found && ship) keepBetter<&ForeignSlots::Slot125>(out, ship);
        if (!found) break;
    }
    return found;
}

int System::FindBestShipOfCommandKind1(uint32_t side, GameObjNotifier** out) {
    int found = FindCharacterByCommandKind(1, out);
    ChildIterator fleets(this, side, &family::kFleets, kStateUsable);
    for (fleets.Begin(); found && fleets.Current(); fleets.Advance()) {
        GameObjNotifier* ship = nullptr;
        found = static_cast<Fleet*>(fleets.Current())->FindShipByNumber(1, &ship);
        if (found && ship) keepBetter<&ForeignSlots::Slot124>(out, ship);
    }
    return found;
}

uint32_t System::GetRecordField44() { return m_record ? static_cast<SdRecord*>(m_record)->m_field44 : 0; }

void System::GetRecordField4c(uint32_t* out) { *out = m_record ? static_cast<SdRecord*>(m_record)->m_field4c : 0; }

int System::GetEnergy() { return m_energy; }

int System::GetRawMaterial() { return m_rawMaterial; }

GameObjNotifier* System::FindManufacturingManagerBySlot123(int value) {
    ChildIterator managers(this, &family::kManufacturingManagers, kStateAlways);
    for (managers.Begin(); managers.Current(); managers.Advance())
        if (asForeign(managers.Current())->Slot123() == value) break;
    return managers.Current();
}

int System::HasFleetsOrFighterSquadrons(uint32_t side) {
    const int fleets = ChildIterator(this, side, &family::kFleets, kStateUsable).HasCurrent();
    const int squadrons = ChildIterator(this, side, &family::kFighterSquadrons, kStateUsable).HasCurrent();
    return fleets || squadrons ? 1 : 0;
}

int System::FindFleetWithFlag02(uint32_t side, GameObjNotifier** out) {
    return findFleetWithFlag(this, kFleetFlag02, side, out);
}

int System::FindFleetWithFlag04(uint32_t side, GameObjNotifier** out) {
    return findFleetWithFlag(this, kFleetFlag04, side, out);
}

int System::GetBlockadingSide(uint32_t* out) {
    *out = kSideNone;
    int found = (m_flags & system_flag::kBlockade) ? 1 : 0;
    if (found) {
        ChildIterator fleets(this, &family::kFleets, kStateUsable);
        fleets.Begin();
        found = fleets.Current() != nullptr;
        if (found) *out = fleets.Current()->ControlSide();
    }
    return found;
}

int System::GetMoreLoyalSide() {
    if (GetLoyaltyFor(kSideAlliance) > GetLoyaltyFor(kSideEmpire)) return 1;
    return GetLoyaltyFor(kSideEmpire) <= GetLoyaltyFor(kSideAlliance) ? 3 : 2;
}

int System::IsBlockaderMoreLoyal(uint32_t* out) {
    uint32_t blockader;
    *out = 0;
    const int found = GetBlockadingSide(&blockader);
    if (found) *out = static_cast<uint32_t>(GetMoreLoyalSide()) == blockader;
    return found;
}

int System::GetDataWord48(int side) { return sideWord(GetData()->m_word48, side); }
int System::GetDataWord54(int side) { return sideWord(GetData()->m_word54, side); }
int System::GetDataWord68(int side) { return sideWord(GetData()->m_word68, side); }

int System::SumWords48And54(int side) {
    int total = 0;
    if (!isSideOneOrTwo(side)) return total;
    const int owner = GetDataWord60();
    if (!isSideOneOrTwo(owner)) return total;
    total = GetDataWord48(side) + GetDataWord54(side);
    if (owner != side) total = GetDataWord54(kSideNone) + total + GetDataWord48(kSideNone);
    return total;
}

int System::GetDataWord60() { return GetData()->m_word60; }
int System::GetDataWord64() { return m_data ? GetData()->m_word64 : 0; }
int System::GetDataWord74() { return m_data ? GetData()->m_word74 : 0; }
int System::GetDataWord78() { return m_data ? GetData()->m_word78 : 0; }
int System::GetDataWord7c() { return m_data ? GetData()->m_word7c : 0; }
