#pragma once
#include <cstdint>

#include "ai/ai_side_controller.h"
#include "ai/ai_task_manager.h"
#include "app/player_controller_decl.h"

// The computer player of one side (0x4856f0-0x486120, VT 0x65abf0, 0x42c bytes). It embeds the strategic model of the
// side (AiSideController at +0x84) and runs the side's tasks: after the model is built (StepInitialization), every call
// of StepPlayer lets one task produce orders (sent with the player controller's message path) or lets the model refresh
// its figures. The game's notifications (slots 25-92) are forwarded to the model, which marks the records they concern.
class AiPlayer : public PlayerController
{
  public:
  AiPlayer();                  // 0x4856f0
  explicit AiPlayer(int side); // 0x4857a0
  ~AiPlayer() override;        // 0x485780 (scalar deleting destructor), 0x485850

  int GetKind() override
  {
    return kPlayerAi;
  } // 3: 0x5a6730 (shared)
  void Read(istream* in) override;   // 1: 0x4858c0
  void Write(ostream* out) override; // 2: 0x485990
  // 4: builds the model (m_state 1) and creates the 14 tasks (2); 1 when the player is ready
  int StepInitialization() override; // 0x485a50
  // 9: one step of play (m_state 3: a task, 4 and 5: the model's Analyze and the tasks' Evaluate); 1 when a task ran
  int StepPlayer() override; // 0x485b40
  int RouteOutgoingMessage(PlayerMessage*) override
  {
    return 1;
  } // 8: shared 0x51ebb0 (mov eax,1; ret 4)
  void ProcessPlayerFrame() override;                     // 10: 0x485cf0 (deletes the first finished group of notes)
  void BeginVote(int id, ObjectKey* key) override;        // 0x485d50
  void ResetVote(int id, ObjectKey* key) override;        // 0x485db0
  void HandleNotification(NotifPayload* notice) override; // 0x485d10
  void AdvanceOrderBudget() override;                     // 0x485de0 (counts a turn of the order budget)

  void UpdateMaintenanceAndAdvisorState(
      intptr_t value) override; // 0x485e60: NoteValueA (a pointer to the maintenance pair in the original)
  void ClearAdvisorStateAndUpdateMaintenance(int value) override; // 0x485e80: NoteValueB
  void HandleAdvisorFlags(unsigned flags) override;               // 0x485ea0: NoteMask

  // 0x485ec0: the notifications that only mark the record of the object (arguments: the object, one more that is
  // ignored)
  void OnLoyaltyNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnEnergyNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnEnergyAllocatedNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnRawMaterialNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnRawMaterialAllocatedNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnPopulatedNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnExploredNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnUprisingNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnNeverBeenControlledNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnBattleNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnBlockadeNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnTargetRequested(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnTargetRequestBlocked(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnResourceIncidentNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnDisasterIncidentNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnInformantIncidentNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnUprisingIncidentNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnControlKindUprisingNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnTroopRegRequiredNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnManagerProductionChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnShipHullChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnShipShieldRechargeDamageChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnShipWeaponRechargeDamageChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnShipTractorBeamDamageChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnShipSpeedDamageChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnShipPrimaryHyperdriveDamageChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnShipBackupHyperdriveDamageChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnSquadronSizeDamageChanged(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnEnrouteActiveNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnForceNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnStateNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnEnrouteNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnUsableNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnDamagedNotified(const ObjectKey* key, int) override
  {
    m_model.NoteObject(key);
  }
  void OnMissionAssignmentChanged(const ObjectKey* key, const ObjectKey*, const ObjectKey*) override
  {
    m_model.NoteObject(key);
  }
  void OnObjectChanged(const ObjectKey* key) override
  {
    m_model.NoteObject(key);
  }
  void OnObjectDestroyedNotified(const ObjectKey* key) override
  {
    m_model.NoteObject(key);
  }

  void OnFacilityTierChanged(int unused, int tier) override
  {
    m_model.NoteFacilityTier(unused, tier);
  } // 0x485ee0
  void OnTroopTierChanged(int unused, int tier) override
  {
    m_model.NoteTroopTier(unused, tier);
  } // 0x485f00
  void OnCapitalShipTierChanged(int unused, int tier) override
  {
    m_model.NoteCapitalShipTier(unused, tier);
  } // 0x485f20
  // 0x485f40: only the notes that ask for a redraw
  void OnFleetAssaultChanged(const ObjectKey* key, int redraw) override
  {
    if (redraw != 0) {
      m_model.NoteObject(key);
    }
  }
  void OnFleetBattleChanged(const ObjectKey* key, int redraw) override
  {
    if (redraw != 0) {
      m_model.NoteObject(key);
    }
  }
  void OnFleetBombardChanged(const ObjectKey* key, int redraw) override
  {
    if (redraw != 0) {
      m_model.NoteObject(key);
    }
  }
  void OnMissionEvent(const ObjectKey* key, int argument) override; // 0x485f60: OnMissionEvent
  void OnPlannerEvent(const ObjectKey* key, int argument) override; // 0x485f80: OnPlannerEvent
  void OnObjectAndSystemChanged(const ObjectKey* key, intptr_t unused, const ObjectKey* system) override; // 0x485fa0
  void OnCharacterChanged(const ObjectKey* key, int) override
  {
    m_model.NoteCharacterChanged(key);
  } // 0x485fd0
  void OnObjectMoved(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to) override; // 0x486030: NoteMoved
  void OnSystemChanged(const ObjectKey* key, int) override;                                      // 0x486050
  void NotifyShipChanged(const ObjectKey* key) override;                                         // 0x486080
  void OnObjectReparented(const ObjectKey* key, const ObjectKey* newParent) override;            // 0x4860d0
  void OnObjectCombatChanged(intptr_t unused, const ObjectKey* key) override
  {
    m_model.NoteCombat(key);
  } // 0x486100
  void RequestSyncTarget(unsigned target) override; // 0x485e10
  void AcknowledgeSync(int side) override;          // 0x485e30

  int m_state; // +0x50 1 building the model, 2 creating the tasks, 3 running a task, 4 analysing, 5 evaluating
  AiNoticeQueue m_notices;   // +0x54
  AiTaskManager m_tasks;     // +0x64
  AiTaskBase* m_currentTask; // +0x80 the task that is running
  AiSideController m_model;  // +0x84
  int m_tasksOff;            // +0x418 non-zero: the tasks are not evaluated
  int m_turnCount;           // +0x41c turns of the current order budget
  int m_ordersSent;          // +0x420 orders sent in the current budget
  int m_turnsPerBudget;      // +0x424
  int m_ordersPerBudget;     // +0x428 (10 once the tasks exist)

  protected:
  virtual bool AcceptTaskOrder(OrderBase*)
  {
    return true;
  }
};
