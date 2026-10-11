#pragma once
#include "mission/mission_leaf.h"

class ObjectKeySet;
class ObjectRefList;

// MissionRecruit (family 0x55, ctor 0x56afc0, VT 0x664890, secondary ObjectBase VT 0x664888, 0xac bytes): the Recruit
// mission, which recruits one character for the side. The recruited character's key is a property of its own with
// Set/Apply/Changed/Notified like the base properties (slots 162, 163). Slot 8 and 37 are the shared forwarders (see
// mission_leaf.h); slots 5-7 and 9 reach the bodies below through forwarders that two more key-carrying mission types
// share.
class MissionRecruit : public MissionSystemTargetBase
{
public:
  MissionRecruit(); // 0x56afc0
  ~MissionRecruit() override; // 0x56b020 (deleting), 0x56b040

  uint32_t GetFamily() override; // 1, 0x56b2f0: 0x55
  int Read(istream* in) override; // 5, 0x570560 -> 0x5730b0
  int Write(ostream* out) override; // 6, 0x570570 -> 0x5730e0
  int Checksum(Crc32* crc) override; // 7, 0x572f80 -> 0x573140
  int Validate() override; // 8, shared 0x56d460
  int ApplyAll(NotifyContext* ctx) override; // 9, 0x56b2d0
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared 0x576d10
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x56b0a0
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x56b0c0
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x56b180
  GameObj* CreateViewObject() override; // 57, 0x56b260
  int Virtual157(GameObj* person, int* out) override; // 157, 0x56b7b0
  int Virtual158(NotifyContext* ctx) override; // 158, 0x56b940
  int Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x56b9a0
  int Virtual160(NotifyContext* ctx) override; // 160, shared 0x576700
  virtual int OnRecruitedCharacterChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
  {
    return 1;
  } // 162, 0x524b10
  virtual int OnRecruitedCharacterNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx); // 163, 0x56bb10

  int SetRecruitedCharacter(ObjectKey* key, NotifyContext* ctx); // 0x56b1a0
  int ApplyRecruitedCharacter(NotifyContext* ctx); // 0x56b860
  int ApplyAllBody(NotifyContext* ctx); // 0x56b830

  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x56b370
  static int CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out); // 0x56b550
  static int CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out); // 0x56b680
  static int BuildEventKinds(); // 0x56bb50
  static GameObjNotifier* Create(); // 0x56b300

  ObjectKey m_recruitedCharacter; // +0xa8
};
