#pragma once
#include "character/character.h"

// Character38 (family 0x38, VT 0x665be0, secondary 0x665bdc, 0xb0 bytes): a plain Character subclass for type 0x38
// (no strings; the story gates on it at 0x5438a0 together with the five named characters). It adds no fields. Its slots
// 5-7 and 9 are the second-level forwarders shared with the major characters (major_character.h).
class Character38 : public Character
{
  public:
  Character38();           // 0x56f020
  ~Character38() override; // 0x56f040 (deleting), 0x56f060

  uint32_t GetFamily() override;                                  // 1, 0x56f220: 0x38
  int Read(istream* in) override;                                 // 5, 0x56ef50 (MajorLeafRead)
  int Write(ostream* out) override;                               // 6, 0x56ef60
  int Checksum(Crc32* crc) override;                              // 7, 0x56ef80
  int Validate() override;                                        // 8, 0x56f210 (jump thunk to Character::Validate)
  int ApplyAll(NotifyContext* ctx) override;                      // 9, 0x56f1f0
  int GetEventKinds(int unused, EventKindSet* out) override;      // 15, 0x56f080
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x56f0a0
  int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, 0x56f950 (the story characters' shared stub)
  GameObj* CreateViewObject() override;                           // 57, 0x56f180
  int GetMissionLeaderRank() override
  {
    return 2;
  } // 127, 0x43c1f0
  int ShouldDestroyFromInjury() override;                                       // 184, 0x56f160
  int OnInjuryChanged(int oldValue, int newValue, NotifyContext* ctx) override; // 203, 0x56f310

  static int BuildEventKinds();     // 0x56f330, g_character38EventKinds at 0x6bb9d8
  static GameObjNotifier* Create(); // 0x56f230, the object factory of family 0x38
};
