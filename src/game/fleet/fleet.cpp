#include "fleet/fleet.h"

#include <cstdint>

#include "core/vptr_store.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_family.h"
#include "orders/order_checks.h"

// Shared helpers of the movement/validity checks (0x553350, 0x553410; cdecl).

extern EventKindSet g_fleetEventKinds;  // 0x6b2b10

namespace {

constexpr uint32_t kCreated = 0x2;
constexpr uint32_t kCompleted = 0x4;
constexpr uint32_t kEnroute = 0x10;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kExisting = 0x40;
constexpr uint32_t kDamaged = 0x200;
constexpr uint32_t kFlag01 = 0x1;
constexpr uint32_t kBlockade = 0x20;
constexpr uint32_t kFlag02Or04 = 0x6;
constexpr uint32_t kControlSideMask = 0xc0;

constexpr uint32_t kUsableFilter = 1;
constexpr uint32_t kExistingFilter = 3;
constexpr uint32_t kAlwaysFilter = 4;

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNoSuchOrder = 1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusShipNotCompleted = 0x10;
constexpr int kStatusShipEnroute = 0x11;
constexpr int kStatusNoShips = 1;
constexpr int kFleetStatusCategory = 8;
constexpr int kFleetStatusNoShips = 1;
constexpr int kFleetStatusNotBlockading = 7;
constexpr int kDestroyedReasonNoShips = 0x14;
constexpr int kDestroyedReasonStranded = 5;

constexpr uint32_t kAllianceView = 1;
constexpr uint32_t kEmpireView = 2;

void setStatus(StatusCode* out, int code) { *out = StatusCode(kStatusCategory, code); }
void setStatus(StatusCode* out, int category, int code) { *out = StatusCode(category, code); }
bool isOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(kStatusOk); }

bool isShipFamily(uint32_t family) { return family >= 0x14 && family < 0x1c; }
bool isFleetFamily(uint32_t family) { return family >= family::kFleet && family < family::kTroopRegiment; }

GameObj* currentObject(const ChildIterator& iterator) { return static_cast<GameObj*>(iterator.Current()); }

bool isReady(const GameObj* ship) { return (ship->m_stateFlags & kCompleted) && !(ship->m_stateFlags & kEnrouteActive); }

int hasReadyShip(Fleet* fleet) {
    int ready = 0;
    CapShipIterator ships(fleet, kExistingFilter, fleet->ControlSide());
    ships.Begin();
    while (ships.Current() && ready == 0) {
        ready = isReady(currentObject(ships)) ? 1 : 0;
        ships.Advance();
    }
    return ready;
}

template <typename T>
T* asPointer(intptr_t value) {
    return reinterpret_cast<T*>(static_cast<intptr_t>(value));
}

}  // namespace

// ---- construction -----------------------------------------------------------------------------------------

Fleet::Fleet() : GameObj(), m_fleetFlags(0) {}

Fleet::~Fleet() { REB_KEEP_VPTR_STORE(); }

Fleet* CreateFleet() { return new Fleet(); }

GameObj* Fleet::CreateViewObject() { return new Fleet(); }

// ---- serialisation, checksum, validation ------------------------------------------------------------------

int Fleet::Read(istream* in) { return ReadImpl(in); }

int Fleet::Write(ostream* out) { return WriteImpl(out); }

int Fleet::Checksum(Crc32* crc) { return ChecksumImpl(crc); }

int Fleet::Validate() { return ValidateImpl(); }

int Fleet::ReadImpl(istream* in) {
    const int result = GameObj::Read(in);
    if (result) ReadValue32(in, &m_fleetFlags);
    return result;
}

int Fleet::WriteImpl(ostream* out) {
    const int result = GameObj::Write(out);
    if (result) WriteValue32(out, &m_fleetFlags);
    return result;
}

int Fleet::ChecksumImpl(Crc32* crc) {
    const int result = GameObj::Checksum(crc);
    if (result) crc->AddInt(m_fleetFlags);
    return result;
}

int Fleet::ValidateImpl() {
    uint32_t consistent = GameObj::Validate();
    GameObjIterator children(this, kAlwaysFilter);
    children.Begin();
    while (children.Current()) {
        if (consistent == 0) return 0;
        ObjectKey key;
        children.Current()->GetKey(&key);
        consistent = 0;
        if (isShipFamily(key.m_value >> 24)) {
            consistent = ((children.Current()->m_flags ^ m_flags) & kControlSideMask) == 0;
        }
        children.Advance();
    }
    int result = 0;
    if (consistent != 0) {
        int total = 0;
        if (SumShipCounts(&total, 0, 0) != 0 && total < 2) result = 1;
    }
    return result;
}

int Fleet::CollectDependencies(void* context, int value, ObjectKeyValueSet* out) {
    const uint32_t targetFamily = static_cast<uint32_t>(static_cast<const ActivityRequest*>(context)->m_target.m_value) >> 24;
    if (isFleetFamily(targetFamily)) {
        GameObjIterator children(this, kExistingFilter);
        return AddChildKeys(&children, out, 1, ControlSide());
    }
    return GameObj::CollectDependencies(context, value, out);
}

// ---- creation of views ------------------------------------------------------------------------------------

int Fleet::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsImpl(controlKind, ctx); }

int Fleet::CreateViewsImpl(int controlKind, NotifyContext* ctx) {
    int result = GameObj::CreateViews(controlKind, ctx);
    if (result) result = SetFlag01(1, ctx);
    return result;
}

int Fleet::ApplyAll(NotifyContext* ctx) {
    int result = IsSideView();
    if (result) result = ApplyAllImpl(ctx);
    return result;
}

int Fleet::ApplyAllImpl(NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) {
        GameObj::ApplyAll(ctx);
        if (ctx->m_flag04 == 1) {
            ApplyFlag01(ctx);
            ApplyFlag02(ctx);
            ApplyFlag04(ctx);
            ApplyBattle(ctx);
            ApplyBlockade(ctx);
            ApplyBombard(ctx);
            ApplyAssault(ctx);
        }
    }
    return result;
}

// ---- queries over the ships -------------------------------------------------------------------------------

int Fleet::SumUnitStat119() {
    int total = 0;
    CapShipIterator ships(this, kUsableFilter);
    ships.Begin();
    while (ships.Current()) {
        total += static_cast<CombatUnit*>(ships.Current())->GetDefenseLeft();
        FightSquadIterator squadrons(ships.Current(), kUsableFilter);
        squadrons.Begin();
        while (squadrons.Current()) {
            total += static_cast<CombatUnit*>(squadrons.Current())->GetDefenseLeft();
            squadrons.Advance();
        }
        ships.Advance();
    }
    return total;
}

int Fleet::FindShipByNumber(int number, GameObjNotifier** out) {
    int result = 1;
    *out = nullptr;
    CapShipIterator ships(this, kExistingFilter, ControlSide());
    ships.Begin();
    do {
        if (!ships.Current() || *out != nullptr) break;
        result = static_cast<CapShip*>(ships.Current())->FindCharacterById(number, out);
        ships.Advance();
    } while (result != 0);
    return result;
}

int Fleet::SumShipCounts(int* out, int unused1, int unused2) {
    int result = 1;
    *out = 0;
    int count = 0;
    CapShipIterator ships(this, kExistingFilter);
    ships.Begin();
    do {
        if (!ships.Current()) break;
        result = static_cast<CapShip*>(ships.Current())->CountCharacters(&count, 0, 0);
        if (result) *out += count;
        ships.Advance();
    } while (result != 0);
    return result;
}

int Fleet::GetTransitSpeed(int arg) {
    int speed = 0;
    bool first = true;
    CapShipIterator ships(this, kExistingFilter);
    ships.Begin();
    while (ships.Current()) {
        if (isReady(currentObject(ships))) {
            if (first) {
                first = false;
                speed = currentObject(ships)->GetTransitSpeed(arg);
            }
            if (speed != 0) {
                const bool notFaster = currentObject(ships)->GetTransitSpeed(arg) != 0 &&
                                       currentObject(ships)->GetTransitSpeed(arg) <= speed;
                if (!notFaster) speed = currentObject(ships)->GetTransitSpeed(arg);
            }
        }
        ships.Advance();
    }
    return speed;
}

int Fleet::IsConstructed() { return hasReadyShip(this); }

int Fleet::CanBeControlledBy(int arg) { return (m_stateFlags & kCreated) ? 0 : 1; }

int Fleet::UpdateUsable(NotifyContext* ctx) {
    const int ready = hasReadyShip(this);
    const uint32_t flags = m_stateFlags;
    const bool usable = (flags & kExisting) && (flags & kCompleted) && !(flags & kEnroute) && ready;
    return SetUsable(usable ? 1 : 0, ctx) != 0;
}

int Fleet::UpdateDamaged(NotifyContext* ctx) {
    uint32_t damaged = 0;
    CapShipIterator ships(this, kExistingFilter);
    ships.Begin();
    while (ships.Current() && damaged == 0) {
        damaged = currentObject(ships)->m_stateFlags;
        ships.Advance();
        damaged = (damaged & kDamaged) ? 1 : 0;
    }
    return SetDamaged(damaged, ctx) != 0;
}

int Fleet::RefreshFromShips(NotifyContext* ctx) {
    bool ok = true;
    CapShipIterator ships(this, kExistingFilter, ControlSide());
    const int hasShips = ships.HasCurrent();
    if (!hasShips) {
        if (m_fleetFlags & kFlag01) ok = ChangeDestroyedReason(kDestroyedReasonNoShips, ctx) != 0;
    } else {
        const int flagged = SetFlag01(1, ctx);
        const int created = SetCreated(1, ctx);
        ok = created != 0 && flagged != 0;
        const int completed = SetCompleted(1, ctx);
        ok = completed != 0 && ok;
    }
    return ok;
}

// ---- event kinds and request checks -----------------------------------------------------------------------

int Fleet::GetEventKinds(int unused, EventKindSet* out) {
    const int result = InitEventKinds();
    if (result) *out = g_fleetEventKinds;
    return result;
}

int Fleet::CheckOrder(int a, int orderKind, StatusCode* out) {
    setStatus(out, kStatusNotSupported);
    const int result = InitEventKinds();
    if (result && g_fleetEventKinds.Contains(orderKind)) setStatus(out, kStatusOk);
    return result;
}

int Fleet::CheckFleetRequest(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (result) {
        if (isOk(out)) {
            StatusCode verdict(kFleetStatusCategory, kFleetStatusNoShips);
            CapShipIterator ships(this, kExistingFilter);
            ships.Begin();
            while (ships.Current()) {
                if (isOk(&verdict)) break;
                const uint32_t flags = currentObject(ships)->m_stateFlags;
                if (!(flags & kCompleted)) {
                    verdict = StatusCode(kStatusCategory, kStatusShipNotCompleted);
                } else if (!(currentObject(ships)->m_stateFlags & kEnroute)) {
                    verdict = StatusCode(kStatusCategory, kStatusOk);
                } else {
                    verdict = StatusCode(kStatusCategory, kStatusShipEnroute);
                }
                ships.Advance();
            }
            if (!isOk(&verdict)) *out = verdict;
        }
        return result;
    }
    setStatus(out, kStatusNoShips);
    return result;
}

int Fleet::CheckRequestImpl(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckFleetRequest(request, out);
    if (!result) setStatus(out, kStatusNoSuchOrder);
    return result;
}

int Fleet::CheckRenameImpl(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (!result) setStatus(out, kStatusNoSuchOrder);
    return result;
}

int Fleet::CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (!result) setStatus(out, kStatusNoSuchOrder);
    return result;
}

int Fleet::Virtual29(int a, int b, int c, intptr_t d, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int needCompleted = (m_fleetFlags & kFlag02Or04) ? 0 : 1;
    return CheckControlAndState(this, a, b, needCompleted, 1, out);
}

int Fleet::Virtual30(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) {
    setStatus(out, kStatusNotSupported);
    *o1 = 1;
    *o2 = 0;
    *o3 = 0;
    const bool capitalShips = !(range->m_first < family::kCapitalShip || family::kFighterSquadron < range->m_second);
    if (capitalShips) {
        *o2 = 1;
    } else {
        *o1 = 0;
    }
    return 1;
}

int Fleet::CheckAssaultImpl(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckFleetRequest(request, out);
    if (result && isOk(out) && !(m_fleetFlags & kBlockade)) {
        setStatus(out, kFleetStatusCategory, kFleetStatusNotBlockading);
    }
    if (!result) setStatus(out, kStatusNoSuchOrder);
    return result;
}

int Fleet::CheckAssault(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckAssaultImpl(request, out);
    if (!result) setStatus(out, kStatusNoSuchOrder);
    return result;
}

int Fleet::CheckBombardImpl(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckFleetRequest(request, out);
    if (!result) setStatus(out, kStatusNoSuchOrder);
    return result;
}

int Fleet::CheckBombard(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckBombardImpl(request, out);
    if (!result) setStatus(out, kStatusNoSuchOrder);
    return result;
}

int Fleet::StartAssault(intptr_t request, NotifyContext* ctx) {
    StatusCode status(kStatusCategory, kStatusOk);
    bool ok = CheckAssault(asPointer<const ActivityRequest>(request), &status) != 0;
    if (isOk(&status)) {
        const int started = SetAssault(1, ctx);
        ok = started != 0 && ok;
    }
    return ok;
}

int Fleet::StartBombard(intptr_t request, NotifyContext* ctx) {
    StatusCode status(kStatusCategory, kStatusOk);
    bool ok = CheckBombard(asPointer<const ActivityRequest>(request), &status) != 0;
    if (isOk(&status)) {
        const int started = SetBombard(1, ctx);
        ok = started != 0 && ok;
    }
    return ok;
}

// ---- events from other objects ----------------------------------------------------------------------------

int Fleet::Virtual48(intptr_t ctx) {
    const int shared = GameObj::Virtual55(ctx);
    const bool usable = UpdateUsable(asPointer<NotifyContext>(ctx)) != 0 && shared != 0;
    const int refreshed = RefreshFromShips(asPointer<NotifyContext>(ctx));
    return refreshed != 0 && usable ? 1 : 0;
}

int Fleet::NotifyUnitExisting(intptr_t other, int b, intptr_t ctx) {
    bool ok = GameObj::NotifyUnitExisting(other, b, ctx) != 0;
    if (isShipFamily(asPointer<GameObj>(other)->GetFamily())) {
        const int refreshed = RefreshFromShips(asPointer<NotifyContext>(ctx));
        ok = refreshed != 0 && ok;
    }
    return ok;
}

int Fleet::NotifyUnitUsable(intptr_t other, int b, intptr_t ctx) {
    bool ok = GameObj::NotifyUnitUsable(other, b, ctx) != 0;
    if (isShipFamily(asPointer<GameObj>(other)->GetFamily())) {
        const int updated = UpdateUsable(asPointer<NotifyContext>(ctx));
        ok = updated != 0 && ok;
    }
    return ok;
}

int Fleet::NotifyUnitDamaged(intptr_t other, int b, intptr_t ctx) {
    bool ok = GameObj::NotifyUnitDamaged(other, b, ctx) != 0;
    if (isShipFamily(asPointer<GameObj>(other)->GetFamily())) {
        const int updated = UpdateDamaged(asPointer<NotifyContext>(ctx));
        ok = updated != 0 && ok;
    }
    return ok;
}

int Fleet::Reparent(ObjectKey* key, NotifyContext* ctx) {
    bool ok = true;
    const uint32_t targetFamily = key->m_value >> 24;
    if (isFleetFamily(targetFamily)) {
        GameObjIterator children(this, kAlwaysFilter);
        children.Begin();
        while (children.Current()) {
            GameObj* child = currentObject(children);
            children.Advance();
            if ((child->m_stateFlags & kExisting) && (child->m_stateFlags & kCompleted)) {
                const int moved = child->GameObj::Reparent(key, ctx);
                ok = moved != 0 && ok;
            }
        }
        return ok;
    }
    const bool intoSystemOrSideRoot = (targetFamily >= family::kSystem && targetFamily < family::kSystem + 8) ||
                                      (targetFamily >= family::kSideRoot && targetFamily < family::kSide);
    if (!intoSystemOrSideRoot) return 0;
    return GameObj::Reparent(key, ctx) != 0;
}

int Fleet::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) {
    bool ok = GameObj::OnParentChanged(oldParent, newParent, ctx) != 0;
    const int cleared02 = SetFlag02(0, ctx);
    ok = cleared02 != 0 && ok;
    const int cleared04 = SetFlag04(0, ctx);
    return cleared04 != 0 && ok ? 1 : 0;
}

int Fleet::OnUsableChanged(int newValue, NotifyContext* ctx) { return GameObj::OnUsableChanged(newValue, ctx) != 0; }

int Fleet::OnDestroyedChanged(int newValue, NotifyContext* ctx) {
    bool ok = GameObj::OnDestroyedChanged(newValue, ctx) != 0;
    if (newValue != 0) {
        const int battleCleared = SetBattle(0, ctx);
        ok = battleCleared != 0 && ok;
        const int assaultCleared = SetAssault(0, ctx);
        ok = assaultCleared != 0 && ok;
        const int bombardCleared = SetBombard(0, ctx);
        if (bombardCleared != 0 && ok) return 1;
        ok = false;
    }
    return ok;
}

int Fleet::OnAutoroutingChanged(int newValue, NotifyContext* ctx) {
    int result = GameObj::OnAutoroutingChanged(newValue, ctx);
    CapShipIterator ships(this, kExistingFilter);
    ships.Begin();
    while (ships.Current()) {
        if (isReady(currentObject(ships))) {
            if (currentObject(ships)->GetTransitSpeed(0) == 0) {
                const int stranded = currentObject(ships)->ChangeDestroyedReason(kDestroyedReasonStranded, ctx);
                result = stranded != 0 && result != 0 ? 1 : 0;
            }
        }
        ships.Advance();
    }
    return result;
}

// ---- free functions ---------------------------------------------------------------------------------------

int CountFleets(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&family::kFleets, side, filter);
}

void DeleteObjectMemory(void* memory) { operator delete(memory); }
