#pragma once
#include <cstdint>
#include <memory>
#include <set>

#include "app/player_controller_decl.h"
#include "core/entry_identity.h"
#include "core/game_string.h"
#include "core/stopwatch.h"
#include "tactical/tactical_types.h"

// The tactical engine (0x6b8 bytes, VT 0x66bae8): lazily owned by the current battle flow and created on first use by the facade
// (tactical_facade.h) when the strategic battle flow hands a battle over (app/battle_flow.h), destroyed again when the
// results have been read back.
//
// `m_abstract` (+0x668) selects the mode: 1 = auto-resolve (a headless HeadlessSimulator, DAT_006bc4a8, runs the
// battle without any rendering), 0 = the full 3D battle (the object manager DAT_006bc4a0 and the 3D view).

struct HeadlessSimulator;
class TacticalHub;
class TacticalRegistry;

// An object the game deletes through its virtual destructor.
class OwnedObject
{
public:
  virtual ~OwnedObject();
};

// One mapping of the tactical type table: the 16-bit record type bits of a unit (key) and the sprite/sequence id the
// 3D view uses for it (value). The tree is ordered by key, then value, both unsigned.
class TypeMapNode : public AvlNode
{
public:
  TypeMapNode(); // 0x5f5c10 base, then key/value stored by the TacticalGame constructor
  ~TypeMapNode() override; // 0x59a840 (scalar deleting destructor 0x59a820)

  uint16_t m_key; // +0x18
  uint16_t m_value; // +0x1a
};

class TypeMapTree
{
public:
  TypeMapTree();
  ~TypeMapTree();
  void Insert(TypeMapNode* node);
  void Clear();
  const auto& Entries() const
  {
    return m_nodes;
  }

private:
  struct NodeOrder
  {
    bool operator()(const std::unique_ptr<TypeMapNode>& a, const std::unique_ptr<TypeMapNode>& b) const;
  };
  std::multiset<std::unique_ptr<TypeMapNode>, NodeOrder> m_nodes;
  bool m_clearing = false;
};

class TacticalGame
{
public:
  TacticalGame(); // 0x597610
  virtual ~TacticalGame(); // 0x599ad0 (scalar deleting destructor 0x599a50)

  // Sprite id of a type key, 0 when the key is unknown.
  uint16_t LookupTypeSprite(int16_t key); // 0x5975d0

  // Enters the battle: abstract battles reset the simulator, 3D battles switch the application to `appState` and
  // tell the object manager which side is the player.
  void BeginBattle(int appState); // 0x599a70
  // Destroys the simulator, the object manager, the hub and the turn-sync state. Returns 1.
  int ShutDownBattle(); // 0x599bc0
  // Index of the table entry (0-255) whose sprite id equals the low 12 bits of `typeBits`; 0 if none.
  uint32_t FindShipTypeIndex(uint32_t typeBits); // 0x599c40
  int GetPlayerSide(); // 0x599c70, the object manager's player side
  void CallClientSlot1(); // 0x599c80

  void PostHubMessage0(int arg); // 0x59a3f0
  void PostHubMessage3(int arg); // 0x59a410
  void PostHubMessage2(int arg); // 0x59a430
  void PostHubMessage1(int arg); // 0x59a450
  void PostHubNotice(int arg, int unused); // 0x59a470
  void CopyHubPlayerFlags8(uint32_t* out); // 0x59a490
  void CopyHubPlayerFlags4(uint32_t* out); // 0x59a4b0
  void PostHubPair(int a, int b); // 0x59a4d0
  int ResetBattleView(); // 0x59a4f0
  int HubIsRunning(); // 0x59a520

  // One frame. Returns 1 once the battle is over (and then m_outcome holds the result); the parameter is unused.
  int Update(float unused); // 0x59a530
  // Advances the 3D battle by `milliseconds`; returns 1 when it ended.
  int Advance(float milliseconds); // 0x59a6a0

  void ForwardToObjectManager(int a, int b, int c); // 0x59a710
  void GetOutcome(BattleOutcome* out); // 0x59a730
  int AddDeathStar(int family,
                   int id,
                   int notCompleted,
                   int hasFacility,
                   float hull,
                   float hullNow,
                   int hangarFree); // 0x59a750

  int m_unk04; // +0x04
  OwnedObject* m_unk08; // +0x08 deleted by the destructor (always null in practice)
  int m_unk0c; // +0x0c
  std::string m_stateNames[12]; // +0x10 "ssNONE" ... "ssDEAD": the sprite-state names
  std::string m_shipTypeNames[38]; // +0xa0 "CALAMARI" ... "DEATH_STAR "
  int m_spriteIds[256]; // +0x268 (only the first 38 are set; FindShipTypeIndex scans all 256)
  int m_abstract; // +0x668
  BattleOutcome m_outcome; // +0x66c
  TypeMapTree m_typeMap; // +0x688
  int m_busy = 0; // +0x690 non-zero while the engine is being set up or torn down: Update does nothing
  int m_unk694; // +0x694, 1
  Stopwatch m_clock; // +0x698 times the 3D frames
  int m_firstFrame; // +0x6b0, 1 until the first 3D frame has been timed
  int m_unk6b4; // +0x6b4

  HeadlessSimulator* Simulator();
  HeadlessSimulator* GetOrCreateSimulator();
  void DestroySimulator();
  TacticalRegistry* Registry();
  TacticalRegistry* GetOrCreateRegistry();

private:
  std::unique_ptr<HeadlessSimulator> m_simulator;
  std::unique_ptr<TacticalRegistry> m_registry;
};

// Sets up the battle (a human-vs-AI game; the variant without vote times is for human-vs-human): both sides' fleet figures
// and the mode, either in the simulator (abstract) or in the object manager (3D). Free functions so that the facade's
// by-value copy of the name is made before the singleton is fetched, as in the original.
int TacticalGameSetupBattle(TacticalGame* self,
                            int humanIsNotAlliance,
                            int systemField44,
                            std::string name,
                            float sideAValue,
                            float sideBValue,
                            int sideAC,
                            int sideBC,
                            GameTime timeA,
                            GameTime timeB,
                            int battleKindNotAlliance,
                            int systemFlag8,
                            int controlIsEmpire,
                            int controlIsAlliance,
                            int abstractFlag); // 0x599c90
int TacticalGameSetupBattleLocal(TacticalGame* self,
                                 int humanIsNotAlliance,
                                 int systemField44,
                                 std::string name,
                                 float sideAValue,
                                 float sideBValue,
                                 int sideAC,
                                 int sideBC,
                                 int battleKindNotAlliance,
                                 int systemFlag8,
                                 int controlIsEmpire,
                                 int controlIsAlliance,
                                 int abstractFlag); // 0x59a0b0

// The battle flow's tactical game, created on first use (null if there is no flow or allocation fails).
TacticalGame* GetTacticalGame(); // 0x597530
// Shuts the battle down and deletes the singleton.
void DestroyTacticalGame(); // 0x5975a0

// The headless simulator (DAT_006bc4a8, 0x274 bytes), created on first use.
HeadlessSimulator* GetHeadlessSimulator(); // 0x596f30
