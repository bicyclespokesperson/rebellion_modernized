#pragma once
class AiNoticeGroup;
#include <cstddef>
#include <cstdint>
#include <map>

#include "core/entry_identity.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"
#include "orders/order_list.h"
#include "strategy_ui/ui_handler_table.h"

// Minimal declarations of the player-controller classes (0x472-0x4a0, owned by other agents) that GameSession
// drives. Everything here is a stand-in meant to be dropped, or merged into the real headers, once the owners
// publish theirs. Unknown parts are padding; descriptive names preserve the original virtual slot order.
//
//   PlayerController   abstract base, VT 0x65aef0 (0x50 bytes, 96 virtual slots)
//     HumanPlayer      0xc4 bytes, VT 0x65ad70, ctor 0x486120 / 0x4861b0(side)
//     AiPlayer         0x42c bytes, VT 0x65abf0, see ai/ai_player.h
//     RemotePlayer     0x60 bytes, VT 0x65aa70, see multiplayer/remote_player.h
//
// A GameSession has two controllers, one per side (side 1 = Alliance, 2 = Empire); the "vote" fields are the
// negotiation state of a battle-type vote that is exchanged between the two controllers.

class PlayerController;
class HumanPlayer;
using PlayerMessage = MessageBase;

class NotifPayload;
class MessageBase;

// A 4-byte time value (0x4fcd90 assigns, 0x4fcdd0 / 0x42dbe0 zero it; 0x461c4000 = 10000.0f is stored in it).
struct GameTime {
  float m_value; // +0x00
};

enum PlayerKind {
  kPlayerHuman = 1,
  kPlayerRemote = 2,
  kPlayerAi = 3,
  kPlayerAiCodex = 4,
  kPlayerAiClaude = 5,
};

// Messages that have been sent but not yet delivered (the order module's MessageTree, VT 0x661030).
using PlayerMessageTree = MessageTree;

// The per-player notice bookkeeping embedded in a human player at +0x6c (events/player_dispatch.cpp): it turns the
// notifications the game raises into UiNotice objects (a notice class per kind, events/player_notices.h), keeps them in
// four trees by class, and drives the message index, the button highlights and the dialogs that show them.
class PlayerDispatch
{
  public:
  PlayerDispatch();         // 0x4888f0
  ~PlayerDispatch();        // 0x488990
  void Read(istream* in);   // 0x488a30
  void Write(ostream* out); // 0x488ac0
  void ClearRecords();      // 0x488b30: empties the trees of dialogs
  // Builds the notice for a list of notification payloads and files it, or tells the payloads nobody wants them.
  void HandleNotifications(const AiNoticeGroup* payloads); // 0x488b50
  void Update();                                           // 0x488b90: opens the next waiting dialog
  void AddMessage(UiNotice* message);                      // 0x488ca0: files or retires a dialog notice by class
  void AddArrivalNotice(const ObjectKey* key, int attack); // 0x488d20
  int ClassifyNotifications(
      const AiNoticeGroup* payloads); // 0x488de0: the notice kind the payloads describe, 0 for none
  UiNotice* CreateNotice(int kind, const AiNoticeGroup* payloads); // 0x489740
  void DiscardPayloads(const AiNoticeGroup* payloads);             // 0x48a040
  void AddNotice(UiNotice* notice);                                // 0x48a060
  void RemoveRecord(int id);                                       // 0x48a190
  unsigned GetStateValue(int key);                                 // 0x48a1c0
  void SetDefaultState(int state);                                 // 0x48a1f0
  void SetStateValue(int key, int value);                          // 0x48a210
  void RefreshRecords();                         // 0x48a2a0: drops stale notices, updates the highlights
  void QueueConfirmation(MessageBase* message);  // 0x48a340
  void OnChatMessage(MessageBase* message);      // 0x48a3b0
  void QueueChat(std::string* text, int unused); // 0x48a470
  int IsIdle();                                  // 0x48a500
  void ClearAlerts(unsigned mask);               // 0x48a530
  void AddCharacterNotice(int value);            // 0x48a590
  void SetHold(int on);                          // 0x48a610

  int m_side;            // +0x00
  void* m_advisor;       // +0x04 the advisor droid of the side (set by the human player)
  PersistentInt m_flags; // +0x08 bit 1/2: dialog waiting, 8: idle wait, 0x10000000: held, 0x20000000: dialog shown
  PersistentInt m_defaultState; // +0x0c
  UiRecordTree m_trees[4]; // +0x10 [0]: notices of the message index, [1]: dialogs, [2]: report windows, [3]: saved
  int m_unk40;             // +0x40
  std::map<uint32_t, int> m_states; // +0x44 per-key state values (see GetStateValue)
  int m_unk50;                      // +0x50 the highlighted message index tabs
};

// The base of the controllers (VT 0x65b070, 0x30 bytes, events/player_queue.cpp): the messages the side has received
// from the other side, delivered in id order, and the sync message the side currently has out. Slots 3-6, 8-10, 12 and
// 14 are pure.
class PlayerQueue
{
  public:
  PlayerQueue();                    // 0x48a660
  explicit PlayerQueue(int side);   // 0x48a6e0
  virtual ~PlayerQueue();           // 0x48a750 (scalar deleting destructor 0x48a6c0)
  virtual void Read(istream* in);   // 1: 0x48a7c0
  virtual void Write(ostream* out); // 2: 0x48a8a0
  virtual int GetKind() = 0;
  virtual int StepInitialization() = 0;
  virtual void SetFirstMessageId(int firstMessageId) = 0;
  virtual int NextMessageId() = 0;
  virtual void QueueReceivedMessage(PlayerMessage* message); // 7: 0x48a920 queues a message
  virtual int RouteOutgoingMessage(PlayerMessage* message) = 0;
  virtual int StepPlayer() = 0;
  virtual void ProcessPlayerFrame() = 0;
  virtual int DeliverNextMessage(); // 11: 0x48a960 delivers the next message, true while the sync is not reached
  virtual int ProcessSyncPoint(unsigned syncId) = 0;
  virtual int ResetQueueSyncState(); // 13: 0x48a9f0
  virtual void RefreshPlayerInterface() = 0;

  MessageTree m_pending;   // +0x04 messages not delivered yet
  PersistentInt m_state;   // +0x14 bit 1: sync sent, 2: sync answered, 4: a sync message was delivered
  int m_syncMessageId;     // +0x18 id of the sync message that is out (0 for none)
  unsigned m_deliveredId;  // +0x1c id of the last message delivered
  MessageBase* m_sync;     // +0x20 the sync message that is out (a NetMsg1), owned
  int m_side;              // +0x24 1 or 2
  unsigned m_syncId;       // +0x28
  unsigned m_maxSyncPoint; // +0x2c the highest sync point any queued sync message names
};

// Abstract controller of one side of a game session. A virtual slot is vtable offset / 4.
class PlayerController : public PlayerQueue
{
  public:
  PlayerController();                  // 0x488460
  explicit PlayerController(int side); // 0x4884f0
  ~PlayerController() override;        // 0x488580
  void Read(istream* in) override;     // 1: 0x488650
  void Write(ostream* out) override;   // 2: 0x4886d0
  int GetKind() override
  {
    return 0;
  } // PlayerKind: 1 human, 2 remote, 3 AI (the original's slots 3, 4, 9, 10, 14 and 17-94 are pure)
  int StepInitialization() override
  {
    return 0;
  }
  void SetFirstMessageId(int firstMessageId) override; // 5: 0x4885e0
  int NextMessageId() override;                        // 6: 0x4885f0 the next message or sync id
  int RouteOutgoingMessage(PlayerMessage*) override
  {
    return 0;
  } // 8, shared 0x48b440
  int StepPlayer() override
  {
    return 0;
  }
  void ProcessPlayerFrame() override {}
  int ProcessSyncPoint(unsigned syncId) override; // 12: 0x488750
  void RefreshPlayerInterface() override {}
  virtual int GetVoteWithTime(int* outId, ObjectKey* outKey, GameTime* outTime); // returns the vote state
  virtual int GetVote(int* outId, ObjectKey* outKey);                            // returns the vote state
  virtual void BeginVote(int id, ObjectKey* key) {}
  virtual void ForwardVoteResponse(int id, ObjectKey* key, int state, GameTime* time) {}
  virtual void ResetVote(int id, ObjectKey* key) {}
  virtual void HandleNotification(NotifPayload* notice) {}
  virtual void AdvanceOrderBudget() {}
  virtual void SetAdvisorAnimationStart(int a, int b) {}
  virtual void SetRawMaterials(int value) {}
  virtual void SetRefinedMaterials(int value) {}
  virtual void UpdateMaintenanceAndAdvisorState(intptr_t value) {}
  virtual void ClearAdvisorStateAndUpdateMaintenance(int value) {}
  virtual void OnWaitingRefineriesNotified(int value) {}
  virtual void OnWaitingManufacturersNotified(int value) {}
  virtual void HandleAdvisorFlags(unsigned flags) {}
  virtual void OnLoyaltyNotified(const ObjectKey* key, int arg) {}
  virtual void OnEnergyNotified(const ObjectKey* key, int arg) {}
  virtual void OnEnergyAllocatedNotified(const ObjectKey* key, int arg) {}
  virtual void OnRawMaterialNotified(const ObjectKey* key, int arg) {}
  virtual void OnRawMaterialAllocatedNotified(const ObjectKey* key, int arg) {}
  virtual void OnSmugglingPercentNotified(const ObjectKey* key, int arg) {}
  virtual void OnProductionModifierNotified(const ObjectKey* key, int arg) {}
  virtual void OnPopulatedNotified(const ObjectKey* key, int arg) {}
  virtual void OnExploredNotified(const ObjectKey* key, int arg) {}
  virtual void OnUprisingNotified(const ObjectKey* key, int arg) {}
  virtual void OnNeverBeenControlledNotified(const ObjectKey* key, int arg) {}
  virtual void OnBattleNotified(const ObjectKey* key, int arg) {}
  virtual void OnBlockadeNotified(const ObjectKey* key, int arg) {}
  virtual void OnTargetRequested(const ObjectKey* key, int sound) {}
  virtual void OnTargetRequestBlocked(const ObjectKey* key, int sound) {}
  virtual void OnResourceIncidentNotified(const ObjectKey* key, int arg) {}
  virtual void OnDisasterIncidentNotified(const ObjectKey* key, int arg) {}
  virtual void OnInformantIncidentNotified(const ObjectKey* key, int arg) {}
  virtual void OnUprisingIncidentNotified(const ObjectKey* key, int arg) {}
  virtual void OnControlKindUprisingNotified(const ObjectKey* key, int arg) {}
  virtual void OnTroopRegRequiredNotified(const ObjectKey* key, int arg) {}
  virtual void OnManagerProductionChanged(const ObjectKey* manager, int unused) {}
  virtual void OnManagerProgressChanged(const ObjectKey* manager, int position) {}
  virtual void OnManagerCharacterChanged(const ObjectKey* manager, const ObjectKey* character) {}
  virtual void OnManagerStatusTextChanged(const ObjectKey* manager, std::string* text) {}
  virtual void OnManagerProductKeyChanged(const ObjectKey* key, const ObjectKey* first, const ObjectKey* second) {}
  virtual void OnManagerRequiredPointsChanged(const ObjectKey* key, int arg) {}
  virtual void OnManagerSeedKeyChanged(const ObjectKey* key, const ObjectKey* other) {}
  virtual void OnManagerReservationChanged(const ObjectKey* key, int arg) {}
  virtual void OnFacilityTierChanged(int a, int b) {}
  virtual void OnTroopTierChanged(int a, int b) {}
  virtual void OnCapitalShipTierChanged(int a, int b) {}
  virtual void OnFleetAssaultChanged(const ObjectKey* key, int redraw) {}
  virtual void OnFleetBattleChanged(const ObjectKey* key, int redraw) {}
  virtual void OnFleetBlockadeChanged(const ObjectKey* key, int arg) {}
  virtual void OnFleetBombardChanged(const ObjectKey* key, int redraw) {}
  virtual void OnShipHullChanged(const ObjectKey* key, int arg) {}
  virtual void OnShipShieldRechargeDamageChanged(const ObjectKey* key, int arg) {}
  virtual void OnShipWeaponRechargeDamageChanged(const ObjectKey* key, int arg) {}
  virtual void OnShipTractorBeamDamageChanged(const ObjectKey* key, int arg) {}
  virtual void OnShipSpeedDamageChanged(const ObjectKey* key, int arg) {}
  virtual void OnShipPrimaryHyperdriveDamageChanged(const ObjectKey* key, int arg) {}
  virtual void OnShipBackupHyperdriveDamageChanged(const ObjectKey* key, int arg) {}
  virtual void OnSquadronSizeDamageChanged(const ObjectKey* key, int arg) {}
  virtual void OnMissionEvent(const ObjectKey* key, int arg) {}
  virtual void OnPlannerEvent(const ObjectKey* key, int arg) {}
  virtual void OnObjectAndSystemChanged(const ObjectKey* key, intptr_t arg, const ObjectKey* system) {}
  virtual void OnEnrouteActiveNotified(const ObjectKey* key, int arg) {}
  virtual void OnForceNotified(const ObjectKey* key, int arg) {}
  virtual void OnStateNotified(const ObjectKey* key, int arg) {}
  virtual void OnCharacterChanged(const ObjectKey* key, int arg) {}
  virtual void OnMissionAssignmentChanged(const ObjectKey* key, const ObjectKey* oldKey, const ObjectKey* newKey) {}
  virtual void OnObjectChanged(const ObjectKey* key) {}
  virtual void OnObjectMoved(const ObjectKey* key, const ObjectKey* newParent, const ObjectKey* arg) {}
  virtual void OnSystemChanged(const ObjectKey* key, int arg) {}
  virtual void OnObjectCreated(const ObjectKey* key, int arg) {}
  virtual void OnEnrouteNotified(const ObjectKey* key, int arg) {}
  virtual void NotifyShipChanged(const ObjectKey* key) {}
  virtual void OnUsableNotified(const ObjectKey* key, int arg) {}
  virtual void OnDamagedNotified(const ObjectKey* key, int arg) {}
  virtual void OnObjectDestroyedNotified(const ObjectKey* key) {}
  virtual void OnObjectReparented(const ObjectKey* key, const ObjectKey* newParent) {}
  virtual void OnObjectCombatChanged(intptr_t a, const ObjectKey* key) {}
  virtual void RequestSyncTarget(unsigned target) {}
  virtual void AcknowledgeSync(int side) {}
  virtual void OnSyncFinished(); // 0x4888e0

  void Send(MessageBase* message);           // 0x48aa90: gives the message an id if it has none and sends it
  void SendKeyRequest(const ObjectKey* key); // 0x48aa00
  int IsSynced();                            // 0x4888c0
  void SetSyncTarget(unsigned target);       // 0x4888d0

  int m_lastMessageId;   // +0x30 one less than m_nextMessageId after SetFirstMessageId
  int m_nextMessageId;   // +0x34 the id of the next message this controller sends
  int m_voteState;       // +0x38 0 none, 1 accepted, 2 declined, 3 confirmed (returned by GetVoteWithTime/GetVote)
  GameTime m_voteTime;   // +0x3c
  ObjectKey m_voteKey;   // +0x40
  int m_voteId;          // +0x44
  unsigned m_syncTarget; // +0x48 non-zero while waiting for the other side to reach this sync point
  int m_unk4c;           // +0x4c
};

#include "strategy_ui/human_player.h"
