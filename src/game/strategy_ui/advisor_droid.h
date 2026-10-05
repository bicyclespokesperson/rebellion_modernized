#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/status_code.h"

class HumanPlayer;
class OrderBase;
class SidedMessage;
class PlayerDispatch;

// An entry of the advisor's task tree (IdValueNode at +0x1c: its state, 2 = ready). The concrete task classes belong
// to the AI helper module (0x49c6a0-0x49ecb0); the advisor only drives them through these virtuals.
class AdvisorTask : public IdValueNode {
public:
    virtual void Virtual03() = 0;
    virtual void Advance() = 0;                                              // slot 4
    virtual SidedMessage* NextOrder() = 0;                                   // slot 5: the order to give, or null
    virtual void OnOrderRefused(IntPair* status, SidedMessage* order) = 0;  // slot 6

    int m_done;          // +0x20 non-zero when the task has nothing more to do
    uint8_t m_unk24[0xc];  // +0x24
    int m_unk30;         // +0x30 cleared before each NextOrder
    uint8_t m_unk34[0x20];  // +0x34
    class AdvisorBase* m_advisor;  // +0x54
};

// The advisor droid of a side (0x194 bytes, VT 0x658e10): the character that talks to the player (C-3PO / R2-D2
// style scripts), owned by the human player (m_player, +0x144). The base class holds the task machinery and the
// save/load code; the two side classes (AllianceAdvisor VT 0x65c4c8, EmpireAdvisor VT 0x65c4a0) own the tables of
// "cue" timers: m_cueDue[cue] is the tick until which the raised cue is still current (0: not raised).
//
// The class-wide state (what the original keeps in globals at 0x6b28b0-0x6b28d4) is shared by both sides.
class AdvisorBase {
public:
    AdvisorBase(int side, HumanPlayer* player);  // 0x439320
    virtual ~AdvisorBase();                      // 0x439490 (scalar deleting destructor 0x439300)

    virtual void Read(istream* in, HumanPlayer* player);  // slot 1, 0x439550
    virtual void Write(ostream* out);                     // slot 2, 0x4397a0
    // The player's order `order` was refused for the reason `status` ({category, code}).
    virtual void HandleRefusedOrder(OrderBase* order, IntPair* status) = 0;  // slot 3
    // `flags` are the game's UI request bits (Virtual29 of the human player).
    virtual void HandleUiFlags(unsigned flags) = 0;  // slot 4
    // Schedules the droid's reaction to cue `cue` (the table of the side's own cues).
    virtual void ScheduleCue(int cue) = 0;           // slot 5
    // Plays the raised cues; returns 1.
    virtual int Update() = 0;                        // slot 6
    // Picks the id of a cue by the droid's mood (m_mood).
    virtual int PickMoodCue() = 0;                   // slot 7
    // Loads the cue scripts from the data file; true if it was read.
    virtual int LoadCueScripts() = 0;                // slot 8

    // Non-virtual members called by the human player and the UI.
    int StepStartup();                      // 0x439950
    int StepTasks();                        // 0x439a10
    int Tick();                             // 0x439bb0: Update() through the vtable
    void SpeakIfIdle();                     // 0x439bc0
    void ResetSpeech();                     // 0x439cb0
    int IsBusy();                           // 0x439cd0
    void SetTaskState(int value);           // 0x439cf0
    void SelectTaskMode(int command);       // 0x439d10
    void ToggleTask(unsigned command);      // 0x439d60
    int IsTaskReady(int command);           // 0x439e30
    void FlushPending();                    // 0x439f20
    void SpeakNow();                        // 0x439fb0
    void SpeakTopic(unsigned topic);        // 0x43a0b0
    void SetAiHelperFlag(int value);        // 0x43a210
    void ClearAiHelperFlag(int value);      // 0x43a220
    void ResetTasks();                      // 0x43a230
    int PickAdvice();                       // 0x43a250
    void SetRefusedOrderKind(int kind);     // 0x43a290
    void OnOrderSubmitted(int kind);        // 0x43a2a0

    uint32_t& CueDue(unsigned byteOffset) { return *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(m_cueDue) + byteOffset); }

    IdAvlTree* Tasks() { return reinterpret_cast<IdAvlTree*>(m_taskTree); }
    IdAvlTree* Cues() { return reinterpret_cast<IdAvlTree*>(m_cueTree); }
    void* AiHelper() { return m_aiHelper; }

    int m_unk04;                    // +0x04 1
    PlayerDispatch* m_dispatch;     // +0x08 set by the human player
    int m_state;                    // +0x0c start-up / update state machine (1..6)
    int m_enabled;                  // +0x10 the droid is switched on
    int m_side;                     // +0x14
    alignas(4) uint8_t m_taskTree[0x1c];  // +0x18 AI helper task tree (ctor 0x49dcf0)
    AdvisorTask* m_currentTask;     // +0x34 the task being stepped by StepTasks
    int m_unk38;                    // +0x38
    alignas(4) uint8_t m_aiHelper[0xfc];  // +0x3c AI helper object (ctor 0x49c780)
    alignas(4) uint8_t m_cueTree[0xc];    // +0x138 tree of cue scripts (ctor 0x49c6a0)
    HumanPlayer* m_player;          // +0x144
    uint32_t m_lastSpeechTime;      // +0x148
    int m_unk14c;                   // +0x14c 8
    PersistentInt m_flags;          // +0x150 bit 0x10000000: the pending notices were collected; low bits: topics already told
    IdValueTree m_notices;          // +0x154 notices waiting to be told (nodes: +0x1c & 0xf = topic)
    int m_unk160;                   // +0x160
    uint32_t m_cueCount;            // +0x164
    uint32_t* m_cueDue;             // +0x168 per cue: the tick until which it is raised (0 = not raised)
    uint32_t* m_cueCooldown;        // +0x16c per cue: the tick before which it may not run again
    uint8_t m_mood;                 // +0x170
    IntPair m_lastStatus;           // +0x174 the last refusal reason that was reacted to
    int m_statusRepeat;             // +0x17c
    int m_refusedKind;              // +0x180
    AdvisorTask* m_primaryTask;     // +0x184 the task with id 0x17
    int m_unk188;                   // +0x188
    int m_unk18c;                   // +0x18c
    int m_unk190;                   // +0x190
};

class AllianceAdvisor : public AdvisorBase {
public:
    AllianceAdvisor(int side, HumanPlayer* player);  // 0x4c27f0
    virtual ~AllianceAdvisor();                      // 0x4c28d0 (scalar deleting destructor 0x4c27d0)

    virtual void HandleRefusedOrder(OrderBase* order, IntPair* status) override;  // 0x4c2940
    virtual void HandleUiFlags(unsigned flags) override;                          // 0x4c3010
    virtual void ScheduleCue(int cue) override;                                   // 0x4c44b0
    virtual int Update() override;                                                // 0x4c2b20
    virtual int PickMoodCue() override;                                           // 0x4c48e0
    virtual int LoadCueScripts() override;                                        // 0x4c2c70

    void ProcessIdle();                                                // 0x4c2dd0
    void RunPendingCommand();                                          // 0x4c3060
    void RunCommand(int command);                                      // 0x4c30c0
    void RefusedSystem(OrderBase* order, IntPair* status);             // 0x4c3750
    void RefusedManager(OrderBase* order, IntPair* status);            // 0x4c3860
    void RefusedFleet(OrderBase* order, IntPair* status);              // 0x4c3920
    void RefusedShip(OrderBase* order, IntPair* status);               // 0x4c3930
    void RefusedSide(OrderBase* order, IntPair* status);               // 0x4c3b40
    void RefusedOther(OrderBase* order, IntPair* status);              // 0x4c3bb0
    void ScheduleRandomPair();                                         // 0x4c4390
    void ToggleRepeatedStatus(IntPair* status);                        // 0x4c43d0
    void ScheduleRandomQuad();                                         // 0x4c4430
    void ScheduleRandomPairAt124();                                    // 0x4c4480
};

class EmpireAdvisor : public AdvisorBase {
public:
    EmpireAdvisor(int side, HumanPlayer* player);  // 0x4c0710
    virtual ~EmpireAdvisor();                      // 0x4c07f0 (scalar deleting destructor 0x4c06f0)

    virtual void Read(istream* in, HumanPlayer* player) override;  // 0x4c0850 (shared by both sides)
    virtual void Write(ostream* out) override;                     // 0x4c2930 (shared by both sides)
    virtual void HandleRefusedOrder(OrderBase* order, IntPair* status) override;  // 0x4c0870
    virtual void HandleUiFlags(unsigned flags) override;                          // 0x4c0f10
    virtual void ScheduleCue(int cue) override;                                   // 0x4c22f0
    virtual int Update() override;                                                // 0x4c0a60
    virtual int PickMoodCue() override;                                           // 0x4c2700
    virtual int LoadCueScripts() override;                                        // 0x4c0ba0

    void ProcessIdle();                                                // 0x4c0d00
    void RunPendingCommand();                                          // 0x4c0f60
    void RunCommand(int command);                                      // 0x4c0fc0
    void RefusedSystem(OrderBase* order, IntPair* status);             // 0x4c1550
    void RefusedManager(OrderBase* order, IntPair* status);            // 0x4c16a0
    void RefusedFleet(OrderBase* order, IntPair* status);              // 0x4c1760
    void RefusedShip(OrderBase* order, IntPair* status);               // 0x4c1770
    void RefusedSide(OrderBase* order, IntPair* status);               // 0x4c1980
    void RefusedOther(OrderBase* order, IntPair* status);              // 0x4c19f0
    void ToggleRepeatedStatus(IntPair* status);                        // 0x4c21d0
    void ScheduleRandomPair();                                         // 0x4c2230
    void ScheduleRandomQuad();                                         // 0x4c2270
    void ScheduleRandomPairAt118();                                    // 0x4c22c0
};

// The class-wide advisor state.
// Statics of the advisor module.
void AdvisorInitStatics();                    // 0x4392b0
void AdvisorInitFlags();                      // 0x4392c0
void AdvisorRegisterFlagsAtExit();            // 0x4392d0
void AdvisorToggleUiFlag(int command);        // 0x439e80
void AdvisorSetPendingCommand(int mode, int argument);  // 0x439eb0
void AdvisorNoteNag();                        // 0x439ed0
void AdvisorAllowSpeech();                    // 0x439ef0
void AdvisorSetFlag0();                       // 0x43a1f0
void AdvisorRequestIdle();                    // 0x43a200

extern PersistentInt g_humanUiFlags;  // 0x6b28b0 bit set of what the human just did / the droid should react to
extern int g_advisorNagCount;         // 0x6b28b8
extern int g_advisorMaySpeak;         // 0x6b28bc
extern int g_advisorFlag0;            // 0x6b28c0
extern int g_advisorPendingMode;      // 0x6b28c4 1: nothing, 2: run command g_advisorPendingArg, 3: select
extern int g_advisorPendingArg;       // 0x6b28c8
extern uint32_t g_advisorNow;         // 0x6b28cc the tick of the current update
extern uint32_t g_advisorLastRun;     // 0x6b28d0
extern int g_advisorFlushAll;         // 0x6b28d4

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AdvisorTask, 0x58);
REB_ASSERT_OFFSET(AdvisorTask, m_advisor, 0x54);
REB_ASSERT_SIZE(AdvisorBase, 0x194);
REB_ASSERT_OFFSET(AdvisorBase, m_state, 0x0c);
REB_ASSERT_OFFSET(AdvisorBase, m_taskTree, 0x18);
REB_ASSERT_OFFSET(AdvisorBase, m_currentTask, 0x34);
REB_ASSERT_OFFSET(AdvisorBase, m_aiHelper, 0x3c);
REB_ASSERT_OFFSET(AdvisorBase, m_cueTree, 0x138);
REB_ASSERT_OFFSET(AdvisorBase, m_player, 0x144);
REB_ASSERT_OFFSET(AdvisorBase, m_flags, 0x150);
REB_ASSERT_OFFSET(AdvisorBase, m_notices, 0x154);
REB_ASSERT_OFFSET(AdvisorBase, m_cueCount, 0x164);
REB_ASSERT_OFFSET(AdvisorBase, m_mood, 0x170);
REB_ASSERT_OFFSET(AdvisorBase, m_lastStatus, 0x174);
REB_ASSERT_OFFSET(AdvisorBase, m_primaryTask, 0x184);
REB_ASSERT_SIZE(AllianceAdvisor, 0x194);
REB_ASSERT_SIZE(EmpireAdvisor, 0x194);
#pragma clang diagnostic pop
