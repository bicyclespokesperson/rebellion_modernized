#pragma once
#include <cstdint>

#include "ai/ai_side_controller.h"
#include "ai/ai_task_manager.h"
#include "app/player_controller_decl.h"

// The computer player of one side (0x4856f0-0x486120, VT 0x65abf0, 0x42c bytes). It embeds the strategic model of the side
// (AiSideController at +0x84) and runs the side's tasks: after the model is built (Virtual04), every call of Virtual09 lets one
// task produce orders (sent with the player controller's message path) or lets the model refresh its figures. The game's
// notifications (slots 25-92) are forwarded to the model, which marks the records they concern.
class AiPlayer : public PlayerController {
public:
    AiPlayer();                      // 0x4856f0
    explicit AiPlayer(int side);     // 0x4857a0
    virtual ~AiPlayer();             // 0x485780 (scalar deleting destructor), 0x485850

    virtual int GetKind() override { return kPlayerAi; }  // 3: 0x5a6730 (shared)
    virtual void Read(istream* in) override;               // 1: 0x4858c0
    virtual void Write(ostream* out) override;            // 2: 0x485990
    // 4: builds the model (m_state 1) and creates the 14 tasks (2); 1 when the player is ready
    virtual int Virtual04() override;                      // 0x485a50
    // 9: one step of play (m_state 3: a task, 4 and 5: the model's Analyze and the tasks' Evaluate); 1 when a task ran
    virtual int Virtual09() override;                      // 0x485b40
    virtual int Virtual08(PlayerMessage*) override { return 1; }  // 8: shared 0x51ebb0 (mov eax,1; ret 4)
    virtual void Virtual10() override;                     // 10: 0x485cf0 (deletes the first finished group of notes)
    virtual void Virtual17(int id, ObjectKey* key) override;   // 0x485d50
    virtual void Virtual19(int id, ObjectKey* key) override;   // 0x485db0
    virtual void Virtual20(PlayerNotice* notice) override;     // 0x485d10
    virtual void Virtual21() override;                         // 0x485de0 (counts a turn of the order budget)

    virtual void Virtual25(intptr_t value) override;   // 0x485e60: NoteValueA (a pointer to the maintenance pair in the original)
    virtual void Virtual26(int value) override;   // 0x485e80: NoteValueB
    virtual void Virtual29(unsigned flags) override;  // 0x485ea0: NoteMask

    // 0x485ec0: the notifications that only mark the record of the object (arguments: the object, one more that is ignored)
    virtual void Virtual30(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual31(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual32(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual33(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual34(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual37(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual38(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual39(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual40(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual41(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual42(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual43(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual44(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual45(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual46(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual47(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual48(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual49(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual50(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual51(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual66(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual67(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual68(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual69(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual70(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual71(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual72(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual73(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual77(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual78(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual79(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual86(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual88(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual89(const ObjectKey* key, int) override { m_model.NoteObject(key); }
    virtual void Virtual81(const ObjectKey* key, const ObjectKey*, const ObjectKey*) override { m_model.NoteObject(key); }
    virtual void Virtual82(const ObjectKey* key) override { m_model.NoteObject(key); }
    virtual void Virtual90(const ObjectKey* key) override { m_model.NoteObject(key); }

    virtual void Virtual59(int unused, int tier) override { m_model.NoteFacilityTier(unused, tier); }       // 0x485ee0
    virtual void Virtual60(int unused, int tier) override { m_model.NoteTroopTier(unused, tier); }          // 0x485f00
    virtual void Virtual61(int unused, int tier) override { m_model.NoteCapitalShipTier(unused, tier); }    // 0x485f20
    // 0x485f40: only the notes that ask for a redraw
    virtual void Virtual62(const ObjectKey* key, int redraw) override { if (redraw != 0) m_model.NoteObject(key); }
    virtual void Virtual63(const ObjectKey* key, int redraw) override { if (redraw != 0) m_model.NoteObject(key); }
    virtual void Virtual65(const ObjectKey* key, int redraw) override { if (redraw != 0) m_model.NoteObject(key); }
    virtual void Virtual74(const ObjectKey* key, int argument) override;   // 0x485f60: NotePlannerSlot12
    virtual void Virtual75(const ObjectKey* key, int argument) override;   // 0x485f80: NotePlannerSlot3
    virtual void Virtual76(const ObjectKey* key, intptr_t unused, const ObjectKey* system) override;  // 0x485fa0
    virtual void Virtual80(const ObjectKey* key, int) override { m_model.NoteCharacterChanged(key); }   // 0x485fd0
    virtual void Virtual83(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to) override;   // 0x486030: NoteMoved
    virtual void Virtual84(const ObjectKey* key, int) override;                             // 0x486050
    virtual void Virtual87(const ObjectKey* key, StringWords name) override;  // 0x486080
    virtual void Virtual91(const ObjectKey* key, const ObjectKey* newParent) override;      // 0x4860d0
    virtual void Virtual92(intptr_t unused, const ObjectKey* key) override { m_model.NoteCombat(key); }       // 0x486100
    virtual void Virtual93(unsigned target) override;   // 0x485e10
    virtual void Virtual94(int side) override;          // 0x485e30

    int m_state;                  // +0x50 1 building the model, 2 creating the tasks, 3 running a task, 4 analysing, 5 evaluating
    AiNoticeQueue m_notices;      // +0x54
    AiTaskManager m_tasks;        // +0x64
    AiTaskBase* m_currentTask;    // +0x80 the task that is running
    AiSideController m_model;     // +0x84
    int m_tasksOff;               // +0x418 non-zero: the tasks are not evaluated
    int m_turnCount;              // +0x41c turns of the current order budget
    int m_ordersSent;             // +0x420 orders sent in the current budget
    int m_turnsPerBudget;         // +0x424
    int m_ordersPerBudget;        // +0x428 (10 once the tasks exist)
};
