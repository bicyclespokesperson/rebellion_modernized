#pragma once
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "tactical/tactical_types.h"

// The 3D battle's object manager (singleton DAT_006bc4a0, accessor 0x5aaf90; abstract base class, VT 0x66c2d0 with 40
// slots, the concrete class lives in the 3D view). It is never created for an auto-resolved battle. The base class
// reconstructed here (0x5a7500-0x5af4b0) keeps the two sides' state, the unit lists and the event handlers of the 3D
// battle.

class TacticalObjectManager;
class TacticalUnit;

// Per-side record (the manager keeps one for each side, at +0x08 and +0x0c).
class TacticalSideData {
public:
    virtual void Slot00();
    virtual void Slot01();
    virtual void Slot02();
    virtual int HasNoShips();  // slot 3 (+0x0c)

    uint8_t m_pad04[4];
    int m_status;           // +0x08 1 while the side is active
    uint8_t m_pad0c[0x20 - 0x0c];
    int m_controlFlag;      // +0x20
    GameTime m_battleTime;  // +0x24 the vote time of this side's controller
    uint8_t m_pad28[0x7c - 0x28];
    float m_valueScaled;    // +0x7c the side's fleet value in milliseconds (set from a value in seconds)
    int m_unk80;            // +0x80
    int m_unk84;            // +0x84
    uint8_t m_pad88[0x94 - 0x88];
};

class TacticalObjectManager {
public:
    virtual void Virtual00();               // 0x5aac30: raises the pause counter (+0x8c8)
    virtual void Virtual01();               // 0x5aac60: lowers it
    virtual void Virtual02();               // 0x5aac10: calls slot 1 when the mode (+0x8b4) is above 2
    virtual void Virtual03();               // 0x5aac20: calls slot 0 when the mode is above 2
    virtual void Virtual04() = 0;
    virtual void Virtual05() = 0;
    virtual void Virtual06() = 0;
    virtual void Virtual07() = 0;
    virtual void Virtual08() = 0;
    virtual void Virtual09();               // 0x5ad430
    virtual void PostEvent(int kind, int unit, int arg, int flag);  // 10 (+0x28), 0x5ad4a0: queues an event (RET 0x10)
    virtual void QueueEvent(void* event, int flag) = 0;  // 11 (+0x2c): queues an event built with 0x5ce500
    virtual void SetPlayerSide(int side) = 0;           // 12
    virtual void Virtual13();               // 0x5a9010
    virtual int Virtual14(void* unit);      // 0x5aada0
    virtual int Virtual15(void* unit);      // 0x5aae10
    virtual void Virtual16(void* unit);     // 0x5aae40
    virtual void* ScalarDeletingDestructor(int flags);  // 17, 0x5a8840
    virtual void Virtual18() = 0;
    virtual void Virtual19() = 0;
    virtual void Virtual20() = 0;
    virtual void Virtual21(TacticalUnit* unit) = 0;     // 21 (+0x54): the facade registers a special force
    virtual void Virtual22(TacticalUnit* unit) = 0;     // 22 (+0x58): the facade registers a character
    virtual void Virtual23() = 0;
    virtual void Virtual24(TacticalUnit* unit) = 0;     // 24 (+0x60): the facade registers a capital ship
    virtual void Virtual25(TacticalUnit* unit) = 0;     // 25 (+0x64): the facade registers a fighter squadron
    virtual void Virtual26() = 0;
    virtual void Virtual27() = 0;
    virtual void Virtual28() = 0;
    virtual void Virtual29() = 0;
    virtual void Virtual30() = 0;
    virtual void Virtual31() = 0;
    virtual int Virtual32(int a, int side);   // 0x5a9d10
    virtual int Virtual33(int a);             // 0x5a9d40
    virtual int Update(float milliseconds);   // 34, 0x5a9de0
    virtual void Virtual35() = 0;
    virtual void Virtual36() = 0;
    virtual void Virtual37() = 0;
    virtual int Virtual38(float a, float b);  // 0x5a9ec0
    virtual void Virtual39() = 0;

    // Adds a registered unit to the world's lists and returns its handle.
    int AddShipToWorld(TacticalUnit* ship);                  // 0x5a9340
    int AddCharacterToWorld(TacticalUnit* character);        // 0x5a93e0
    int AddSpecialForceToWorld(TacticalUnit* force);         // 0x5a9450
    int AddLaunchedFighterToWorld(TacticalUnit* fighter);    // 0x5a9470
    int PlayerSideIsActive();                                // 0x5a74d0
    int PlayerSideCanAct(int side);                          // 0x5a8a90: asks the side's record, 0 when it is not the player's
    int CanPlayerCommandShip(void* ship);                    // 0x5ae120
    int CanPlayerCommandFighter(void* fighter);              // 0x5ae170
    int AnySideHasFlag();                                    // 0x5adde0
    int WorldCheck();                                        // 0x5ad930

    // Fleet value of `side` in seconds (stored in milliseconds in the side record); sides above 1 are ignored.
    void SetSideValue(int side, float seconds);  // 0x5a8b10
    void SetSideFlag(int side, int value);       // 0x5a8af0 (+0x80 of the side record)
    void SetControlFlags(int sideA, int sideB);  // 0x5ae1b0 (+0x20 of both side records)
    // The system's id and name for the battle's backdrop and title.
    void SetSystemInfo(int field44, String name);  // 0x59a850
    unsigned Step(float milliseconds);           // 0x5aacb0
    void ForwardEvent(int a, int b, int c);      // 0x5aafc0
    void ResetView();                            // 0x5ab0e0
    int AddDeathStar(int family, int id, int notCompleted, int hasFacility, float hull, float hullNow, int hangarFree);  // 0x5a9220

    int m_unk04;                        // +0x04
    TacticalSideData* m_sides[2];       // +0x08
    uint8_t m_pad10[0x87c - 0x10];
    uint32_t m_clock;                   // +0x87c the battle clock in milliseconds
    uint8_t m_pad880[0x88c - 0x880];
    int m_impactFlag;                   // +0x88c set when a unit was hit
    int m_unitsChanged;                 // +0x890 set when a unit left the world
    int m_unk894;                       // +0x894
    float m_cameraDistance;             // +0x898
    uint8_t m_pad89c[0x8b0 - 0x89c];
    int m_unk8b0;                       // +0x8b0
    int m_mode;                         // +0x8b4
    uint8_t m_pad8b8[0x8c4 - 0x8b8];
    int m_finished;                     // +0x8c4
    int m_pauseCount;                   // +0x8c8
    uint8_t m_pad8cc[4];
    int m_systemField44;                // +0x8d0
    uint8_t m_pad8d4[0x8dc - 0x8d4];
    String m_systemName;                // +0x8dc
    int m_battleKindNotAlliance;        // +0x8e8
    uint8_t m_pad8ec[0x9ec - 0x8ec];
    int m_playerSide;                   // +0x9ec
    BattleOutcome m_outcome;            // +0x9f0
    uint8_t m_pada0c[0xea0 - 0xa0c];
};

TacticalObjectManager* GetObjectManager();  // 0x5aaf90
void DestroyObjectManager();                // 0x5aafa0
