#pragma once
#include <cstdint>

#include "economy/facility.h"
#include "gameobj/tracked_stat.h"

// The data block of ProcFacil objects (VT 0x66a178, 0x28 bytes, ctor 0x584590 over the facility data block 0x584100).
class ProcFacilData : public FacilityData
{
public:
  ProcFacilData(); // 0x584590
  ~ProcFacilData() override; // 0x5845f0 (deleting), 0x584610
  int Read(istream* in) override; // 0x584660
  int Write(ostream* out) override; // 0x5846b0
  int Checksum(Crc32* crc) override; // 0x584700

  TrackedStat m_processingStat; // +0x18, the counted value that the "processing" timer event 0x394 tracks
  int m_duration; // +0x20, processing time in game time units
  int m_cycleCount; // +0x24, bumped whenever the state enters or leaves WAITING
};

// ProcFacil (ctor 0x53a670, VT 0x6615b0, secondary ObjectBase VT 0x6615a8): the facilities that process something over
// time (families 0x28-0x2f: manufacturing and production facilities), 0x64 bytes. Vtable: 129 slots, 0-114 are
// Facility's, 115-128 are new (the Changed events 115-121 and the Notified events 122-128 of the properties below).
//
// Properties, with the usual SetX -> ApplyX (side views) -> OnXChanged / OnXNotified shape (see game_obj.h):
//
//   State (+0x58)  1 WAITING, 2 PROCESSING, 3 DONE, 4 IDLE ("Invalid ProcFacilState value!"; must be >= 0)
//   Etc (+0x5c)    estimated time of completion, game time ("Invalid ETC value!")
//   m_procFlags (+0x60), five bits:
//     0x01 Suspended   0x02 PointPresent   0x04 PointProcessed   0x08 Processing   0x10 OnStartupCycle
//
// UpdateState is the state machine that turns the flags into a state: a facility controlled by side 3 is IDLE, WAITING
// moves to PROCESSING once a point is present, PROCESSING to DONE once it is processed, DONE falls back to WAITING when
// the point is gone.
class ProcFacil : public Facility
{
public:
  ProcFacil(); // 0x53a670
  ~ProcFacil() override; // 0x53a6c0 (scalar deleting destructor 0x53a6a0)

  int HandleLocationConstraintFailure(NotifyContext* ctx) override; // 46, 0x53ab30: ChangeDestroyedReason(4, ctx)
  GameObjData* CreateData() override; // 58, 0x53b650
  int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override; // 60, 0x53b0f0
  int OnUsableChanged(int newValue, NotifyContext* ctx) override; // 68, 0x53b160
  int OnCreatedChanged(int newValue, NotifyContext* ctx) override; // 69, 0x53b140
  int OnExistingChanged(int newValue, NotifyContext* ctx) override; // 74, 0x53b1b0

  // ---- slots 115-121: events on the master ----
  virtual int OnStateChanged(int oldState, int newState, NotifyContext* ctx); // 115, 0x53b1d0
  virtual int OnEtcChanged(int oldValue, int newValue, NotifyContext* ctx)
  {
    return 1;
  } // 116, shared "return 1"
  virtual int OnSuspendedChanged(int value, NotifyContext* ctx); // 117, 0x53b2a0
  virtual int OnPointPresentChanged(int value, NotifyContext* ctx); // 118, 0x53b2c0
  virtual int OnPointProcessedChanged(int value, NotifyContext* ctx); // 119, 0x53b310
  virtual int OnProcessingChanged(int value, NotifyContext* ctx); // 120, 0x53b330
  virtual int OnStartupCycleChanged(int value, NotifyContext* ctx)
  {
    return 1;
  } // 121, shared "return 1" (0x524fb0)

  // ---- slots 122-128: events on a side view (debug logging, dead in the shipped game) ----
  virtual int OnStateNotified(int oldState, int newState, NotifyContext* ctx); // 122, 0x53b3e0
  virtual int OnEtcNotified(int oldValue, int newValue, NotifyContext* ctx); // 123, 0x53b4c0
  virtual int OnSuspendedNotified(int value, NotifyContext* ctx); // 124, 0x53b510
  virtual int OnPointPresentNotified(int value, NotifyContext* ctx); // 125, 0x53b550
  virtual int OnPointProcessedNotified(int value, NotifyContext* ctx); // 126, 0x53b590
  virtual int OnProcessingNotified(int value, NotifyContext* ctx); // 127, 0x53b5d0
  virtual int OnStartupCycleNotified(int value, NotifyContext* ctx); // 128, 0x53b610

  // ProcFacilData::m_cycleCount, 0 without a data block.
  int GetCycleCount(); // 0x53a6e0

  // ---- validated setters (master view only) ----
  int SetState(int value, NotifyContext* ctx); // 0x53a6f0
  int SetEtc(int value, NotifyContext* ctx); // 0x53a770
  int SetSuspended(int on, NotifyContext* ctx); // 0x53a7f0
  int SetPointPresent(int on, NotifyContext* ctx); // 0x53a860
  int SetPointProcessed(int on, NotifyContext* ctx); // 0x53a8d0
  int SetProcessing(int on, NotifyContext* ctx); // 0x53a940
  int SetOnStartupCycle(int on, NotifyContext* ctx); // 0x53a9b0

  // ---- state machine helpers ----
  // PROCESSING -> point processed.
  int MarkPointProcessedIfProcessing(NotifyContext* ctx); // 0x53aa20
  // Re-derives the state from the flags (see above); 0 if the state is out of range.
  int UpdateState(NotifyContext* ctx); // 0x53aa40
  // IDLE -> WAITING for a usable facility that is not suspended, else IDLE. Returns the setter's result.
  int Restart(NotifyContext* ctx); // 0x53aac0
  // Sets the state to IDLE; always 1.
  int ResetToIdle(NotifyContext* ctx); // 0x53ab10

  // ---- views ----
  int ApplyState(NotifyContext* ctx); // 0x53aff0
  int ApplyEtc(NotifyContext* ctx); // 0x53b070
  int ApplySuspended(NotifyContext* ctx); // 0x53ad20
  int ApplyPointPresent(NotifyContext* ctx); // 0x53adb0
  int ApplyPointProcessed(NotifyContext* ctx); // 0x53ae40
  int ApplyProcessing(NotifyContext* ctx); // 0x53aed0
  int ApplyOnStartupCycle(NotifyContext* ctx); // 0x53af60

  // ---- bodies behind slots 5-9 of the leaf classes ----
  int ReadImpl(istream* in); // 0x53ab50
  int WriteImpl(ostream* out); // 0x53aba0
  int ChecksumImpl(Crc32* crc); // 0x53ac60
  int ValidateImpl(); // 0x53aca0
  int ApplyAllImpl(NotifyContext* ctx); // 0x53abf0

  static constexpr uint32_t kSuspended = 0x1;
  static constexpr uint32_t kPointPresent = 0x2;
  static constexpr uint32_t kPointProcessed = 0x4;
  static constexpr uint32_t kProcessing = 0x8;
  static constexpr uint32_t kOnStartupCycle = 0x10;
  static constexpr int kStateWaiting = 1;
  static constexpr int kStateProcessing = 2;
  static constexpr int kStateDone = 3;
  static constexpr int kStateIdle = 4;

  int m_state; // +0x58
  int m_etc; // +0x5c
  uint32_t m_procFlags; // +0x60
};

// Iterator over families [0x28,0x30) (VT 0x6617b4, destructor only).
class ProcFacilIterator : public ChildIterator
{
public:
  ProcFacilIterator(GameObjNotifier* parent, uint32_t stateFilter); // 0x53b6e0
  ProcFacilIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side); // 0x53b760
  ~ProcFacilIterator() override; // 0x53b7e0 (scalar deleting destructor 0x53b6c0)
};
