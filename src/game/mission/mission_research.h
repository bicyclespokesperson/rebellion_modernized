#pragma once
#include "mission/mission_leaf.h"

class ObjectKeySet;
class ObjectRefList;

// MissionResearch (family 0x53, ctor 0x56c070, VT 0x664db8, secondary ObjectBase VT 0x664db0, 0xb0 bytes): the Research
// mission, which adds R&D points to the side's shipyard / training facility / construction yard research (the kind is
// the mission record's key, 0x53000020-0x53000022). It keeps two integer properties of its own, the points it delivers
// and the orders it changes, each with Set/Apply/Changed/Notified like the base properties (slots 162-165). Slot 8
// and 37 are the shared forwarders (see mission_leaf.h); slots 5-7 and 9 have own forwarders to the bodies below.
class MissionResearch : public MissionSystemTargetBase
{
public:
  MissionResearch(); // 0x56c070
  ~MissionResearch() override; // 0x56c0a0 (deleting), 0x56c0c0

  uint32_t GetFamily() override; // 1, 0x56c3e0: 0x53
  int Read(istream* in) override; // 5, 0x56c390 -> 0x56cb40
  int Write(ostream* out) override; // 6, 0x56c3a0 -> 0x56cb80
  int Checksum(Crc32* crc) override; // 7, 0x56c3d0 -> 0x56cc00
  int Validate() override; // 8, shared 0x56d460
  int ApplyAll(NotifyContext* ctx) override; // 9, 0x56c3b0
  int CreateViews(int controlKind, NotifyContext* ctx) override; // 37, shared 0x576d10
  int GetEventKinds(int unused, EventKindSet* out) override; // 15, 0x56c140
  int CheckOrder(int a, int orderKind, StatusCode* out) override; // 22, 0x56c160
  int GetCheckFunctions(MissionCheckFunctions* out) override; // 111, 0x56c220
  GameObj* CreateViewObject() override; // 57, 0x56c320
  int GetPersonSuccessChance(GameObj* person, int* out) override; // 157, 0x56cb20
  int BeforeScoringResults(NotifyContext* ctx) override; // 158, 0x56cd70
  int ApplyPersonResult(GameObj* person, int roll, int* out, NotifyContext* ctx) override; // 159, 0x56cdb0
  int AfterScoringResults(NotifyContext* ctx) override; // 160, 0x56cf30
  virtual int OnRdPointsChanged(int oldValue, int newValue, NotifyContext* ctx)
  {
    return 1;
  } // 162, 0x524b10
  virtual int OnRdOrdersChanged(int oldValue, int newValue, NotifyContext* ctx)
  {
    return 1;
  } // 163, 0x524b10
  virtual int OnRdPointsNotified(int oldValue, int newValue, NotifyContext* ctx); // 164, 0x56d0b0
  virtual int OnRdOrdersNotified(int oldValue, int newValue, NotifyContext* ctx); // 165, 0x56d0f0

  int SetRdPoints(int value, NotifyContext* ctx); // 0x56c240
  int SetRdOrders(int value, NotifyContext* ctx); // 0x56c2b0
  int ApplyRdPoints(NotifyContext* ctx); // 0x56cc50
  int ApplyRdOrders(NotifyContext* ctx); // 0x56cce0
  int ApplyAllBody(NotifyContext* ctx); // 0x56cbc0
  int GetResearchKind(int* out); // 0x56c0e0: 0 shipyard, 1 training facility, 2 construction yard

  static int ResearchKindFromKey(ObjectKey* key, int* out); // 0x56c460
  static int CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out); // 0x56c510
  static int CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out); // 0x56c820
  static int CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out); // 0x56c990
  static int BuildEventKinds(); // 0x56d130
  static GameObjNotifier* Create(); // 0x56c3f0

  int m_rdPoints; // +0xa8
  int m_rdOrders; // +0xac
};
