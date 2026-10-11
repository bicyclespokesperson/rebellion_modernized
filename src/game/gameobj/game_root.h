#pragma once
#include <cstdint>

#include "core/stream_io.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_settings.h"
#include "gameobj/object_key.h"
#include "gameobj/side_bits.h"

// The game root (family 0xf1, VT 0x65e938, secondary 0x65e930): the single top object of the game-object tree. It
// owns the one-time setup of a new game (an object-type database, table registry, object database and unresolved-key
// set, then a long sequence of "stages" that each fill in one part of the universe), loads and saves the three views
// of the whole tree, and caches the master objects of the singleton managers that the rest of the game asks for by key.
//
// The setup stage number lives in a global (0x6b2bb4); the stages and what each one calls are listed in StepSetup.

class Crc32;
class RandomGenerator;

class GameRoot : public GameObj
{
public:
  GameRoot();
  ~GameRoot() override;

  uint32_t GetFamily() override;
  int Read(istream* in) override;
  int Write(ostream* out) override;
  int Checksum(Crc32* crc) override;
  int Validate() override;
  // Only side views take part in ApplyAll; the master answers 0.
  int ApplyAll(NotifyContext* ctx) override;
  int Virtual16() override
  {
    return 1;
  }
  int CreateViews(int controlKind, NotifyContext* ctx) override;
  GameObj* CreateViewObject() override;

  // Copies the settings in; returns the stored copy.
  GameSettings& SetSettings(const GameSettings& settings); // 0x513640
  // Flags m_settings.m_field04 if the settings are still the defaults, then enters setup stage 2. Returns 1.
  int BeginSetup(); // 0x513650
  // Runs one setup stage (the global stage number selects it) and advances to the next one when it is done.
  // *status receives the stage's result (1 if the stage does not produce one). Returns whether setup is complete.
  int StepSetup(int* status); // 0x5136d0
  int IsSetupComplete(); // 0x513960

  // ---- bodies behind the Read/Write/Checksum/Validate thunks, the random generator and the consistency checks
  //      (0x518dc0-0x519800), see galaxy/game_root_body.cpp ----
  int BindDifficultyTables(); // 0x518dc0, loads the rule tables for the settings' difficulty
  int ReadBody(istream* in); // 0x518dd0
  int WriteBody(ostream* out); // 0x518ef0
  int ChecksumBody(Crc32* crc); // 0x519060
  int ValidateBody(); // 0x519130
  int ValidateBuilders(); // 0x519350, every created unit is known to its builder manager
  int NoteSideControlChange(SideBits* from, SideBits* to); // 0x518040

  // ---- the pending battle (rules/game_root_battle.cpp, 0x516710-0x517510) ----
  int GetRootKey(ObjectKey* out); // 0x516710, copies the key of the system whose battle is pending
  // Whether `side` can retreat from the pending battle: *gravityWell = the opponent still has a working gravity well
  // projector, *hasMobileUnit = a fleet or squadron of the side can move, *canRetreat = it found a system to go to
  // and *retreatDays the trip's length. Returns whether the battle system exists and the side is 1 or 2.
  int QueryRetreat(int side,
                   uint32_t* gravityWell,
                   uint32_t* hasMobileUnit,
                   int* canRetreat,
                   uint32_t* retreatDays); // 0x516730
  int ClearBattle(); // 0x516cd0, forgets the pending battle's key and context
  // Picks the first system with a pending battle, forgets the previous one and starts it. Returns whether all steps worked.
  int StartPendingBattle(); // 0x516d50
  NotifyContext* GetBattleContext(); // 0x516ee0, +0x90
  // Before the fight: removes the battle flag, the attacker's squadrons leave and the defender's units are auto-routed.
  int PrepareBattle(); // 0x516ef0
  // After the fight: the winner takes the battle-won control kind, the loyalties shift, and the battle is cleared.
  int ResolveBattle(); // 0x517270
  int ValidateLocks(); // 0x519470, every object's lock count equals the number of references to it
  int CreateRandomGenerator(); // 0x519740, seeded from the settings; registers it with SetGameRandom
  int DestroyRandomGenerator(); // 0x5197c0, returns 1
  int HasRandomGenerator(); // 0x5197f0

  // ---- setup stage bodies (0x519800-0x51bf30), see galaxy/game_setup_stages.cpp and game_setup_seed.cpp ----
  int SetupRootObject(NotifyContext* ctx); // 0x519800, stage 8
  int CreateRootChildren(NotifyContext* ctx); // 0x519910, stage 9
  int CreateSectors(int galaxySize, NotifyContext* ctx); // 0x519c20, stage 0xa
  int DistributeSystems(NotifyContext* ctx); // 0x519d00, stage 0xb
  int PlaceNextPooledSystem(int* done, NotifyContext* ctx); // 0x51a280, stage 0xc
  int BuildRulesObject(NotifyContext* ctx); // 0x51a530, stage 0xd
  int BeginSystemWalk(NotifyContext* ctx); // 0x51a550, stage 0xe
  int InitializeNextSystem(int* done, NotifyContext* ctx); // 0x51a570, stage 0xf
  int PlaceHeadquarters(NotifyContext* ctx); // 0x51a640, stage 0x10
  int PlaceCoreSystem(NotifyContext* ctx); // 0x51a680
  int PlaceCoruscantSystem(NotifyContext* ctx); // 0x51a870
  int SeedInitialForces(NotifyContext* ctx); // 0x51aa50, stage 0x12
  int SeedSystemTroops(NotifyContext* ctx); // 0x51ab20
  int SeedYavinForces(NotifyContext* ctx); // 0x51acb0
  int SeedKey154Forces(NotifyContext* ctx); // 0x51ad90
  int SeedCoruscantForces(NotifyContext* ctx); // 0x51ae20
  int SeedStartingFleet(NotifyContext* ctx); // 0x51af00
  int SeedCoruscantFleet(NotifyContext* ctx); // 0x51b080
  int SeedAllianceReserves(NotifyContext* ctx); // 0x51b1b0
  int SeedEmpireReserves(NotifyContext* ctx); // 0x51b1d0
  int SeedSideReserves(int side, NotifyContext* ctx); // 0x51b1f0
  int SetupCharacterManagerA(NotifyContext* ctx); // 0x51b6d0, stage 0x14
  int SetupCharacterManagerB(NotifyContext* ctx); // 0x51b710, stage 0x15
  int SetupSideObjects(NotifyContext* ctx); // 0x51b750, stages 0x11 and 0x13
  int InitializeAllObjects(NotifyContext* ctx); // 0x51b7f0, stage 0x17
  int FinishSideViews(NotifyContext* ctx); // 0x51b940, stage 0x18 (runs FinishSideView on both side views)
  int FinishSideView(NotifyContext* ctx); // 0x51b990
  // Creates the system `systemKey` under the sector of its record with the given control side, applies `level` and
  // remembers the key of the headquarters system (m_key154). `unusedA` is ignored by the original.
  int PlaceSystem(ObjectKey* systemKey, int unusedA, int initArg, int level, NotifyContext* ctx); // 0x51bc70
  // Sums the seed value (SdRecord field 0x4c) of every record named in `list` into *total, times `factor`.
  int SumSeedValues(int factor, ObjectRefList* list, int* total); // 0x51be20
  // Creates the units of rows `first`..`last` of table `tableType` under `parent`.
  int SeedFromTableRows(GameObjNotifier* parent, int tableType, int first, int last, NotifyContext* ctx); // 0x51be90
  // The first `count` rounds create the first listed object as a child of `parent` and the others under it.
  int CreateChildrenFromList(GameObjNotifier* parent, int count, ObjectRefList* list, NotifyContext* ctx); // 0x51bf30

  // Loads a whole game: the unresolved-key set, then the root and its tree once per view. *out receives the master
  // root (null if none could be built). Returns 1 on success.
  static int LoadGame(istream* in, GameRoot** out); // 0x513a50
  // Saves / checksums the three views of `root`, one after the other.
  static int SaveGame(ostream* out, GameRoot* root); // 0x513df0
  static int ChecksumGame(Crc32* crc, GameRoot* root); // 0x513e80

  // ---- fields ----
  GameSettings m_settings; // +0x58, m_settings.m_field04 is also read as the "defaults" flag
  int m_unk6c; // +0x6c, 0x500
  int m_unk70; // +0x70
  int m_unk74; // +0x74
  int m_unk78[5]; // +0x78
  ObjectKey m_key8c; // +0x8c
  NotifyContext m_ctx90; // +0x90
  RandomGenerator* m_random; // +0xac, the gameplay random generator (registered with SetGameRandom)
  // Master objects of singletons, looked up by key once the tree is loaded (0x513a50), cleared by the destructor.
  GameObjNotifier* m_master_f2000003; // +0xb0
  GameObjNotifier* m_master_20000005; // +0xb4
  GameObjNotifier* m_master_f8000006; // +0xb8
  GameObjNotifier* m_master_f9000007; // +0xbc, the rules object
  GameObjNotifier* m_master_fa000008; // +0xc0
  GameObjNotifier* m_master_f300000f; // +0xc4
  GameObjNotifier* m_master_f3000010; // +0xc8
  GameObjNotifier* m_master_98000440; // +0xcc
  GameObjNotifier* m_master_98000441; // +0xd0
  GameObjNotifier* m_master_98000442; // +0xd4
  GameObjNotifier* m_master_98000480; // +0xd8
  GameObjNotifier* m_master_98000481; // +0xdc
  ObjectRefList m_refLists[7]; // +0xe0
  ObjectKey m_key150; // +0x150, the system picked as the second headquarters by setup stage 0xb
  ObjectKey m_key154; // +0x154, the key of that system once it was created (stage 0xc)
  GameObjNotifier* m_nextSystem; // +0x158, the next system setup stage 0xf initializes
  NotifyContext m_ctx15c; // +0x15c, the context the setup stages run with
  NotifyContext m_ctx178; // +0x178
  NotifyContext m_ctx194; // +0x194
};

// The single game root (0x6b2bb0), null before setup stage 6 and after the root is destroyed.

// The factory registered for family 0xf1 (0x513970).
GameObjNotifier* CreateGameRoot();
