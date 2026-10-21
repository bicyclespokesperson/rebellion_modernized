#include "economy/procfacil.h"

#include "core/checked_new.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "events/event_post.h"
#include "fleet/unit_view_flags.h"
#include "gameobj/game_hooks.h"

// The data block constructor (0x584590).

extern int g_procFacilNotifyLog; // 0x6b9048, never set in the shipped game

namespace
{

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;
constexpr int kMasterView = 0;
constexpr int kEventStateChanged = 0x315;
constexpr int kEventProcessingTimer = 0x394;
constexpr uint32_t kControlSideMask = 0xc0;
constexpr uint32_t kUsable = 0x1;
constexpr uint32_t kDestroyedReasonIdle = 4;
constexpr int kApplyEverything = 1;

ProcFacil* viewOf(ProcFacil* facility, int view)
{
  return static_cast<ProcFacil*>(facility->ViewAs(view));
}

ProcFacilData* dataOf(ProcFacil* facility)
{
  return static_cast<ProcFacilData*>(facility->m_data);
}

using ApplyFunction = int (ProcFacil::*)(NotifyContext*);
using FlagEvent = int (ProcFacil::*)(int, NotifyContext*);

int setProcFlag(ProcFacil* facility, uint32_t mask, int on, NotifyContext* ctx, ApplyFunction apply, FlagEvent changed)
{
  const int result = facility->IsMasterView();
  if (result && UpdateFlagBit(mask, on, &facility->m_procFlags)) {
    (viewOf(facility, kAllianceView)->*apply)(ctx);
    (viewOf(facility, kEmpireView)->*apply)(ctx);
    (facility->*changed)(on, ctx);
  }
  return result;
}

int applyProcFlag(ProcFacil* facility, uint32_t mask, NotifyContext* ctx, FlagEvent notified)
{
  return fleet::applyFlagToView(facility, &ProcFacil::m_procFlags, mask, ctx, [&](int value) {
    return (facility->*notified)(value, ctx);
  });
}

int notifyProcFlag(ProcFacil* facility, const char* tag, const char* property, int value, NotifyContext* ctx)
{
  const int result = facility->IsSideView();
  if (result && g_procFacilNotifyLog) {
    facility->LogFlag(tag, property, value, 0, ctx);
  }
  return result;
}

const char* stateName(int state)
{
  switch (state) {
    case 0:
      return "NONE      ";
    case 1:
      return "WAITING   ";
    case 2:
      return "PROCESSING";
    case 3:
      return "DONE      ";
    case 4:
      return "IDLE      ";
    default:
      return "UNKNOWN   ";
  }
}

} // namespace

ProcFacil::ProcFacil()
    : m_state(kStateIdle)
    , m_etc(0)
    , m_procFlags(0)
{
}

ProcFacil::~ProcFacil() = default;

// ---- virtuals ------------------------------------------------------------------------------------------------

int ProcFacil::HandleLocationConstraintFailure(NotifyContext* ctx)
{
  return ChangeDestroyedReason(kDestroyedReasonIdle, ctx);
}

GameObjData* ProcFacil::CreateData()
{
  return NewOrNull<ProcFacilData>();
}

ProcFacilData::ProcFacilData()
    : m_duration(0)
    , m_cycleCount(0)
{
}

ProcFacilData::~ProcFacilData() = default;

int ProcFacilData::Read(istream* in)
{
  int result = UnitObjData::Read(in);
  if (result) {
    result = m_processingStat.Read(in);
    if (result) {
      ReadValue32(in, &m_duration);
      ReadValue32(in, &m_cycleCount);
    }
  }
  return result;
}

int ProcFacilData::Write(ostream* out)
{
  int result = UnitObjData::Write(out);
  if (result) {
    result = m_processingStat.Write(out);
    if (result) {
      WriteValue32(out, &m_duration);
      WriteValue32(out, &m_cycleCount);
    }
  }
  return result;
}

int ProcFacilData::Checksum(Crc32* crc)
{
  int result = UnitObjData::Checksum(crc);
  if (result) {
    result = m_processingStat.Checksum(crc);
    if (result) {
      crc->AddInt(m_duration);
      crc->AddInt(m_cycleCount);
    }
  }
  return result;
}

int ProcFacil::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx)
{
  const int base = Facility::OnControlKindChanged(oldKind, newKind, ctx) != 0;
  return ResetToIdle(ctx) && base;
}

int ProcFacil::OnUsableChanged(int newValue, NotifyContext* ctx)
{
  int result = Facility::OnUsableChanged(newValue, ctx) != 0;
  if (newValue == 0) {
    result = ResetToIdle(ctx) && result;
  }
  return result;
}

int ProcFacil::OnCreatedChanged(int newValue, NotifyContext* ctx)
{
  return UnitObjBase::OnCreatedChanged(newValue, ctx) != 0;
}

int ProcFacil::OnExistingChanged(int newValue, NotifyContext* ctx)
{
  return Facility::OnExistingChanged(newValue, ctx) != 0;
}

// ---- events on the master ------------------------------------------------------------------------------------

int ProcFacil::OnStateChanged(int oldState, int newState, NotifyContext* ctx)
{
  if (newState == kStateWaiting || oldState == kStateWaiting) {
    ++dataOf(this)->m_cycleCount;
  }
  uint32_t startupCycle = (m_procFlags >> 4) & 1;
  if (newState == kStateWaiting) {
    startupCycle = oldState == kStateIdle;
  } else if (newState == kStateIdle) {
    startupCycle = 0;
  }
  const int startupSet = SetOnStartupCycle(startupCycle, ctx);
  const int processingSet = SetProcessing(newState == kStateProcessing, ctx);
  bool ok = processingSet != 0 && startupSet != 0;
  ok = UpdateState(ctx) != 0 && ok;
  return PostSourceEvent(kEventStateChanged, this, oldState, newState, ctx) != 0 && ok;
}

int ProcFacil::OnSuspendedChanged(int value, NotifyContext* ctx)
{
  int result = 1;
  if (value != 0) {
    result = ResetToIdle(ctx) != 0;
  }
  return result;
}

int ProcFacil::OnPointPresentChanged(int value, NotifyContext* ctx)
{
  bool ok = true;
  if (value == 0) {
    ok = SetPointProcessed(0, ctx) != 0;
  }
  return UpdateState(ctx) != 0 && ok;
}

int ProcFacil::OnPointProcessedChanged(int value, NotifyContext* ctx)
{
  return UpdateState(ctx) != 0;
}

int ProcFacil::OnProcessingChanged(int value, NotifyContext* ctx)
{
  bool ok = true;
  if (value != 0) {
    ok = ArmTimerAfterSetLevel(kEventProcessingTimer, dataOf(this)->m_duration, this, &dataOf(this)->m_processingStat,
                               ctx)
         != 0;
  }
  const bool tracked
      = ArmTimerAfterSetFlag(kEventProcessingTimer, value, this, &dataOf(this)->m_processingStat, ctx) != 0 && ok;
  int etc = 0;
  if (value != 0) {
    ProcFacilData* data = dataOf(this);
    etc = CallGameHook2B08() + data->m_duration;
  }
  return SetEtc(etc, ctx) != 0 && tracked;
}

// ---- events on a side view -----------------------------------------------------------------------------------

int ProcFacil::OnStateNotified(int oldState, int newState, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result && g_procFacilNotifyLog) {
    LogChange("(ProcFacilProcFacilStateNotif)", "ProcFacilState", stateName(newState), stateName(oldState), nullptr,
              ctx);
  }
  return result;
}

int ProcFacil::OnEtcNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result && g_procFacilNotifyLog) {
    LogInt("(ProcFacilETCNotif)", "ETC", newValue, oldValue, 0, ctx);
  }
  return result;
}

int ProcFacil::OnSuspendedNotified(int value, NotifyContext* ctx)
{
  return notifyProcFlag(this, "(ProcFacilSuspendedNotif)", "Suspended", value, ctx);
}

int ProcFacil::OnPointPresentNotified(int value, NotifyContext* ctx)
{
  return notifyProcFlag(this, "(ProcFacilPointPresentNotif)", "PointPresent", value, ctx);
}

int ProcFacil::OnPointProcessedNotified(int value, NotifyContext* ctx)
{
  return notifyProcFlag(this, "(ProcFacilPointProcessedNotif)", "PointProcessed", value, ctx);
}

int ProcFacil::OnProcessingNotified(int value, NotifyContext* ctx)
{
  return notifyProcFlag(this, "(ProcFacilProcessingNotif)", "Processing", value, ctx);
}

int ProcFacil::OnStartupCycleNotified(int value, NotifyContext* ctx)
{
  return notifyProcFlag(this, "(ProcFacilOnStartupCycleNotif)", "OnStartupCycle", value, ctx);
}

// ---- setters -------------------------------------------------------------------------------------------------

int ProcFacil::GetCycleCount()
{
  return m_data ? dataOf(this)->m_cycleCount : 0;
}

int ProcFacil::SetState(int value, NotifyContext* ctx)
{
  int result = 0;
  if (IsMasterView()) {
    result = IsInRange(value, 0, value, "Invalid ProcFacilState value!");
  }
  if (result && m_state != value) {
    const int oldValue = m_state;
    m_state = value;
    viewOf(this, kAllianceView)->ApplyState(ctx);
    viewOf(this, kEmpireView)->ApplyState(ctx);
    OnStateChanged(oldValue, m_state, ctx);
  }
  return result;
}

int ProcFacil::SetEtc(int value, NotifyContext* ctx)
{
  int result = 0;
  if (IsMasterView()) {
    result = IsInRange(value, 0, value, "Invalid ETC value!");
  }
  if (result && m_etc != value) {
    const int oldValue = m_etc;
    m_etc = value;
    viewOf(this, kAllianceView)->ApplyEtc(ctx);
    viewOf(this, kEmpireView)->ApplyEtc(ctx);
    OnEtcChanged(oldValue, m_etc, ctx);
  }
  return result;
}

int ProcFacil::SetSuspended(int on, NotifyContext* ctx)
{
  return setProcFlag(this, kSuspended, on, ctx, &ProcFacil::ApplySuspended, &ProcFacil::OnSuspendedChanged);
}

int ProcFacil::SetPointPresent(int on, NotifyContext* ctx)
{
  return setProcFlag(this, kPointPresent, on, ctx, &ProcFacil::ApplyPointPresent, &ProcFacil::OnPointPresentChanged);
}

int ProcFacil::SetPointProcessed(int on, NotifyContext* ctx)
{
  return setProcFlag(this, kPointProcessed, on, ctx, &ProcFacil::ApplyPointProcessed,
                     &ProcFacil::OnPointProcessedChanged);
}

int ProcFacil::SetProcessing(int on, NotifyContext* ctx)
{
  return setProcFlag(this, kProcessing, on, ctx, &ProcFacil::ApplyProcessing, &ProcFacil::OnProcessingChanged);
}

int ProcFacil::SetOnStartupCycle(int on, NotifyContext* ctx)
{
  return setProcFlag(this, kOnStartupCycle, on, ctx, &ProcFacil::ApplyOnStartupCycle,
                     &ProcFacil::OnStartupCycleChanged);
}

// ---- state machine ---------------------------------------------------------------------------------------------

int ProcFacil::MarkPointProcessedIfProcessing(NotifyContext* ctx)
{
  return m_state == kStateProcessing ? SetPointProcessed(1, ctx) : 0;
}

int ProcFacil::UpdateState(NotifyContext* ctx)
{
  int state = m_state;
  const int valid = state >= kStateWaiting && state <= kStateIdle;
  int result = valid;
  if (valid) {
    if ((m_flags & kControlSideMask) == kControlSideMask) {
      state = kStateIdle;
    } else if (state == kStateWaiting) {
      if (m_procFlags & kPointPresent) {
        state = kStateProcessing;
      }
    } else if (state == kStateProcessing) {
      if (m_procFlags & kPointProcessed) {
        state = kStateDone;
      }
    } else if (state == kStateDone && !(m_procFlags & kPointPresent)) {
      state = kStateWaiting;
    }
    result = SetState(state, ctx);
  }
  return result;
}

int ProcFacil::Restart(NotifyContext* ctx)
{
  if (m_state == kStateIdle && (m_stateFlags & kUsable) && !(m_procFlags & kSuspended)) {
    return SetState(kStateWaiting, ctx);
  }
  return ResetToIdle(ctx);
}

int ProcFacil::ResetToIdle(NotifyContext* ctx)
{
  SetState(kStateIdle, ctx);
  return 1;
}

// ---- views -------------------------------------------------------------------------------------------------------

int ProcFacil::ApplyState(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ProcFacil* master = viewOf(this, kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_state;
      if (oldValue != master->m_state) {
        m_state = master->m_state;
        if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide()))) {
          result = OnStateNotified(oldValue, m_state, ctx);
        }
      }
    }
  }
  return result;
}

int ProcFacil::ApplyEtc(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    ProcFacil* master = viewOf(this, kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_etc;
      if (oldValue != master->m_etc) {
        m_etc = master->m_etc;
        if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide()))) {
          result = OnEtcNotified(oldValue, m_etc, ctx);
        }
      }
    }
  }
  return result;
}

int ProcFacil::ApplySuspended(NotifyContext* ctx)
{
  return applyProcFlag(this, kSuspended, ctx, &ProcFacil::OnSuspendedNotified);
}

int ProcFacil::ApplyPointPresent(NotifyContext* ctx)
{
  return applyProcFlag(this, kPointPresent, ctx, &ProcFacil::OnPointPresentNotified);
}

int ProcFacil::ApplyPointProcessed(NotifyContext* ctx)
{
  return applyProcFlag(this, kPointProcessed, ctx, &ProcFacil::OnPointProcessedNotified);
}

int ProcFacil::ApplyProcessing(NotifyContext* ctx)
{
  return applyProcFlag(this, kProcessing, ctx, &ProcFacil::OnProcessingNotified);
}

int ProcFacil::ApplyOnStartupCycle(NotifyContext* ctx)
{
  return applyProcFlag(this, kOnStartupCycle, ctx, &ProcFacil::OnStartupCycleNotified);
}

// ---- serialisation, checksum, validation -------------------------------------------------------------------------

int ProcFacil::ReadImpl(istream* in)
{
  const int result = GameObj::Read(in);
  if (result) {
    int state;
    ReadValue32(in, &state);
    m_state = state;
    ReadValue32(in, &m_etc);
    ReadValue32(in, &m_procFlags);
  }
  return result;
}

int ProcFacil::WriteImpl(ostream* out)
{
  const int result = GameObj::Write(out);
  if (result) {
    int state = m_state;
    WriteValue32(out, &state);
    WriteValue32(out, &m_etc);
    WriteValue32(out, &m_procFlags);
  }
  return result;
}

int ProcFacil::ChecksumImpl(Crc32* crc)
{
  const int result = GameObj::Checksum(crc);
  if (result) {
    crc->AddInt(m_state);
    crc->AddInt(m_etc);
    crc->AddInt(m_procFlags);
  }
  return result;
}

int ProcFacil::ApplyAllImpl(NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    Facility::ApplyAllImpl(ctx);
    if (ctx->m_flag04 == kApplyEverything) {
      ApplyState(ctx);
      ApplyEtc(ctx);
      ApplySuspended(ctx);
      ApplyPointPresent(ctx);
      ApplyPointProcessed(ctx);
      ApplyProcessing(ctx);
      ApplyOnStartupCycle(ctx);
    }
  }
  return result;
}

int ProcFacil::ValidateImpl()
{
  bool valid = false;
  if (Facility::ValidateImpl() != 0) {
    uint32_t check = ~m_procFlags >> 2 & 1;
    if (check != 0) {
      if (m_state == kStateWaiting) {
        check = ~m_procFlags >> 1 & 1;
      }
      if (check != 0) {
        if (m_state == kStateProcessing) {
          check = m_procFlags >> 1 & 1;
        }
        if (check != 0) {
          valid = m_state != kStateDone;
          if (valid
              && ((m_flags & kControlSideMask) == kControlSideMask || (m_procFlags & kSuspended)
                  || !(m_stateFlags & kUsable))) {
            valid = m_state == kStateIdle;
          }
        }
      }
    }
  }
  return valid;
}

// ---- iterator ------------------------------------------------------------------------------------------------------

namespace
{
const FamilyRange kProcFacilFamilies(0x28, 0x30);
}

ProcFacilIterator::ProcFacilIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kProcFacilFamilies, stateFilter)
{
}

ProcFacilIterator::ProcFacilIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kProcFacilFamilies, stateFilter)
{
}

ProcFacilIterator::~ProcFacilIterator() = default;

int g_procFacilNotifyLog{};
