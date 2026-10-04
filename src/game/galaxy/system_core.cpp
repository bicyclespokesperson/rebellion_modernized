#include "core/vptr_store.h"
#include "galaxy/system.h"
#include "galaxy/system_rules_decl.h"
#include "gameobj/object_family.h"

// The shared permission check of the object tree (0x553410); fills *out and returns 0 only if it could not run.
int FUN_00553410(GameObj* object, uint32_t sideA, uint32_t sideB, int requireCompleted, int requireSide, StatusCode* out);
// The game-rules constants read by the checks below (GNPRTB values).
int FUN_0055a0b0();
int FUN_0055a0f0(int value);

namespace {

constexpr uint32_t kDestroyedState = 0x8;
constexpr uint32_t kControlSideNeutral = 3;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusDestroyed = 0x22;
constexpr int kStatusSystemCategory = 0x90;
constexpr int kStatusNoCapacity = 5;
constexpr int kStatusNoBlockade = 6;
constexpr int kStatusNoRoom = 4;
constexpr int kStatusNoDeathStarRoom = 3;
constexpr int kStatusNoSlotFacility = 2;
constexpr int kStatusNoSlotEnergy = 1;
constexpr int kOrderMoveKind = 0x204;
constexpr int kOrderCharacterKind = 0x240;
constexpr int kOrderCharacterKind2 = 0x241;
constexpr int kOrderAssaultKind = 0x3f0;
constexpr int kDestroyedByCapture = 0xc;
constexpr int kDestroyedByRetreat = 0xe;
constexpr int kDestroyedByLoss = 0xf;

constexpr FamilyRange kFleetRange(0x08, 0x10);
constexpr FamilyRange kTroopRange(0x10, 0x14);
constexpr FamilyRange kCapitalShipRange(0x14, 0x20);
constexpr FamilyRange kDeathStarRange(0x18, 0x1c);
constexpr FamilyRange kFighterRange(0x1c, 0x20);
constexpr FamilyRange kUnitRange(0x10, 0x20);
constexpr FamilyRange kFacilityRange(0x20, 0x30);
constexpr FamilyRange kFacilityKindARange(0x2c, 0x2d);
constexpr FamilyRange kFacilityKindBRange(0x24, 0x25);
constexpr FamilyRange kFacilityKindCRange(0x28, 0x30);
constexpr FamilyRange kCharacterRange(0x30, 0x40);
constexpr FamilyRange kMissionRange(0x40, 0x80);
constexpr FamilyRange kManagerRange(0xa0, 0xb0);

void setStatus(StatusCode* out, int category, int code) { *out = StatusCode(category, static_cast<uint32_t>(code)); }

bool statusIsOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(kStatusOk); }

bool familyIn(uint32_t family, const FamilyRange& range) { return family >= range.m_first && family < range.m_second; }

// The request's family range lies completely inside `outer`.
bool rangeWithin(const FamilyRange* range, const FamilyRange& outer) {
    return range->m_first >= outer.m_first && range->m_second <= outer.m_second;
}

// Virtual call of a slot that only the unit subclasses (ships, troops) declare; they are reconstructed elsewhere.
int callUnitSlot(GameObj* unit, int slot) {
    using SlotFunction = int(__thiscall*)(GameObj*);
    void** table = *reinterpret_cast<void***>(unit);
    return reinterpret_cast<SlotFunction>(table[slot])(unit);
}

constexpr int kUnitSlotA = 112;
constexpr int kUnitSlotB = 114;

}  // namespace

// ---- construction -----------------------------------------------------------------------------------------

System::System()
    : GameObj(),
      m_loyalty(50),
      m_energy(0),
      m_energyAllocated(0),
      m_rawMaterial(0),
      m_rawMaterialAllocated(0),
      m_smugglingPercent(0),
      m_productionModifier(100),
      m_troopRegWithdrawPercent(100),
      m_controlBits(0),
      m_troopRegSurplus(0),
      m_troopRegRequired(0),
      m_controlData(),
      m_flags(0) {
    SetFlag31(GetFlag31() != 0);
}

System::~System() { REB_KEEP_VPTR_STORE(); }

// ---- event kinds and order checks -----------------------------------------------------------------------

int System::GetEventKinds(int type, EventKindSet* out) {
    BuildEventKinds();
    EventKindSet* kinds = nullptr;
    const int result = SelectEventKinds(static_cast<uint32_t>(type), &kinds);
    if (result) *out = *kinds;
    return result;
}

int System::CheckOrder(int type, int orderKind, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusNotSupported);
    BuildEventKinds();
    EventKindSet* kinds = nullptr;
    const int result = SelectEventKinds(static_cast<uint32_t>(type), &kinds);
    if (result && kinds->Contains(orderKind)) setStatus(out, kStatusCategory, kStatusOk);
    return result;
}

int System::CheckSameControl(GameObj* other, StatusCode* out) {
    if (!(m_flags & system_flag::kPopulated) && ControlSide() == kControlSideNeutral &&
        familyIn(other->GetFamily(), kTroopRange)) {
        setStatus(out, kStatusCategory, kStatusOk);
        return 1;
    }
    return GameObj::CheckSameControl(other, out);
}

// Whether a unit order may be given here: `rangeArg` is really a FamilyRange* (the base class declares it as an int).
int System::Virtual29(int sideA, int sideB, int kind, int rangeArg, StatusCode* out) {
    const FamilyRange* range = reinterpret_cast<const FamilyRange*>(static_cast<intptr_t>(rangeArg));
    setStatus(out, kStatusCategory, kStatusOk);
    const bool fleetMove = kind == kOrderMoveKind && rangeWithin(range, kFleetRange);
    const bool troopMove = kind == kOrderMoveKind && rangeWithin(range, kTroopRange);
    const bool characterOrder = kind == kOrderCharacterKind && rangeWithin(range, kCharacterRange);
    const bool characterOrder2 = kind == kOrderCharacterKind2 && rangeWithin(range, kCharacterRange);

    if (statusIsOk(out) && (m_stateFlags & kDestroyedState) && !fleetMove && !characterOrder)
        setStatus(out, kStatusCategory, kStatusDestroyed);
    int result = 1;
    if (statusIsOk(out)) {
        result = FUN_00553410(this, static_cast<uint32_t>(sideA), static_cast<uint32_t>(sideB), 1, 1, out);
        if (result != 0 && !statusIsOk(out)) {
            if (fleetMove || characterOrder) setStatus(out, kStatusCategory, kStatusOk);
            if (troopMove && (m_flags & system_flag::kExplored) && ControlSide() == kControlSideNeutral &&
                !(m_flags & system_flag::kPopulated))
                setStatus(out, kStatusCategory, kStatusOk);
        }
    }
    if (result != 0 && statusIsOk(out) && (m_flags & system_flag::kBlockade)) {
        setStatus(out, kStatusSystemCategory, 4);
        if (fleetMove || characterOrder || characterOrder2 || kind == kOrderAssaultKind)
            setStatus(out, kStatusCategory, kStatusOk);
    }
    return result;
}

// How many more units of the requested family range fit here (*remaining) and which of the two limits applies.
int System::Virtual30(const FamilyRange* range, int stateFilter, int* allowed, int* unlimited, int* remaining,
                      StatusCode* out) {
    *allowed = 1;
    *unlimited = 1;
    *remaining = 0;
    setStatus(out, kStatusCategory, kStatusOk);
    if (rangeWithin(range, kFacilityRange)) {
        const bool kindA = rangeWithin(range, kFacilityKindARange);
        *unlimited = 0;
        if (kindA) {
            ChildIterator facilities(this, &kFacilityKindARange, static_cast<uint32_t>(stateFilter));
            *remaining = m_rawMaterial - facilities.Count();
            setStatus(out, kStatusSystemCategory, kStatusNoSlotFacility);
        } else {
            ChildIterator facilities(this, &kFacilityRange, static_cast<uint32_t>(stateFilter));
            *remaining = m_energy - facilities.Count();
            setStatus(out, kStatusSystemCategory, kStatusNoSlotEnergy);
        }
        return 1;
    }
    const bool supported = rangeWithin(range, kTroopRange) || rangeWithin(range, kFighterRange) ||
                           rangeWithin(range, kFleetRange) || rangeWithin(range, kManagerRange) ||
                           rangeWithin(range, kCharacterRange) || rangeWithin(range, kMissionRange);
    if (supported)
        *unlimited = 1;
    else
        *allowed = 0;
    return 1;
}

int System::Virtual32(const FamilyRange* range, int stateFilter, int* allowed, int* unlimited, int* remaining,
                      StatusCode* out) {
    *allowed = 1;
    *unlimited = 1;
    *remaining = 0;
    setStatus(out, kStatusCategory, kStatusOk);
    if (rangeWithin(range, kDeathStarRange)) {
        int deathStars = 0;
        ChildIterator fleets(this, &kFleetRange, static_cast<uint32_t>(stateFilter));
        for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
            ChildIterator ships(fleets.Current(), &kDeathStarRange, static_cast<uint32_t>(stateFilter));
            deathStars += ships.Count();
        }
        *unlimited = 0;
        constexpr int kDeathStarLimit = 1;
        *remaining = kDeathStarLimit - deathStars;
        setStatus(out, kStatusSystemCategory, kStatusNoDeathStarRoom);
    }
    return 1;
}

int System::Virtual111(int a, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    if (statusIsOk(out)) {
        if (m_stateFlags & kDestroyedState) setStatus(out, kStatusCategory, kStatusDestroyed);
        if (statusIsOk(out)) {
            if (!(m_flags & system_flag::kBlockade)) setStatus(out, kStatusSystemCategory, kStatusNoBlockade);
            if (statusIsOk(out)) {
                ChildIterator facilities(this, &kFacilityKindBRange, 1);
                if (FUN_0055a0b0() <= facilities.Count()) setStatus(out, kStatusSystemCategory, kStatusNoCapacity);
            }
        }
    }
    return 1;
}

int System::Virtual112(int a, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    const int result = Virtual111(a, out);
    if (!result) setStatus(out, kStatusCategory, kStatusOk);
    return result;
}

int System::Virtual113(int a, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    if (statusIsOk(out) && (m_stateFlags & kDestroyedState)) setStatus(out, kStatusCategory, kStatusDestroyed);
    return 1;
}

int System::Virtual114(int a, StatusCode* out) {
    setStatus(out, kStatusCategory, kStatusOk);
    const int result = Virtual113(a, out);
    if (!result) setStatus(out, kStatusCategory, kStatusOk);
    return result;
}

// ---- identity and destruction ---------------------------------------------------------------------------

int System::GetOwnerKey(ObjectKey* out) {
    ObjectKey key;
    GetKey(&key);
    *out = key;
    return 1;
}

int System::ChangeDestroyedReason(int reason, NotifyContext* ctx) {
    NotifyContext sideContext(*ctx);
    sideContext.m_flag04 = 1;
    sideContext.m_flag08 = 3;
    int ok = 1;
    if (System* alliance = ViewAsSystem(1)) ok = alliance->ApplyBombard(&sideContext) != 0;
    if (System* empire = ViewAsSystem(2)) {
        const int applied = empire->ApplyBombard(&sideContext);
        ok = applied && ok;
    }
    int applied = GameObj::ChangeDestroyedReason(reason, ctx);
    ok = applied && ok;
    applied = ApplyAllToView(1, &sideContext);
    ok = applied && ok;
    applied = ApplyAllToView(2, &sideContext);
    ok = applied && ok;
    return ok;
}

int System::ApplyAllToViewOf(int view, int unused, NotifyContext* ctx) {
    return GameObj::ApplyAllToViewOf(view, unused, ctx) != 0;
}

// ---- reactions to child objects appearing or changing (slots 47-53; `ctx` and `child` arrive as ints) -----------

int System::Virtual47(int ctxArg) {
    NotifyContext* ctx = reinterpret_cast<NotifyContext*>(static_cast<intptr_t>(ctxArg));
    int ok = GameObj::Virtual47(ctxArg);
    int applied = SetExplored(1, ctx);
    ok = applied && ok;
    applied = SetNeverBeenControlled(1, ctx);
    return applied && ok;
}

int System::Virtual48(int ctxArg) {
    NotifyContext* ctx = reinterpret_cast<NotifyContext*>(static_cast<intptr_t>(ctxArg));
    int ok = GameObj::Virtual48(ctxArg);
    int applied = FUN_0050ad60(this, ctx);
    ok = applied && ok;
    applied = FUN_0050ad80(this, ctx);
    ok = applied && ok;
    applied = FUN_0050b0b0(this, ctx);
    ok = applied && ok;
    applied = FUN_0050b230(this, ctx);
    ok = applied && ok;
    applied = FUN_0050b2c0(this, ctx);
    ok = applied && ok;
    applied = FUN_0050b310(this, ctx);
    ok = applied && ok;
    applied = FUN_0050b610(this, ctx);
    ok = applied && ok;
    applied = FUN_0050b5a0(this, ctx);
    ok = applied && ok;
    applied = FUN_0050b500(this, ctx);
    ok = applied && ok;
    applied = FUN_0050ba90(this, ctx);
    ok = applied && ok;
    applied = FUN_0050bb70(this, ctx);
    ok = applied && ok;
    applied = FUN_0050bc60(this, ctx);
    ok = applied && ok;
    applied = FUN_0050be00(this, ctx);
    ok = applied && ok;
    applied = FUN_0050c350(this, ctx);
    ok = applied && ok;
    applied = FUN_0050b8e0(this, ctx);
    ok = applied && ok;
    applied = GameObj::Virtual48(ctxArg);
    ok = applied && ok;
    applied = FUN_0050b800(this, ctx);
    ok = applied && ok;
    applied = FUN_0050bb00(this, ctx);
    return applied && ok;
}

int System::Virtual49(int childArg, int added, int ctxArg) {
    GameObj* child = reinterpret_cast<GameObj*>(static_cast<intptr_t>(childArg));
    NotifyContext* ctx = reinterpret_cast<NotifyContext*>(static_cast<intptr_t>(ctxArg));
    int ok = GameObj::Virtual49(childArg, added, ctxArg);
    const uint32_t family = child->GetFamily();
    int applied;
    if (familyIn(family, kFacilityRange)) {
        applied = FUN_0050ad80(this, ctx);
        ok = applied && ok;
    }
    if (family == 0x2c) {
        applied = FUN_0050b0b0(this, ctx);
        ok = applied && ok;
    }
    if (familyIn(family, kFacilityKindCRange) && added != 0) {
        applied = FUN_0050bbb0(this, ctx);
        ok = applied && ok;
    }
    if (familyIn(family, kFleetRange)) {
        applied = FUN_0050bc60(this, ctx);
        ok = applied && ok;
        applied = FUN_0050be00(this, ctx);
        ok = applied && ok;
        applied = FUN_0050c0b0(this, ctx);
        ok = applied && ok;
    }
    return ok;
}

int System::Virtual50(int childArg, int added, int ctxArg) {
    GameObj* child = reinterpret_cast<GameObj*>(static_cast<intptr_t>(childArg));
    NotifyContext* ctx = reinterpret_cast<NotifyContext*>(static_cast<intptr_t>(ctxArg));
    int ok = GameObj::Virtual50(childArg, added, ctxArg);
    const uint32_t family = child->GetFamily();
    int applied;
    if (familyIn(family, kFacilityRange)) {
        applied = FUN_0050b760(this, ctx);
        ok = applied && ok;
    }
    if (familyIn(family, FamilyRange(0x20, 0x22))) {
        applied = FUN_0050c9f0(this, FUN_0055a0f0(added), 1, ctx);
        ok = applied && ok;
    }
    if (family == 0x22) {
        applied = FUN_0050b310(this, ctx);
        ok = applied && ok;
    }
    if (familyIn(family, FamilyRange(0x2c, 0x30)) && added != 0) {
        applied = FUN_0050c560(this, child, ctx);
        ok = applied && ok;
    }
    if (family == 0x28) {
        applied = FUN_0050bb70(this, ctx);
        ok = applied && ok;
        applied = FUN_0050c180(this, ctx);
        ok = applied && ok;
    }
    if (family == 0x29) {
        applied = FUN_0050c1b0(this, ctx);
        ok = applied && ok;
    }
    if (ok) {
        if (family == 0x2a) ok = FUN_0050c1e0(this, ctx) != 0;
        if (ok && familyIn(family, kFleetRange)) {
            const int first = FUN_0050ac80(this, ctx);
            applied = FUN_0050bc60(this, ctx);
            ok = applied && first;
            applied = FUN_0050be00(this, ctx);
            ok = applied && ok;
            if (added != 0) {
                applied = SetControlKindBattleReady(child->ControlSide(), ctx);
                ok = applied && ok;
            }
            applied = FUN_0050c0b0(this, ctx);
            ok = applied && ok;
            applied = FUN_0050c6c0(this, child, added, ctx);
            ok = applied && ok;
            applied = SetBlockadeAndBattlePendingManagementRequired(1, ctx);
            ok = applied && ok;
        }
    }
    if (familyIn(family, kFighterRange)) {
        if (added != 0) {
            applied = SetControlKindBattleReady(child->ControlSide(), ctx);
            ok = applied && ok;
        }
        applied = FUN_0050c6c0(this, child, added, ctx);
        ok = applied && ok;
        applied = SetBlockadeAndBattlePendingManagementRequired(1, ctx);
        ok = applied && ok;
    }
    if (familyIn(family, kTroopRange)) {
        applied = FUN_0050b610(this, ctx);
        ok = applied && ok;
        applied = FUN_0050ba90(this, ctx);
        ok = applied && ok;
        applied = FUN_0050b500(this, ctx);
        ok = applied && ok;
        if (added != 0) {
            applied = FUN_0050c540(this, child, ctx);
            ok = applied && ok;
        }
    }
    return ok;
}

int System::Virtual52(int childArg, int added, int ctxArg) {
    GameObj* child = reinterpret_cast<GameObj*>(static_cast<intptr_t>(childArg));
    int ok = GameObj::Virtual52(childArg, added, ctxArg);
    if (added == 0 && (child->m_stateFlags & kDestroyedState)) {
        if (familyIn(child->GetFamily(), kUnitRange)) {
            int strength = callUnitSlot(child, kUnitSlotA);
            strength -= callUnitSlot(child, kUnitSlotB);
            const int reason = static_cast<int>(child->m_packed & 0xff);
            const uint32_t side = child->ControlSide();
            int applied;
            if (reason == kDestroyedByCapture) {
                applied = FUN_0050cee0(this, FUN_00509ae0(this, side) + strength, side);
            } else if (reason == kDestroyedByRetreat) {
                applied = FUN_0050ce20(this, FUN_005099c0(this, side) + strength, side);
            } else if (reason == kDestroyedByLoss) {
                applied = FUN_0050ce50(this, FUN_005099f0(this, side) + strength, side);
            } else {
                return ok;
            }
            return applied && ok ? 1 : 0;
        }
    }
    return ok;
}

int System::Virtual53(int childArg, int added, int ctxArg) {
    GameObj* child = reinterpret_cast<GameObj*>(static_cast<intptr_t>(childArg));
    NotifyContext* ctx = reinterpret_cast<NotifyContext*>(static_cast<intptr_t>(ctxArg));
    int ok = GameObj::Virtual53(childArg, added, ctxArg);
    const uint32_t family = child->GetFamily();
    int applied;
    if (familyIn(family, kCapitalShipRange)) {
        applied = FUN_0050b230(this, ctx);
        const int first = applied && ok;
        applied = FUN_0050b310(this, ctx);
        ok = applied && first;
        if (added != 0) {
            applied = FUN_0050c580(this, child, ctx);
            ok = applied && ok;
        }
    }
    if (familyIn(family, kTroopRange)) {
        applied = FUN_0050b230(this, ctx);
        ok = applied && ok;
    }
    if (familyIn(family, kFleetRange)) {
        applied = FUN_0050b230(this, ctx);
        const int first = applied && ok;
        applied = FUN_0050b310(this, ctx);
        ok = applied && first;
        if (added != 0) {
            applied = FUN_0050c350(this, ctx);
            ok = applied && ok;
        }
    }
    return ok;
}
