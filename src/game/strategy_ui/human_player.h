#pragma once
#include <cstdint>

#include "app/player_controller_decl.h"
#include "ai/ai_notice_queue.h"
#include "gameobj/object_key.h"

class AdvisorBase;
class ObjectKeyValueSet;
class OrderBase;
class EventKindSet;
class GameObjNotifier;
class String;

// The local (human) controller of a side (0xc4 bytes, VT 0x65ad70, ctors 0x486120 / 0x4861b0): it turns the player's UI
// commands into orders, sends them (directly, or through the dispatch queue when a blockade is involved) and forwards
// the game's notifications to the UI and to the advisor droid (m_advisor, an object of the side's advisor class).
// Slots 22 and up are the notification handlers: most only refresh the UI through the game UI facade.
class HumanPlayer : public PlayerController {
public:
    HumanPlayer();                  // 0x486120
    explicit HumanPlayer(int side);  // 0x4861b0
    ~HumanPlayer() override;         // 0x4862b0 (scalar deleting destructor 0x486190)

    int GetKind() override { return kPlayerHuman; }  // slot 3, 0x5a66f0 (shared)
    void Read(istream* in) override;    // slot 1, 0x486330
    void Write(ostream* out) override;  // slot 2, 0x486440
    int Virtual04() override;           // slot 4, 0x486490
    void Virtual07(PlayerMessage* message) override;  // slot 7, 0x487f40
    int Virtual08(PlayerMessage* message) override;   // slot 8, 0x4864d0
    int Virtual09() override;                          // slot 9, 0x486500
    void Virtual10() override;                         // slot 10, 0x486520
    void Virtual14() override;                         // slot 14, 0x4866b0
    void Virtual17(int id, ObjectKey* key) override;   // slot 17, 0x4865b0
    void Virtual18(int id, ObjectKey* key, int state, GameTime* time) override;  // slot 18, 0x486650
    void Virtual19(int id, ObjectKey* key) override;   // slot 19, 0x486670
    void Virtual20(NotifPayload* notice) override;     // slot 20, 0x486580
    void Virtual21() override;                         // slot 21, 0x4866e0
    void Virtual22(int a, int b) override;             // 0x486710
    void Virtual23(int value) override;                // 0x486730: raw materials
    void Virtual24(int value) override;                // 0x486760: refined materials
    void Virtual25(intptr_t value) override;                // 0x486790: maintenance, advisor
    void Virtual26(int value) override;                // 0x486800
    void Virtual27(int value) override;                // 0x486870 (shared)
    void Virtual28(int value) override { HumanPlayer::Virtual27(value); }  // slot 28, shared 0x486870
    void Virtual29(unsigned flags) override;           // 0x486890
    void Virtual30(const ObjectKey* key, int arg) override;  // 0x4868b0
    void Virtual31(const ObjectKey* key, int arg) override;  // 0x4868f0
    void Virtual32(const ObjectKey* key, int arg) override;  // 0x486920
    void Virtual33(const ObjectKey* key, int arg) override;  // 0x486f00 (shared by slots 33, 34, 37-42, 50, 84, 86, 88, 89)
    void Virtual34(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 34, shared 0x486f00
    void Virtual37(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 37, shared 0x486f00
    void Virtual38(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 38, shared 0x486f00
    void Virtual39(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 39, shared 0x486f00
    void Virtual40(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 40, shared 0x486f00
    void Virtual41(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 41, shared 0x486f00
    void Virtual42(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 42, shared 0x486f00
    void Virtual50(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 50, shared 0x486f00
    void Virtual84(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 84, shared 0x486f00
    void Virtual86(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 86, shared 0x486f00
    void Virtual88(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 88, shared 0x486f00
    void Virtual89(const ObjectKey* key, int arg) override { HumanPlayer::Virtual33(key, arg); }  // slot 89, shared 0x486f00
    void Virtual43(const ObjectKey* key, int sound) override;  // 0x486950: plays sound 0x26d
    void Virtual44(const ObjectKey* key, int sound) override;  // 0x4869f0: plays sound 0x26c
    void Virtual45(const ObjectKey* key, int arg) override;    // 0x486a90 (shared by slots 45-49, 58, 66-73)
    void Virtual46(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 46, shared 0x486a90
    void Virtual47(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 47, shared 0x486a90
    void Virtual48(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 48, shared 0x486a90
    void Virtual49(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 49, shared 0x486a90
    void Virtual58(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 58, shared 0x486a90
    void Virtual66(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 66, shared 0x486a90
    void Virtual67(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 67, shared 0x486a90
    void Virtual68(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 68, shared 0x486a90
    void Virtual69(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 69, shared 0x486a90
    void Virtual70(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 70, shared 0x486a90
    void Virtual71(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 71, shared 0x486a90
    void Virtual72(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 72, shared 0x486a90
    void Virtual73(const ObjectKey* key, int arg) override { HumanPlayer::Virtual45(key, arg); }  // slot 73, shared 0x486a90
    void Virtual51(const ObjectKey* manager, int unused) override;        // 0x486ab0
    void Virtual52(const ObjectKey* manager, int position) override;      // 0x486ae0
    void Virtual53(const ObjectKey* manager, const ObjectKey* character) override;  // 0x486b00
    void Virtual54(const ObjectKey* manager, String* text) override;       // 0x486b20
    void Virtual62(const ObjectKey* key, int redraw) override;             // 0x486b40 (shared by slots 62, 63, 65)
    void Virtual63(const ObjectKey* key, int redraw) override { HumanPlayer::Virtual62(key, redraw); }  // slot 63, shared 0x486b40
    void Virtual65(const ObjectKey* key, int redraw) override { HumanPlayer::Virtual62(key, redraw); }  // slot 65, shared 0x486b40
    void Virtual80(const ObjectKey* key, int arg) override;                // 0x486d30
    void Virtual81(const ObjectKey* key, const ObjectKey* oldKey, const ObjectKey* newKey) override;  // 0x486b60
    void Virtual82(const ObjectKey* key) override;                          // 0x486d50
    void Virtual83(const ObjectKey* key, const ObjectKey* newParent, const ObjectKey* arg) override;  // 0x486d80
    void Virtual87(const ObjectKey* key, StringWords name) override;  // 0x486eb0
    void Virtual90(const ObjectKey* key) override;                          // 0x486f30
    void Virtual91(const ObjectKey* key, const ObjectKey* newParent) override;  // 0x486f60
    void Virtual93(unsigned target) override { SetSyncTarget(target); }  // slot 93, shared 0x4888d0
    void Virtual94(int side) override;                                      // 0x4866f0
    void Virtual95() override;                                              // 0x4883d0

    // Executes the UI command `command` for the objects of `selection` (and `key`, the object a target was picked
    // on; `name`, the new name of a rename).
    void ExecuteCommand(int command, ObjectKeyValueSet* selection, ObjectKey* key, String* name);  // 0x486fb0
    // Checks `order` and sends it (to the other side and the game) or gives the advisor the reason it is refused.
    void SubmitOrder(PlayerMessage* order, int flag);                                              // 0x487740
    void NotifyAdvisor(const ObjectKey* key);                                                                // 0x487810
    // Sets this side's answer to a vote (`choice` 0: declined, else accepted; `timed`: with a timeout).
    void SetVote(int choice, int timed, PlayerMessage* reply);                                     // 0x487830
    void ConfirmVote(PlayerMessage* reply);                                                        // 0x487890
    void SubmitMessage(PlayerMessage* message);                                                    // 0x4878f0
    void FillCommandMenu(EventKindSet* commands);                                                  // 0x487900
    void FillOrderMenu(EventKindSet* commands);                                                    // 0x487a60
    void AdvisorReset();                                                                           // 0x487c10
    uint32_t GetAdvisorCommandMask();                                                              // 0x487c20
    uint32_t GetAdvisorOrderMask();                                                                // 0x487c30
    // A new order of `kind` for this side with `team` and `target`; null if there is no such order.
    OrderBase* CreateOrder(int kind, ObjectKeyValueSet* team, ObjectKey* target);                  // 0x487c50
    void RefuseOrder(PlayerMessage* order, IntPair* status);                                       // 0x487c90
    // True if `order` has to be sent through the dispatch queue.
    int IsQueuedOrder(PlayerMessage* order);                                                       // 0x487cc0
    // Sets the game speed 1 (very slow) .. 4 (fast) and tells the other side.
    void SetSpeed(int speed);                                                                      // 0x487eb0
    int HasPendingMessages();                                                                      // 0x487f30
    void SetAdvisorFlag4000(int on);                                                               // 0x487f80
    int NextAdvice();                                                                              // 0x487fb0
    void ClearAlerts(unsigned mask);                                                               // 0x487fd0
    void SetPaused(int paused);                                                                    // 0x487ff0
    // Speaks the advisor's line for the order about to be sent.
    void SpeakOrderAdvice(PlayerMessage* order);                                                   // 0x488030

    int m_unk50;                  // +0x50
    int m_speed;                  // +0x54 the game speed (1-4)
    int m_pausedSpeed;            // +0x58 the speed to return to after a pause, 0 while not paused
    AiNoticeQueue m_alertList;  // +0x5c list built by 0x4bee50
    alignas(4) uint8_t m_dispatchStorage[sizeof(PlayerDispatch)];  // +0x6c the PlayerDispatch, built and destroyed by the original's functions
    AdvisorBase* m_advisor;       // +0xc0 advisor droid (AllianceAdvisor / EmpireAdvisor)

    PlayerDispatch* Dispatch() { return reinterpret_cast<PlayerDispatch*>(m_dispatchStorage); }
};

// Static helper of the human player: HumanSetUiTarget forwards to AdvisorSetPendingCommand (0x439eb0). The static
// thunks 0x4878b0 / 0x4878c0 jump to AdvisorAllowSpeech / AdvisorSetFlag0.
void HumanSetUiTarget(int a, int b);  // 0x4878d0
