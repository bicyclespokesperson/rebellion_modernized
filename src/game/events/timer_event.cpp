#include "events/timer_event.h"

#include "core/utility_classes.h"
#include "data/random.h"
#include "gameobj/object_queries.h"

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusWrongFamily = 0x17;
constexpr int kStatusMissingTarget = 0x12;

} // namespace

TimerEvent::TimerEvent() : TimedEvent(), m_spec()
{
}

TimerEvent::TimerEvent(const TimerEvent& other) = default;

TimerEvent::~TimerEvent() = default;

void TimerEvent::Read(istream* in)
{
  ReadTimer(in);
}

void TimerEvent::Write(ostream* out)
{
  WriteTimer(out);
}

void TimerEvent::Checksum(Crc32* crc)
{
  ChecksumTimer(crc);
}

int TimerEvent::ReadTimer(istream* in)
{
  ReadTimed(in);
  return m_spec.Read(in);
}

int TimerEvent::WriteTimer(ostream* out)
{
  WriteTimed(out);
  return m_spec.Write(out);
}

int TimerEvent::ChecksumTimer(Crc32* crc)
{
  ChecksumTimed(crc);
  m_spec.Checksum(crc);
  return 1;
}

int TimerEvent::RollDelay()
{
  return GameRandom(m_spec.Progress()) + m_spec.Level();
}

int TimerEvent::CheckEnabled(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  FamilyRange scratch;
  const uint32_t family = m_key.Family();
  const FamilyRange* range = GetTargetRange(&scratch);
  if (family < range->m_first || range->m_second <= family)
    *status = StatusCode(kStatusCategory, kStatusWrongFamily);
  int result = 0;
  if (status->m_second == static_cast<uint32_t>(kStatusOk))
  {
    GameObjNotifier* target;
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    result = ResolveMasterInFamilies_01_ff(&m_key, &target, &missing, status);
  }
  return result;
}

int TimerEvent::CheckValid(StatusCode* status)
{
  return CheckEnabled(status);
}

int TimerEvent::Execute()
{
  StatusCode status(kStatusCategory, kStatusOk);
  int reschedule = 0;
  if (CheckValid(&status) != 0 && status.m_second == static_cast<uint32_t>(kStatusOk))
  {
    reschedule = GetTargetSpec()->m_changeCount == m_spec.m_changeCount;
    if (reschedule)
    {
      Fire();
      m_dueDay += RollDelay();
    }
  }
  return reschedule;
}
