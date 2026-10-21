#pragma once
// The two UI notification handlers that add data to their payload (0x43c650-0x43c8b0 and 0x43db10-0x43df10): they look
// up the objects around the one they are about when the notification is posted (slot 5) and keep the answers.
#include <cstdint>

#include "events/notif_event_payloads.h"
#include "events/ui_notif_helpers.h"
#include "strategy_ui/advisor_notices.h"

class PlayerController;

// kind 0x200: a character's mission and whether the character leads it.
class UiNotif200 : public IntPairPayload
{
  public:
  UiNotif200();           // 0x43c650
  ~UiNotif200() override; // 0x43c6d0 (deleting 0x43c6b0)

  void Read(istream* in) override;   // 0x43c790
  void Write(ostream* out) override; // 0x43c7c0
  int GetKind() override
  {
    return 0x200;
  } // shared 0x48b450
  int GetDeliveryMode() override
  {
    return 4;
  } // shared 0x4047d0
  void PrepareForDispatch() override;                   // 0x43c7f0
  void Dispatch(PlayerController* controller) override; // 0x43c890
  void DispatchToSink(UiNotice* sink) override
  {
    if (sink) {
      sink->OnPayload(GetKind(), asArg(this));
    }
  } // shared 0x43df10

  ObjectKey m_missionKey; // +0x2c the mission of the character named by m_key
  int m_leadsMission;     // +0x30 1 if the character is the mission's leader
};

// kind 0x1e0: a character, the capital ship it is aboard (+0x2c), that ship's grandparent (+0x30) and the
// system / sector-level object it sits in (+0x34, +0x38).
class UiNotif1e0 : public IntPairPayload
{
  public:
  UiNotif1e0();           // 0x43db10
  ~UiNotif1e0() override; // 0x43dbb0 (deleting 0x43db90)

  void Read(istream* in) override;   // 0x43dcb0
  void Write(ostream* out) override; // 0x43dcf0
  int GetKind() override;            // 0x43dca0
  int GetDeliveryMode() override
  {
    return 2;
  } // shared 0x43c1f0
  void PrepareForDispatch() override;                   // 0x43dd30
  void Dispatch(PlayerController* controller) override; // 0x43def0
  void DispatchToSink(UiNotice* sink) override
  {
    if (sink) {
      sink->OnPayload(GetKind(), asArg(this));
    }
  } // shared 0x43df10

  ObjectKey m_shipKey;        // +0x2c
  ObjectKey m_grandparentKey; // +0x30
  ObjectKey m_systemKey;      // +0x34
  ObjectKey m_locationKey;    // +0x38

  ObjectKey* CopyGrandparentKey(ObjectKey* out); // 0x490f40
  ObjectKey* CopySystemKey(ObjectKey* out);      // 0x490f60
};

void* CreateUiNotif200(); // 0x43c720
void* CreateUiNotif1e0(); // 0x43dc30
