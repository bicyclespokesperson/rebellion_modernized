#pragma once
// The concrete event/command classes at 0x562c70-0x5638e0 (names by kind; their purpose is read from what they call):
//   LoyaltyEvent373 (VT 0x6632a8)  shifts a system's loyalty by a stored delta toward side 1 (System::AdjustLoyalty)
//   SystemCommand225 (0x6632d8)    the planetary assault: checks are System slots 111/112, execution is AssaultResolver
//   SystemCommand224 (0x663300)    the planetary bombardment: slots 113/114, execution is BombardResolver
// (rules/planet_assault.h and rules/planet_bombard.h, 0x58b960-0x58e570).
#include <cstdint>

#include "events/event_base.h"
#include "gameobj/game_hooks.h"
#include "gameobj/status_code.h"

class LoyaltyEvent373 : public PlainEvent
{
  public:
  LoyaltyEvent373();                             // 0x562c70
  LoyaltyEvent373(const LoyaltyEvent373& other); // 0x562cc0
  ~LoyaltyEvent373() override;                   // 0x562cb0

  LoyaltyEvent373* Clone() override;             // 1: 0x562cf0
  void Read(istream* in) override;               // 2: 0x562de0
  void Write(ostream* out) override;             // 3: 0x562e00
  void Checksum(Crc32* crc) override;            // 4: 0x562e20
  int CheckEnabled(StatusCode* status) override; // 6: 0x562e50
  int CheckValid(StatusCode* status) override;   // 7: 0x562f00
  int Execute() override;                        // 8: 0x562fb0
  int GetKind() override;                        // 9: 0x562d60, 0x373
  int GetPriority() const override
  {
    return 0x100;
  } // 10: 0x48bc60

  int m_delta; // +0x40
};

class SystemCommand225 : public Command
{
  public:
  SystemCommand225();                              // 0x563090
  SystemCommand225(const SystemCommand225& other); // 0x5630e0
  ~SystemCommand225() override;                    // 0x5630d0

  SystemCommand225* Clone() override;            // 1: 0x563100
  void Read(istream* in) override;               // 2: 0x5635f0
  void Write(ostream* out) override;             // 3: 0x563600
  void Checksum(Crc32* crc) override;            // 4: 0x563610
  int CheckEnabled(StatusCode* status) override; // 6: 0x5631f0
  int CheckValid(StatusCode* status) override;   // 7: 0x5632c0
  int Execute() override;                        // 8: 0x563390
  int GetKind() override;                        // 9: 0x563170, 0x225
};

class SystemCommand224 : public Command
{
  public:
  SystemCommand224();                              // 0x563490
  SystemCommand224(const SystemCommand224& other); // 0x5634e0
  ~SystemCommand224() override;                    // 0x5634d0

  SystemCommand224* Clone() override;            // 1: 0x563500
  void Read(istream* in) override;               // 2: 0x5635f0 (shared with 0x225)
  void Write(ostream* out) override;             // 3: 0x563600
  void Checksum(Crc32* crc) override;            // 4: 0x563610
  int CheckEnabled(StatusCode* status) override; // 6: 0x563620
  int CheckValid(StatusCode* status) override;   // 7: 0x5636f0
  int Execute() override;                        // 8: 0x5637c0
  int GetKind() override;                        // 9: 0x563570, 0x224
};

// Factory functions registered in the event factory table (0x51ef80).
void* CreateLoyaltyEvent373();  // 0x562d70
void* CreateSystemCommand225(); // 0x563180
void* CreateSystemCommand224(); // 0x563580

// The next object after `object` in the character families 0x30-0x3c (0x562a50, a thiscall on the object).
GameObjNotifier* NextCharacterSibling(GameObj* object);
