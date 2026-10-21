#pragma once
#include <cstdint>

#include "character/character.h"
#include "character/role.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"

class Fleet;
class System;

// CharacterMgr (family 0xfa, key 0xfa000008, ctor 0x55ec90, VT 0x6630c0, secondary ObjectBase VT 0x6630bc, 0x60 bytes)
// is the manager of all the people of the game (the game root's cached member +0xc0). It owns
//   * two flags (word at +0x58): SeatOfPower (the Emperor is at Coruscant under Empire control; the Empire's people get
//   the
//     GNPRTB 0xc04 leadership bonus) and PickupInProgress (Darth Vader's pickup is under way),
//   * a master-only value at +0x5c (the galaxy size 1-3, which selects how many minor characters each side starts
//   with),
// and reacts to what happens to systems, fleets and people: it places the story characters when a game is set up (the
// stages 0x14 and 0x15 of the game root), recruits minor characters, resolves the fights and Force detection between
// people that meet, shifts every person's loyalty when a system changes hands and keeps the leadership of the Empire's
// people current.
//
// Vtable (122 slots): the first 111 are GameObj's. 111-115 forward the person checks of Character (slots 179-183) for
// the person named by a key, 116-117 are the hooks of the two fights of people (see ResolveEncounterFight), 118-119 are
// the OnXChanged events of the two flags and 120-121 their OnXNotified events.
namespace character_mgr_flag
{
constexpr uint32_t kSeatOfPower = 0x1;
constexpr uint32_t kPickupInProgress = 0x2;
} // namespace character_mgr_flag

class CharacterMgr : public GameObj
{
  public:
  CharacterMgr();           // 0x55ec90
  ~CharacterMgr() override; // 0x55ecf0 (deleting), 0x55ed10

  // ---- overrides of GameObj slots ----
  uint32_t GetFamily() override;                                  // 1, 0x562090: 0xfa
  int Read(istream* in) override;                                 // 5, 0x562040
  int Write(ostream* out) override;                               // 6, 0x562050
  int Checksum(Crc32* crc) override;                              // 7, 0x562080
  int Validate() override;                                        // 8, shared jump stub 0x569870
  int ApplyAll(NotifyContext* ctx) override;                      // 9, 0x562060
  int GetEventKinds(int unused, EventKindSet* out) override;      // 15, 0x55efd0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x55eff0
  int UpdateObservedByAlliance(NotifyContext* ctx) override;      // 35, 0x5620a0
  int UpdateObservedByEmpire(NotifyContext* ctx) override;        // 36, 0x5620c0
  int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, shared stub 0x5774f0 (own copy 0x562190)
  int InitializeAfterSetup(intptr_t ctxArg) override;             // 48, 0x5620e0: the NotifyContext* of the setup
  GameObj* CreateViewObject() override;                           // 57, 0x561fd0

  // ---- new virtuals ----
  // Forwarders to the person named by `key` (null or non-person keys fail): slot 179 of Character.
  virtual int ForwardMatchesRequestedCommandKind(const ObjectKey* key, intptr_t a, int* out); // 111, 0x55f0b0
  // Slot 180 of Character after the checks that depend on where the person is (see FindLocation); *out starts as
  // {1,-1}.
  virtual int ForwardCheckActivity3(const ObjectKey* key, const ActivityRequest* request,
                                    StatusCode* out); // 112, 0x55f160
  virtual int ForwardCheckActivityStatus3(const ObjectKey* key, const ActivityRequest* request,
                                          StatusCode* out); // 113, 0x55f320
  virtual int ForwardCheckActivity4(const ObjectKey* key, const ActivityRequest* request,
                                    StatusCode* out); // 114, 0x55f3f0
  // Slot 183 of Character; for a person controlled by one side, the target must not be a person of the other side.
  virtual int ForwardCheckActivityStatus4(const ObjectKey* key, const ActivityRequest* request,
                                          StatusCode* out); // 115, 0x55f4c0
  // Gives the person `key` the command named by the request (a command kind 1-3); whoever held it in the same fleet or
  // system loses it. Always returns 0.
  virtual int AssignCommand(const ObjectKey* personKey, const ActivityRequest* request,
                            NotifyContext* ctx); // 116, 0x561bb0
  // Tells the rules object (game root member +0xbc) about the target of the request's person.
  virtual int ReportTarget(const ObjectKey* personKey, const ActivityRequest* request,
                           NotifyContext* ctx);                 // 117, 0x561e90
  virtual int OnSeatOfPowerChanged(int on, NotifyContext* ctx); // 118, 0x562400
  virtual int OnPickupInProgressChanged(int on, NotifyContext* ctx)
  {
    return 1;
  } // 119, shared stub 0x524fb0
  virtual int OnSeatOfPowerNotified(int on, NotifyContext* ctx);      // 120, 0x562450
  virtual int OnPickupInProgressNotified(int on, NotifyContext* ctx); // 121, 0x5624a0

  // ==== flags ====
  int SetSeatOfPower(int on, NotifyContext* ctx);      // 0x55f650
  int SetPickupInProgress(int on, NotifyContext* ctx); // 0x55f6c0
  int ApplySeatOfPower(NotifyContext* ctx);            // 0x5622e0
  int ApplyPickupInProgress(NotifyContext* ctx);       // 0x562370
  // The Emperor holds the seat of power: he is usable, free and at the system 0x90000109 under the Empire's control.
  int UpdateSeatOfPower(NotifyContext* ctx); // 0x5617b0
  // Leadership adjustment of `person`: GNPRTB 0xc04 for the Empire's people while the Emperor holds the seat of power.
  int UpdateLeadershipAdjustment(Character* person, NotifyContext* ctx); // 0x5618d0

  // ==== persistence (the slots forward to these) ====
  int ReadBody(istream* in);            // 0x5621b0
  int WriteBody(ostream* out);          // 0x562200
  int ChecksumBody(Crc32* crc);         // 0x562290
  int ApplyAllBody(NotifyContext* ctx); // 0x562250

  // ==== game setup (stages 0x14 and 0x15 of the game root) ====
  // Creates a person for every record of the major and minor character tables (the side comes from the record).
  int CreateRecordCharacters(NotifyContext* ctx);                   // 0x55f730
  int CreateRecordCharacter(DatRecord* record, NotifyContext* ctx); // 0x562520
  // Puts the story characters and the first minor characters of both sides where the game starts.
  int PlaceStartingCharacters(NotifyContext* ctx); // 0x55f7b0
  // Recruits a random minor character of `side` that has not been created yet at `location`; *out is its key. The last
  // one of a side marks the side's recruitment as done.
  int RecruitRandomMinor(GameObjNotifier* location, int side, ObjectKey* out, NotifyContext* ctx); // 0x55fc80
  // Number of the minor characters of `side` that have not been created yet.
  int CountUncreatedMinors(int side); // 0x55ef30
  // Creates the person `key` (master, not yet created) at `location`.
  int PlaceCharacter(const ObjectKey* key, GameObjNotifier* location, NotifyContext* ctx); // 0x55fe70
  // Places the person `key` at a random place of its side.
  int PlaceAtRandomSystem(const ObjectKey* key, NotifyContext* ctx);           // 0x562680
  int RecruitMinorAtRandomPlace(int side, ObjectKey* out, NotifyContext* ctx); // 0x562610
  // A random usable system or capital ship of `side` (each system and each fleet counts once; a fleet gives one of its
  // ships).
  int PickRandomPlace(uint32_t side, GameObjNotifier** place, NotifyContext* ctx); // 0x562710

  // ==== people ====
  // The ship, fleet and system that hold `person` (null for what is not there). Fails if a parent is missing.
  int FindLocation(GameObj* person, GameObjNotifier** ship, GameObjNotifier** fleet,
                   GameObjNotifier** system); // 0x55ed30
  // Loyalty += amount for every person of `side` (-= for the others; see Character::AdjustLoyaltyForSide). False for
  // side 3.
  int AdjustAllLoyalty(int amount, int side, NotifyContext* ctx); // 0x561910
  int InjureByEscapeBase(Character* person, NotifyContext* ctx);  // 0x561b60

  // ==== reactions to events (called from the event handlers 0x5140c0-0x517270) ====
  int ShiftLoyaltyForSystem(System* system, int side, NotifyContext* ctx); // 0x560d00
  int OnLocationChanged(const ObjectKey* object, const ObjectKey* from, const ObjectKey* to,
                        NotifyContext* ctx);                                               // 0x560d50
  int OnSystemOwnerChanged(const ObjectKey* systemKey, int destroyed, NotifyContext* ctx); // 0x561280
  int OnEmperorMoved(const ObjectKey* object, const ObjectKey* from, const ObjectKey* to, int unused,
                     NotifyContext* ctx); // 0x561340
  int OnSystemControlChanged(const ObjectKey* systemKey, uint32_t* before, uint32_t* after,
                             NotifyContext* ctx); // 0x5613f0
  int OnFacilityOrPersonMoved(const ObjectKey* object, const ObjectKey* other, const ObjectKey* unused, int moved,
                              NotifyContext* ctx);                                                         // 0x561470
  int OnEmperorChanged(const ObjectKey* object, int a, int b, NotifyContext* ctx);                         // 0x5615b0
  int OnCoruscantControlChanged(System* system, uint32_t* before, uint32_t* after, NotifyContext* ctx);    // 0x561670
  int ShiftLoyaltyForControlChange(System* system, uint32_t* before, uint32_t* after, NotifyContext* ctx); // 0x5616a0
  int ShiftLoyaltyForHeadquarters(GameObjNotifier* facility, int moved, NotifyContext* ctx);               // 0x561780
  // A person who discovers Force users among the people around: makes `discoverer` find a Force-potential colleague.
  int DiscoverForceUser(Character* discoverer, NotifyContext* ctx); // 0x560a30
  // Links two people in an encounter (each stores the other's key).
  int BeginEncounter(Character* detected, Character* detector, NotifyContext* ctx); // 0x55ff60
  // Settles the fight of the two people of an encounter: injuries, skill gains, Force experience of the story
  // characters.
  int ResolveEncounterFight(Character* member, Character* other, NotifyContext* ctx); // 0x560200
  // A captive tries to escape with the help of the people and troops around.
  int RollCaptiveEscape(Character* captive, int value, NotifyContext* ctx); // 0x561990

  static int BuildEventKinds();     // 0x5624f0, g_characterMgrEventKinds at 0x6bb798
  static GameObjNotifier* Create(); // 0x562120, the object factory of family 0xfa

  // ==== fields ====
  uint32_t m_flags58; // +0x58, character_mgr_flag
  int m_galaxySize;   // +0x5c, master only
};

// The game root's character manager (null before the game is set up).
inline CharacterMgr* GetCharacterManager()
{
  return static_cast<CharacterMgr*>(GetGameRootMember_c0());
}
