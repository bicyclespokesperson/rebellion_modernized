#pragma once
#include <cstdint>

#include "core/entry_identity.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/side_bits.h"
#include "gameobj/status_code.h"
#include "rules/loyalty_rule.h"

// The event rules (0x513f70-0x518b20): what the game does, beyond changing the object itself, when an object of the
// strategic game changes. Every scheduled event (events/source_events.h, system_events.h, person_events.h) and the
// move command run one of the ApplyEvent functions; they tell the managers that care (missions, characters,
// manufacturing, sides), call the virtuals of the containers an object leaves and enters (GameObj slots 49-53) and
// raise follow-up work. Free functions of cdecl convention that return 1 when everything they ran succeeded: they all
// run every step even after one failed ("ok = step() && ok" evaluates the step first).
//
// The support functions below them (0x517510-0x518ac0) are shared between the handlers and the game root.

namespace event_rules
{

constexpr uint32_t kUsable = 0x1;
constexpr uint32_t kCreated = 0x2;
constexpr uint32_t kCompleted = 0x4;
constexpr uint32_t kDestroyed = 0x8;
constexpr uint32_t kEnroute = 0x10;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kExisting = 0x40;
constexpr uint32_t kDamaged = 0x200;
constexpr uint32_t kLocked = 0x4000;

} // namespace event_rules

// ---- support (0x517510-0x518ac0) -------------------------------------------------------------------------------

// Posts event 0x372 for a destroyed, unlocked object that has no children left (and is not the game root).
int PostDeleteIfDestroyedAndEmpty(GameObj* object, NotifyContext* ctx); // 0x517510
// ReleaseIfDestroyed for a destroyed object that allows it (slot 17).
int ReleaseDestroyedObject(GameObj* object, NotifyContext* ctx); // 0x5175b0
// Every idle manufacturing manager (nothing left to build) whose location is `object` is retargeted at its system.
int RetargetIdleManufacturers(GameObj* object, NotifyContext* ctx); // 0x5175e0
// Gives a unit that still carries its record's name a numbered one: "<record name> <n>".
int NumberUnitName(GameObj* object, NotifyContext* ctx); // 0x517760
// Tells every system of every sector whether a Death Star is in its sector.
int UpdateDeathStarNearby(NotifyContext* ctx); // 0x5178b0
// The loyalty rules of a system whose side bits changed from *from to *to (and the victory state if it is Coruscant).
int ApplySideBitsChangeRules(GameObj* system, SideBits* from, SideBits* to, NotifyContext* ctx); // 0x517aa0
// Coruscant (system id 0x109) part of the above and of the value changes.
int ApplyCoruscantRules(GameObj* system, int value, NotifyContext* ctx);                            // 0x517bb0
int ApplyCoruscantControlChange(GameObj* system, SideBits* from, SideBits* to, NotifyContext* ctx); // 0x517be0
int ApplyCoruscantValueRules(GameObj* system, int value, NotifyContext* ctx);                       // 0x517ca0
// The departure / en route rules of an object that moved (value == 0 only for the first).
int ApplyDepartureRules(GameObj* unused, int value, ObjectKey* system, NotifyContext* ctx); // 0x517cd0
int ApplyEnrouteRules(GameObj* object, int unused, ObjectKey* system, NotifyContext* ctx);  // 0x517de0
// The loyalty pull of the two sides' unit strengths on `object`'s system.
int ApplyStrengthImbalance(GameObj* object, int side, int valueSide1, int valueSide2, NotifyContext* ctx); // 0x517ee0
// Recomputes victory flags from the Alliance HQ, Coruscant and (in standard mode) the four story characters.
// Changes are stored with `ctx`. Returns whether the objects required by the selected mode exist.
int UpdateVictoryState(NotifyContext* ctx); // 0x5180e0
// After setup, rechecks victory before publishing flags to both sides, covering HQ destruction from any cause.
// Returns 1 if the sides or the game root do not exist.
int PublishVictoryState(); // 0x518460

// Look-ups of the objects an object belongs to. All store null in *out first.
int GetOwnerObject(GameObjNotifier* object, GameObjNotifier** out); // 0x518530, via the owner key
int GetOwnerSystem(GameObjNotifier* object, GameObjNotifier** out); // 0x5185c0, the owner key's family is a system
int GetOwnerSector(GameObjNotifier* object, GameObjNotifier** out); // 0x518690, that system's parent sector
// The side object (game root member) of a control side / of side 1 or 2; true unless a valid side has no object.
int GetControlSideObject(GameObjNotifier* object, GameObjNotifier** out); // 0x518750
int GetSideObjectForIndex(int side, GameObjNotifier** out);               // 0x5187a0

// Collects the systems the fleets of an activity request's team are in into *out (keys with value 0). *status is
// {1,-1} on success, {1,0x12} when a fleet is unknown or destroyed and {1,1} when something else failed. See
// events/request_orders.cpp.
int CollectTeamSystems(ActivityRequest* request, ObjectKeyValueSet* team, ObjectKeyValueSet* out,
                       StatusCode* status); // 0x5187f0

// Applies `rule` to every sector / to the sector `object` is in.
int ApplyLoyaltyRuleToAllSectors(LoyaltyRule* rule, NotifyContext* ctx);                            // 0x518a70
int ApplyLoyaltyRuleToParentSector(GameObjNotifier* object, LoyaltyRule* rule, NotifyContext* ctx); // 0x518ac0

// ---- the event handlers (0x5140c0-0x516360) --------------------------------------------------------------------

int ApplyEvent300(ObjectKey* object, ObjectKey* from, ObjectKey* to, int value, NotifyContext* ctx); // 0x5140c0
int ApplyEvent301(ObjectKey* object, ObjectKey* from, ObjectKey* to, int value, NotifyContext* ctx); // 0x514510
int ApplyEvent31d(ObjectKey* object, ObjectKey* from, ObjectKey* to, int value, NotifyContext* ctx); // 0x514820
int ApplyEvent302(ObjectKey* object, int valueA, int valueB, NotifyContext* ctx);                    // 0x514890
int ApplyEvent318(ObjectKey* object, int valueA, int valueB, NotifyContext* ctx);                    // 0x514a20
int ApplyEvent320(ObjectKey* object, ObjectKey* keyC, ObjectKey* keyD, NotifyContext* ctx);          // 0x514a60
int ApplyEvent308(ObjectKey* object, int value, NotifyContext* ctx);                                 // 0x5150c0
int ApplyEvent307(ObjectKey* object, int value, NotifyContext* ctx);                                 // 0x515480
int ApplyEvent306(ObjectKey* object, int value, NotifyContext* ctx);                                 // 0x515a50
int ApplyEvent316(ObjectKey* object, int value, NotifyContext* ctx);                                 // 0x515bb0
int ApplyRoleStateEvent(ObjectKey* object, int value, NotifyContext* ctx);                           // 0x515970
int ApplySystemEvent31b(ObjectKey* system, SideBits* from, SideBits* to, NotifyContext* ctx);        // 0x515c00
int ApplySystemEvent303(ObjectKey* system, int value, NotifyContext* ctx);                           // 0x515cd0
int ApplySystemEvent304(ObjectKey* system, int value, NotifyContext* ctx);                           // 0x515dc0
int ApplySystemEvent309(ObjectKey* system, int value, NotifyContext* ctx);                           // 0x515e30
int ApplySystemEvent310(ObjectKey* system, int value, NotifyContext* ctx);                           // 0x515e70
int ApplySystemEvent30f(ObjectKey* system, int value, NotifyContext* ctx);                           // 0x515eb0
int ApplySystemEvent311(ObjectKey* system, int value, NotifyContext* ctx);                           // 0x515ef0
int ApplySystemEvent305(ObjectKey* system, int value, NotifyContext* ctx);                           // 0x515f40
int ApplyEvent30a(ObjectKey* object, int valueA, int valueB, NotifyContext* ctx);                    // 0x516220
// *smuggled = 1 when the smuggling percent (absolute value) of the facility's system wins a percent roll. Returns
// whether the system exists.
int RollSmuggling(GameObjNotifier* facility, int* smuggled);                   // 0x5166a0
int ApplyEvent315(ObjectKey* object, int kind, int value, NotifyContext* ctx); // 0x516360

// ---- the move command's helpers ----
int CheckMoveEnabled(const ObjectKey* object, StatusCode* status);                             // 0x5152e0
int CheckMoveValid(const ObjectKey* object, const ObjectKey* destination, StatusCode* status); // 0x515390
int ExecuteMove(const ObjectKey* object, const ObjectKey* destination, NotifyContext* ctx);    // 0x515440

// Loads the rule tables of the difficulty; the game root runs it first (0x513f70). Returns 1.
int BindRuleTables(int difficulty); // 0x513f70
