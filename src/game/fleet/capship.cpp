#include "core/reb_abi.h"
#include "fleet/capship.h"

#include <new>

#include "core/utility_classes.h"
#include "core/vptr_store.h"
#include "data/game_math.h"
#include "data/probability_table.h"
#include "data/random.h"
#include "data/sd_records.h"
#include "events/event_post.h"
#include "fleet/capship_data.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_family.h"
#include "orders/order_checks.h"

extern "C" int REB_CDECL atexit(void(REB_CDECL* function)(void));
void DestroyCapShipEventKinds();

extern int g_capShipEventKindsReady;      // 0x6b2b20
extern EventKindSet g_capShipEventKinds;  // 0x6b2b28


namespace {
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNoSuchOrder = 1;
constexpr int kStatusCategoryCapacity = 0x14;
constexpr int kStatusNoFighterRoom = 1;
constexpr int kStatusNoTroopRoom = 2;
constexpr int kApplyEverything = 1;
constexpr int kMasterView = 0;
constexpr uint32_t kViewMask = 0x30;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kEnrouteActiveFlag = 0x20;
constexpr uint32_t kExistingFlag = 0x40;
constexpr uint32_t kLockedFlag = 0x4000;
constexpr uint32_t kHullEventId = 0x1c0;
constexpr int kEveryChild = 4;
constexpr int kExistingChildren = 3;
constexpr uint32_t kMaxRepairedDestroyedReason = 0x10;
constexpr int kHullRepairDivisor = 10;
constexpr int kCharacterIdOffset = 0x96;
constexpr int kCharacterShipDestroyedSlot = 186;
constexpr int kHitTypeNone = 1;
constexpr int kHitTypeShield = 2;
constexpr int kHitTypeWeapon = 3;
constexpr int kHitTypeTractorBeam = 4;
constexpr int kHitTypeSpeed = 5;
constexpr int kHitTypeHyperdrive = 6;
constexpr int kHitTypeDestroyed = 7;
constexpr int kHitTypeTable = 5;
constexpr int kKindCapShip203 = 0x203;
constexpr int kKindCapShip270 = 0x270;

const FamilyRange kCharacterFamilies(0x30, 0x3c);

bool inFamilies(uint32_t family, uint32_t first, uint32_t last) { return family >= first && family < last; }

bool isUnitFamily(uint32_t family) {
    return inFamilies(family, 0x30, 0x40) || inFamilies(family, 0x1c, 0x20) || inFamilies(family, 0x10, 0x14);
}

bool rangeWithin(const FamilyRange* range, uint32_t first, uint32_t last) {
    return range->m_first >= first && range->m_second <= last;
}

GameObj* asObject(GameObjNotifier* notifier) { return static_cast<GameObj*>(notifier); }

int16_t characterId(GameObjNotifier* character) {
    return *reinterpret_cast<const int16_t*>(reinterpret_cast<const char*>(character) + kCharacterIdOffset);
}

// The Character class belongs to another module; slot 186 of its vtable takes the notify context.
int notifyCharacterOfShipLoss(GameObjNotifier* character, NotifyContext* ctx) {
    using Slot = int(REB_THISCALL*)(GameObjNotifier*, NotifyContext*);
    void** const vtable = *reinterpret_cast<void***>(character);
    return reinterpret_cast<Slot>(vtable[kCharacterShipDestroyedSlot])(character, ctx);
}

int setStatusAndCheckRequest(GameObj* object, const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusOk);
    const int result = object->CheckActivityRequest(request, out);
    if (!result) *out = StatusCode(kStatusCategory, kStatusNoSuchOrder);
    return result;
}

bool wantsNotification(CapShip* view, NotifyContext* ctx) {
    return CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(view->ViewSide()));
}

using ApplyFn = int (CapShip::*)(NotifyContext*);
using ChangedFn = int (CapShip::*)(int, int, NotifyContext*);

int setCriticalHit(CapShip* self, int shift, uint32_t value, int maxValue, const char* message, ApplyFn apply,
                   ChangedFn changed, NotifyContext* ctx) {
    int result = 0;
    if (self->IsMasterView()) result = IsInRange(value, 0, maxValue, message);
    if (result) {
        const uint32_t oldBits = self->m_criticalHits;
        if (self->GetCriticalHit(shift) != value) {
            const uint32_t mask = 0xfu << shift;
            self->m_criticalHits = (oldBits & ~mask) | ((value & 0xf) << shift);
            (static_cast<CapShip*>(self->GetViewObject(1))->*apply)(ctx);
            (static_cast<CapShip*>(self->GetViewObject(2))->*apply)(ctx);
            (self->*changed)((oldBits >> shift) & 0xf, self->GetCriticalHit(shift), ctx);
        }
    }
    return result;
}

int applyCriticalHit(CapShip* self, int shift, NotifyContext* ctx, ChangedFn notified) {
    int result = self->IsSideView();
    if (result) {
        CapShip* const master = static_cast<CapShip*>(self->GetViewObject(kMasterView));
        if (self->MatchesSideMask(ctx->m_flag08)) {
            const uint32_t mask = 0xfu << shift;
            const uint32_t oldBits = self->m_criticalHits;
            if (((master->m_criticalHits ^ oldBits) & mask) != 0) {
                self->m_criticalHits = (oldBits & ~mask) | (master->m_criticalHits & mask);
                if (wantsNotification(self, ctx)) {
                    result = (self->*notified)((oldBits >> shift) & 0xf, self->GetCriticalHit(shift), ctx);
                }
            }
        }
    }
    return result;
}

int notifyCriticalHit(CapShip* self, uint32_t eventId, const char* tag, const char* property, int oldValue,
                      int newValue, NotifyContext* ctx) {
    const int result = self->IsSideView();
    if (result) {
        self->LogInt(tag, property, newValue, oldValue, 0, ctx);
        PostPayloadInts(eventId, self, oldValue, newValue, ctx);
    }
    return result;
}
}  // namespace

CapShip::CapShip() {
    m_hullValueDamage = 0;
    m_criticalHits = 0;
}

CapShip::~CapShip() { REB_KEEP_VPTR_STORE(); }

int CapShip::GetTransitSpeed(int arg) {
    int speed = 0;
    if (m_stateFlags & kExistingFlag) {
        const int primaryLoss = GetDamageField6c();
        speed = GetField6c() - primaryLoss;
        if (speed == 0) {
            const int backupLoss = GetDamageField70();
            speed = GetField70() - backupLoss;
        }
        if (speed == 0 && ((m_packed >> 8) & 0xff) == 0) speed = GameObj::GetTransitSpeed(arg);
    }
    return speed;
}

int CapShip::GetEventKinds(int unused, EventKindSet* out) {
    const int ready = EnsureEventKinds();
    if (ready) *out = g_capShipEventKinds;
    return ready;
}

int CapShip::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int ready = EnsureEventKinds();
    if (ready && g_capShipEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return ready;
}

int CapShip::CheckRenameImpl(const ActivityRequest* request, StatusCode* out) {
    return setStatusAndCheckRequest(this, request, out);
}

int CapShip::CheckRequestImpl2(const ActivityRequest* request, StatusCode* out) {
    return setStatusAndCheckRequest(this, request, out);
}

int CapShip::Virtual29(int a, int b, int c, int d, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    return CheckControlAndState(this, a, b, 1, 1, out);
}

int CapShip::Virtual30(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) {
    *o1 = 1;
    *o2 = 0;
    *o3 = 0;
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    if (rangeWithin(range, 0x1c, 0x20)) {
        *o2 = 0;
        ChildIterator squadrons(this, &family::kFighterSquadrons, b);
        *o3 = GetFighterCapacity() - squadrons.Count();
        *out = StatusCode(kStatusCategoryCapacity, kStatusNoFighterRoom);
    } else if (rangeWithin(range, 0x10, 0x14)) {
        *o2 = 0;
        ChildIterator regiments(this, &family::kTroopRegiments, b);
        *o3 = GetTroopCapacity() - regiments.Count();
        *out = StatusCode(kStatusCategoryCapacity, kStatusNoTroopRoom);
    } else if (rangeWithin(range, 0x30, 0x40)) {
        *o2 = 1;
    } else {
        *o1 = 0;
    }
    return 1;
}

GameObjData* CapShip::CreateData() { return new CapShipData(); }

int CapShip::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) {
    return CombatUnit::OnParentChanged(oldParent, newParent, ctx) != 0;
}

int CapShip::OnUsableChanged(int newValue, NotifyContext* ctx) { return CombatUnit::OnUsableChanged(newValue, ctx) != 0; }

int CapShip::OnDestroyedChanged(int newValue, NotifyContext* ctx) {
    bool ok = CombatUnit::OnDestroyedChanged(newValue, ctx) != 0;
    const uint32_t reason = m_packed & 0xff;
    if (reason != 0 && reason <= kMaxRepairedDestroyedReason) {
        ChildIterator characters(this, &kCharacterFamilies, kExistingChildren);
        for (characters.Begin(); characters.Current(); characters.Advance()) {
            if ((asObject(characters.Current())->m_stateFlags & kEnrouteActiveFlag) == 0) {
                ok = notifyCharacterOfShipLoss(characters.Current(), ctx) != 0 && ok;
            }
        }
    }
    return ok;
}

int CapShip::GetRepairLevel() { return (m_combatFlags & kFastRepairFlag) != 0 ? g_gnprtb0804 : g_gnprtb0805; }

int CapShip::GetDefenseLeft() {
    const int lost = GetBombardmentModifierLoss();
    return GetBombardmentModifier() - lost;
}

int CapShip::LostShare(int base, int criticalHits) {
    int remaining = base;
    if (GetMaxCriticalHits() != 0 && GetHull() != 0) {
        const int maxHits = GetMaxCriticalHits();
        const int afterHits = ScaleRatio(base, maxHits - criticalHits, maxHits);
        const int hullLeft = GetHull() - GetHullValueDamage();
        remaining = ScaleRatio(afterHits, hullLeft, GetHull());
    }
    return base - remaining;
}

uint32_t CapShip::GetDamageField60() { return LostShare(GetField60(), GetCriticalHit(kShieldRechargeShift)); }

uint32_t CapShip::GetDamageField64() { return LostShare(GetField64(), GetCriticalHit(kSpeedShift)); }

uint32_t CapShip::GetDamageField6c() {
    int lost = 0;
    if (HasPrimaryHyperdrive()) {
        lost = ScaleRatio(GetField6c(), GetCriticalHit(kPrimaryHyperdriveShift), HasPrimaryHyperdrive());
    }
    return lost;
}

uint32_t CapShip::GetDamageField70() {
    int lost = 0;
    if (HasBackupHyperdrive()) {
        lost = ScaleRatio(GetField70(), GetCriticalHit(kBackupHyperdriveShift), HasBackupHyperdrive());
    }
    return lost;
}

uint32_t CapShip::GetDamageTotal() { return LostShare(GetGridTotal(), GetCriticalHit(kWeaponRechargeShift)); }

uint32_t CapShip::GetDamageColumnTotal(int column) {
    return LostShare(GetGridColumnTotal(column), GetCriticalHit(kWeaponRechargeShift));
}

uint32_t CapShip::GetDamageGridValue(int row, int column) {
    return LostShare(GetGridValue(row, column), GetCriticalHit(kWeaponRechargeShift));
}

int CapShip::GetTractorBeamPowerLoss() {
    return LostShare(GetTractorBeamPower(), GetCriticalHit(kTractorBeamShift));
}

int CapShip::GetBombardmentModifierLoss() {
    int lost = ScaleRatio(GetBombardmentModifier(), GetHullValueDamage(), GetHull());
    if ((m_flags & kViewMask) == 0 && GetDataWord20() != 0) lost = GetBombardmentModifier();
    return lost;
}

int CapShip::GetWeaponRechargeRateLoss() {
    int lost = 0;
    if (GetMaxCriticalHits() != 0) {
        lost = ScaleRatio(GetWeaponRechargeRate(), GetCriticalHit(kWeaponRechargeShift), GetMaxCriticalHits());
    }
    return lost;
}

int CapShip::GetShieldRechargeRateLoss() {
    int lost = 0;
    if (GetMaxCriticalHits() != 0) {
        lost = ScaleRatio(GetShieldRechargeRate(), GetCriticalHit(kShieldRechargeShift), GetMaxCriticalHits());
    }
    return lost;
}

int CapShip::CountFighterSquadrons() {
    ChildIterator squadrons(this, &family::kFighterSquadrons, kExistingChildren);
    return squadrons.Count();
}

int CapShip::CountTroopRegiments() {
    ChildIterator regiments(this, &family::kTroopRegiments, kExistingChildren);
    return regiments.Count();
}

int CapShip::GetHull() { return m_record ? static_cast<CapitalShipSD*>(m_record)->m_hull : 0; }

int CapShip::GetTractorBeamPower() { return m_record ? static_cast<CapitalShipSD*>(m_record)->m_tractorBeamPower : 0; }

int CapShip::GetTractorBeamRange() { return m_record ? static_cast<CapitalShipSD*>(m_record)->m_tractorBeamRange : 0; }

int CapShip::GetGravityWellProjector() {
    return m_record ? static_cast<CapitalShipSD*>(m_record)->m_gravityWellProjector : 0;
}

int CapShip::GetInterdictionStrength() {
    return m_record ? static_cast<CapitalShipSD*>(m_record)->m_interdictionStrength : 0;
}

int CapShip::GetBombardmentModifier() {
    return m_record ? static_cast<CapitalShipSD*>(m_record)->m_bombardmentModifier : 0;
}

int CapShip::GetDamageControl() { return m_record ? static_cast<CapitalShipSD*>(m_record)->m_damageControl : 0; }

int CapShip::GetWeaponRechargeRate() {
    return m_record ? static_cast<CapitalShipSD*>(m_record)->m_weaponRechargeRate : 0;
}

int CapShip::GetShieldRechargeRate() {
    return m_record ? static_cast<CapitalShipSD*>(m_record)->m_shieldRechargeRate : 0;
}

int CapShip::GetFighterCapacity() { return m_record ? static_cast<CapitalShipSD*>(m_record)->m_fighterCapacity : 0; }

int CapShip::GetTroopCapacity() { return m_record ? static_cast<CapitalShipSD*>(m_record)->m_troopCapacity : 0; }

int CapShip::UpdateDamaged(NotifyContext* ctx) {
    const bool undamaged = GetHullValueDamage() == 0 && GetTotalCriticalHits() == 0;
    return SetDamaged(undamaged ? 0 : 1, ctx);
}

int CapShip::OnHullValueDamageChanged(int oldValue, int newValue, NotifyContext* ctx) {
    const int hull = GetHull();
    const int updated = UpdateDamaged(ctx);
    return updated != 0 && newValue <= hull;
}

int CapShip::OnShieldRechargeRateCHChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return UpdateDamaged(ctx) != 0;
}

int CapShip::OnWeaponRechargeRateCHChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return UpdateDamaged(ctx) != 0;
}

int CapShip::OnTractorBeamPowerCHChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return UpdateDamaged(ctx) != 0;
}

int CapShip::OnSpeedCHChanged(int oldValue, int newValue, NotifyContext* ctx) { return UpdateDamaged(ctx) != 0; }

int CapShip::OnPrimaryHyperdriveCHChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return UpdateDamaged(ctx) != 0;
}

int CapShip::OnBackupHyperdriveCHChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return UpdateDamaged(ctx) != 0;
}

int CapShip::OnHullValueDamageNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) {
        LogInt("CapShipHullValueDamageNotif", "HullValueDamage", newValue, oldValue, 0, ctx);
        PostPayloadInts(kHullEventId, this, oldValue, newValue, ctx);
    }
    return result;
}

int CapShip::OnShieldRechargeRateCHNotified(int oldValue, int newValue, NotifyContext* ctx) {
    return notifyCriticalHit(this, 0x1c1, "CapShipShieldRechargeRateCHAllocatedNotif", "ShieldRechargeRateCHAllocated",
                             oldValue, newValue, ctx);
}

int CapShip::OnWeaponRechargeRateCHNotified(int oldValue, int newValue, NotifyContext* ctx) {
    return notifyCriticalHit(this, 0x1c2, "CapShipWeaponRechargeRateCHAllocatedNotif", "WeaponRechargeRateCHAllocated",
                             oldValue, newValue, ctx);
}

int CapShip::OnTractorBeamPowerCHNotified(int oldValue, int newValue, NotifyContext* ctx) {
    return notifyCriticalHit(this, 0x1c3, "CapShipTractorBeamPowerCHAllocatedNotif", "TractorBeamPowerCHAllocated",
                             oldValue, newValue, ctx);
}

int CapShip::OnSpeedCHNotified(int oldValue, int newValue, NotifyContext* ctx) {
    return notifyCriticalHit(this, 0x1c4, "CapShipSpeedCHAllocatedNotif", "SpeedCHAllocated", oldValue, newValue, ctx);
}

int CapShip::OnPrimaryHyperdriveCHNotified(int oldValue, int newValue, NotifyContext* ctx) {
    return notifyCriticalHit(this, 0x1c5, "CapShipPrimaryHyperdriveModifierCHAllocatedNotif",
                             "PrimaryHyperdriveModifierCHAllocated", oldValue, newValue, ctx);
}

int CapShip::OnBackupHyperdriveCHNotified(int oldValue, int newValue, NotifyContext* ctx) {
    return notifyCriticalHit(this, 0x1c6, "CapShipBackupHyperdriveModifierCHAllocatedNotif",
                             "BackupHyperdriveModifierCHAllocated", oldValue, newValue, ctx);
}

int CapShip::GetMaxCriticalHits() { return g_gnprtb0800; }

int CapShip::HasPrimaryHyperdrive() { return GetField6c() != 0; }

int CapShip::HasBackupHyperdrive() { return GetField70() != 0; }

int CapShip::GetTotalCriticalHits() {
    return GetCriticalHit(kTractorBeamShift) + GetCriticalHit(kSpeedShift) + GetCriticalHit(kWeaponRechargeShift) +
           GetCriticalHit(kPrimaryHyperdriveShift) + GetCriticalHit(kBackupHyperdriveShift) +
           GetCriticalHit(kShieldRechargeShift);
}

int CapShip::GetDataWord20() { return m_data ? static_cast<CapShipData*>(m_data)->m_word20 : 0; }

void CapShip::SetDataWord20(int value) {
    if (m_data) static_cast<CapShipData*>(m_data)->m_word20 = value;
}

void CapShip::SetDataWord24(int value) {
    if (m_data) static_cast<CapShipData*>(m_data)->m_word24 = value;
}

int CapShip::FindCharacterById(int id, GameObjNotifier** out) {
    *out = nullptr;
    ChildIterator characters(this, ControlSide(), &kCharacterFamilies, kExistingChildren);
    for (characters.Begin(); characters.Current() && *out == nullptr; characters.Advance()) {
        if (characterId(characters.Current()) == id) *out = characters.Current();
    }
    return 1;
}

int CapShip::CountCharacters(int* out, int unused1, int unused2) {
    *out = 0;
    ChildIterator characters(this, &kCharacterFamilies, kExistingChildren);
    for (characters.Begin(); characters.Current(); characters.Advance()) {
    }
    return 1;
}

int CapShip::SetHullValueDamage(int value, NotifyContext* ctx) {
    int result = IsMasterView();
    if (result) result = IsInRange(value, 0, GetHull(), "Invalid HullValueDamage value!");
    if (result && m_hullValueDamage != value) {
        const int oldValue = m_hullValueDamage;
        m_hullValueDamage = value;
        static_cast<CapShip*>(GetViewObject(1))->ApplyHullValueDamage(ctx);
        static_cast<CapShip*>(GetViewObject(2))->ApplyHullValueDamage(ctx);
        OnHullValueDamageChanged(oldValue, m_hullValueDamage, ctx);
    }
    return result;
}

int CapShip::SetShieldRechargeRateCH(uint32_t value, NotifyContext* ctx) {
    return setCriticalHit(this, kShieldRechargeShift, value, GetMaxCriticalHits(),
                          "Invalid ShieldRechargeRateCHAllocated value!", &CapShip::ApplyShieldRechargeRateCH,
                          &CapShip::OnShieldRechargeRateCHChanged, ctx);
}

int CapShip::SetWeaponRechargeRateCH(uint32_t value, NotifyContext* ctx) {
    return setCriticalHit(this, kWeaponRechargeShift, value, GetMaxCriticalHits(),
                          "Invalid WeaponRechargeRateCHAllocated value!", &CapShip::ApplyWeaponRechargeRateCH,
                          &CapShip::OnWeaponRechargeRateCHChanged, ctx);
}

int CapShip::SetTractorBeamPowerCH(uint32_t value, NotifyContext* ctx) {
    return setCriticalHit(this, kTractorBeamShift, value, GetMaxCriticalHits(),
                          "Invalid TractorBeamPowerCHAllocated value!", &CapShip::ApplyTractorBeamPowerCH,
                          &CapShip::OnTractorBeamPowerCHChanged, ctx);
}

int CapShip::SetSpeedCH(uint32_t value, NotifyContext* ctx) {
    return setCriticalHit(this, kSpeedShift, value, GetMaxCriticalHits(), "Invalid SpeedCHAllocated value!",
                          &CapShip::ApplySpeedCH, &CapShip::OnSpeedCHChanged, ctx);
}

int CapShip::SetPrimaryHyperdriveCH(uint32_t value, NotifyContext* ctx) {
    return setCriticalHit(this, kPrimaryHyperdriveShift, value, HasPrimaryHyperdrive(),
                          "Invalid PrimaryHyperdriveModifierCHAllocated value!", &CapShip::ApplyPrimaryHyperdriveCH,
                          &CapShip::OnPrimaryHyperdriveCHChanged, ctx);
}

int CapShip::SetBackupHyperdriveCH(uint32_t value, NotifyContext* ctx) {
    return setCriticalHit(this, kBackupHyperdriveShift, value, HasBackupHyperdrive(),
                          "Invalid BackupHyperdriveModifierCHAllocated value!", &CapShip::ApplyBackupHyperdriveCH,
                          &CapShip::OnBackupHyperdriveCHChanged, ctx);
}

int CapShip::DamageHull(int amount, int destroyedReason, NotifyContext* ctx) {
    int result = 0;
    if (amount >= 0) {
        int damage = m_hullValueDamage + amount;
        if (damage > GetHull()) damage = GetHull();
        result = SetHullValueDamage(damage, ctx) != 0;
    }
    if (result) {
        const int hullDamage = GetHullValueDamage();
        if (GetHull() == hullDamage) result = ChangeDestroyedReason(destroyedReason, ctx);
    }
    return result;
}

int CapShip::RepairHullDamage(int amount, NotifyContext* ctx) {
    int result = 0;
    if (amount >= 0) {
        int damage = m_hullValueDamage - amount;
        if (damage < 0) damage = 0;
        result = SetHullValueDamage(damage, ctx) != 0;
    }
    return result;
}

int CapShip::ApplyHullValueDamage(NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        CapShip* const master = static_cast<CapShip*>(GetViewObject(kMasterView));
        if (MatchesSideMask(ctx->m_flag08)) {
            const int oldValue = m_hullValueDamage;
            if (oldValue != master->m_hullValueDamage) {
                m_hullValueDamage = master->m_hullValueDamage;
                if (wantsNotification(this, ctx)) result = OnHullValueDamageNotified(oldValue, m_hullValueDamage, ctx);
            }
        }
    }
    return result;
}

int CapShip::ApplyShieldRechargeRateCH(NotifyContext* ctx) {
    return applyCriticalHit(this, kShieldRechargeShift, ctx, &CapShip::OnShieldRechargeRateCHNotified);
}

int CapShip::ApplyWeaponRechargeRateCH(NotifyContext* ctx) {
    return applyCriticalHit(this, kWeaponRechargeShift, ctx, &CapShip::OnWeaponRechargeRateCHNotified);
}

int CapShip::ApplyTractorBeamPowerCH(NotifyContext* ctx) {
    return applyCriticalHit(this, kTractorBeamShift, ctx, &CapShip::OnTractorBeamPowerCHNotified);
}

int CapShip::ApplySpeedCH(NotifyContext* ctx) {
    return applyCriticalHit(this, kSpeedShift, ctx, &CapShip::OnSpeedCHNotified);
}

int CapShip::ApplyPrimaryHyperdriveCH(NotifyContext* ctx) {
    return applyCriticalHit(this, kPrimaryHyperdriveShift, ctx, &CapShip::OnPrimaryHyperdriveCHNotified);
}

int CapShip::ApplyBackupHyperdriveCH(NotifyContext* ctx) {
    return applyCriticalHit(this, kBackupHyperdriveShift, ctx, &CapShip::OnBackupHyperdriveCHNotified);
}

int CapShip::CreateViewsState(int controlKind, NotifyContext* ctx) { return CombatUnit::CreateViews(controlKind, ctx); }

int CapShip::ReadState(istream* in) {
    const int result = CombatUnit::Read(in);
    if (result) {
        ReadValue32(in, &m_hullValueDamage);
        ReadValue32(in, &m_criticalHits);
    }
    return result;
}

int CapShip::WriteState(ostream* out) {
    const int result = CombatUnit::Write(out);
    if (result) {
        WriteValue32(out, &m_hullValueDamage);
        WriteValue32(out, &m_criticalHits);
    }
    return result;
}

int CapShip::ApplyAllState(NotifyContext* ctx) {
    const int result = CombatUnit::ApplyAll(ctx);
    if (result && ctx->m_flag04 == kApplyEverything) {
        ApplyHullValueDamage(ctx);
        ApplyShieldRechargeRateCH(ctx);
        ApplyWeaponRechargeRateCH(ctx);
        ApplyTractorBeamPowerCH(ctx);
        ApplySpeedCH(ctx);
        ApplyPrimaryHyperdriveCH(ctx);
        ApplyBackupHyperdriveCH(ctx);
    }
    return result;
}

int CapShip::ChecksumState(Crc32* crc) {
    const int result = CombatUnit::Checksum(crc);
    if (result) {
        crc->AddInt(m_hullValueDamage);
        crc->AddInt(m_criticalHits);
    }
    return result;
}

int CapShip::RepairStep(NotifyContext* ctx) {
    int ok = 1;
    if (m_hullValueDamage > 0) ok = RepairHullDamage(GetHull() / kHullRepairDivisor, ctx);
    int roll = 0;
    if (!ok) return ok;
    const int totalHits = GetTotalCriticalHits();
    if (totalHits != 0) {
        const int controlLoss = GetDamageControlLoss();
        ok = RollPercentChance(GetDamageControl() - controlLoss, &roll);
    }
    if (!ok || !roll) return ok;

    int pick = GameRandom(totalHits - 1);
    using Setter = int (CapShip::*)(uint32_t, NotifyContext*);
    static const struct {
        int shift;
        Setter set;
    } kSubsystems[] = {
        {kShieldRechargeShift, &CapShip::SetShieldRechargeRateCH},
        {kWeaponRechargeShift, &CapShip::SetWeaponRechargeRateCH},
        {kTractorBeamShift, &CapShip::SetTractorBeamPowerCH},
        {kSpeedShift, &CapShip::SetSpeedCH},
        {kPrimaryHyperdriveShift, &CapShip::SetPrimaryHyperdriveCH},
        {kBackupHyperdriveShift, &CapShip::SetBackupHyperdriveCH},
    };
    for (const auto& subsystem : kSubsystems) {
        const int hits = static_cast<int>(GetCriticalHit(subsystem.shift));
        if (hits != 0 && pick < hits) {
            (this->*subsystem.set)(hits - 1, ctx);
            return 1;
        }
        pick -= hits;
    }
    return 0;
}

int CapShip::ValidateState() {
    int valid = CombatUnit::Validate();
    GameObjIterator children(this, kEveryChild);
    for (children.Begin(); children.Current(); children.Advance()) {
        if (!valid) return 0;
        GameObjNotifier* const child = children.Current();
        valid = isUnitFamily(child->GetFamily()) && ((child->m_flags ^ m_flags) & 0xc0) == 0;
    }
    if (!valid) return 0;

    int characterCount = 0;
    valid = CountCharacters(&characterCount, 0, 0) && characterCount <= 1;
    if (valid) {
        valid = m_hullValueDamage >= 0 && m_hullValueDamage <= GetHull();
        if (valid) {
            if (m_hullValueDamage == GetHull()) valid = (m_stateFlags & kDestroyedFlag) != 0;
            if (valid) {
                valid = GetMaxCriticalHits() != 0;
                valid = valid && GetMaxCriticalHits() != 0;
                valid = valid && GetMaxCriticalHits() != 0;
                valid = valid && GetMaxCriticalHits() != 0;
                valid = valid && HasPrimaryHyperdrive();
                valid = valid && GetHull() != 0;
            }
        }
    }
    if (!valid) return 0;
    return ValidateParentAndCargo();
}

int CapShip::ValidateParentAndCargo() {
    GameObjNotifier* parent = nullptr;
    int valid = GetParent(&parent);
    if (!valid) return 0;

    int locked = (m_stateFlags & kLockedFlag) != 0;
    {
        GameObjIterator children(this, kEveryChild);
        for (children.Begin(); children.Current() && !locked; children.Advance()) {
            locked = (asObject(children.Current())->m_stateFlags & kLockedFlag) != 0;
        }
        if ((m_stateFlags & kDestroyedFlag) == 0 || !locked) {
            valid = (~asObject(parent)->m_stateFlags & kDestroyedFlag) != 0;
        }
    }
    if (!valid) return 0;

    const int fighterCapacity = GetFighterCapacity();
    ChildIterator squadrons(this, &family::kFighterSquadrons, kExistingChildren);
    valid = squadrons.Count() <= fighterCapacity;
    if (!valid) return 0;

    const int troopCapacity = GetTroopCapacity();
    ChildIterator regiments(this, &family::kTroopRegiments, kExistingChildren);
    return regiments.Count() <= troopCapacity;
}

int CapShip::EnsureEventKinds() {
    if (g_capShipEventKindsReady != 0) return 1;
    g_capShipEventKindsReady = 1;
    const int combatUnitKinds = CombatUnit::InitEventKinds(0, &g_capShipEventKinds);
    const bool addedFirst = g_capShipEventKinds.Add(kKindCapShip203, 1) != nullptr && combatUnitKinds != 0;
    const bool addedSecond = g_capShipEventKinds.Add(kKindCapShip270, 1) != nullptr;
    return addedSecond && addedFirst;
}

void CapShipStaticInit() {
    ConstructCapShipEventKinds();
    RegisterCapShipEventKindsAtExit();
}

void ConstructCapShipEventKinds() { new (&g_capShipEventKinds) EventKindSet(); }

void RegisterCapShipEventKindsAtExit() { atexit(DestroyCapShipEventKinds); }

CapShipIterator::CapShipIterator(GameObjNotifier* parent) : ChildIterator(parent, &family::kCapitalShips, 1) {}

CapShipIterator::CapShipIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &family::kCapitalShips, stateFilter) {}

CapShipIterator::CapShipIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &family::kCapitalShips, stateFilter) {}

CapShipIterator::~CapShipIterator() { REB_KEEP_VPTR_STORE(); }

int CapShip::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsState(controlKind, ctx); }

int CapShip::Read(istream* in) { return ReadState(in); }

int CapShip::Write(ostream* out) { return WriteState(out); }

int CapShip::Checksum(Crc32* crc) { return ChecksumState(crc); }

int CapShip::Validate() { return ValidateState(); }

int CapShip::ApplyAll(NotifyContext* ctx) { return ApplyAllState(ctx); }

int CountCapitalShipChildren(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&family::kCapitalShips, side, filter);
}

int RollCriticalHitType(int hullLeft, int damage, int* hitType) {
    int ok = 1;
    *hitType = kHitTypeNone;
    if (hullLeft <= 0) {
        *hitType = kHitTypeDestroyed;
        return 1;
    }
    if (damage == 0) {
        *hitType = kHitTypeNone;
        return 1;
    }
    const int bonus = GameRandom(g_gnprtb0801);
    const int chance = RatioToPercent(damage, damage + hullLeft) + bonus;
    if (chance >= g_gnprtb0802) {
        const TableEntry* record = FindTableRecord(kHitTypeTable, GameRandom(g_gnprtb0803));
        ok = record != nullptr;
        if (ok) *hitType = TableRecordFirstValue(record);
    }
    return ok;
}

void CapShip::SetDataWord28(int value) {
    if (m_data) static_cast<CapShipData*>(m_data)->m_word28 = value;
}

int CapShip::InflictCriticalHit(int damage, int destroyedReason, NotifyContext* ctx) {
    int hitType = 0;
    const int hullDamage = GetHullValueDamage();
    const int hullLeft = GetHull() - hullDamage;
    int ok = RollCriticalHitType(hullLeft, damage, &hitType);
    if (ok) ok = hitType > 0 && hitType < 8;
    if (!ok) return ok;

    switch (hitType) {
        case kHitTypeShield:
            if (GetMaxCriticalHits() - GetCriticalHit(kShieldRechargeShift) != 0) {
                return SetShieldRechargeRateCH(GetCriticalHit(kShieldRechargeShift) + 1, ctx) != 0;
            }
            break;
        case kHitTypeWeapon:
            if (GetMaxCriticalHits() - GetCriticalHit(kWeaponRechargeShift) != 0) {
                return SetWeaponRechargeRateCH(GetCriticalHit(kWeaponRechargeShift) + 1, ctx) != 0;
            }
            break;
        case kHitTypeTractorBeam:
            if (GetMaxCriticalHits() - GetCriticalHit(kTractorBeamShift) != 0) {
                return SetTractorBeamPowerCH(GetCriticalHit(kTractorBeamShift) + 1, ctx) != 0;
            }
            break;
        case kHitTypeSpeed:
            if (GetMaxCriticalHits() - GetCriticalHit(kSpeedShift) != 0) {
                return SetSpeedCH(GetCriticalHit(kSpeedShift) + 1, ctx) != 0;
            }
            break;
        case kHitTypeHyperdrive:
            if (HasPrimaryHyperdrive() - GetCriticalHit(kPrimaryHyperdriveShift) != 0) {
                return SetPrimaryHyperdriveCH(GetCriticalHit(kPrimaryHyperdriveShift) + 1, ctx) != 0;
            }
            if (HasBackupHyperdrive() - GetCriticalHit(kBackupHyperdriveShift) != 0) {
                return SetBackupHyperdriveCH(GetCriticalHit(kBackupHyperdriveShift) + 1, ctx) != 0;
            }
            break;
        case kHitTypeDestroyed:
            return ChangeDestroyedReason(destroyedReason, ctx) != 0;
        default:
            break;
    }
    return ok;
}

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_capShipEventKindsDestroyedGuard;  // 0x6b2b30

void DestroyCapShipEventKinds() {
    if (!(g_capShipEventKindsDestroyedGuard & 0x1)) {
        g_capShipEventKindsDestroyedGuard |= 0x1;
        g_capShipEventKinds.~EventKindSet();
    }
}
