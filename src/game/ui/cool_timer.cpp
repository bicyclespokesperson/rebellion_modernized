#include "ui/cool_timer.h"

int BeginTimerPeriod(UINT milliseconds)
{
  if (g_timerPeriodActive == 1)
    return 0;
  TIMECAPS capabilities;
  if (timeGetDevCaps(&capabilities, sizeof(capabilities)) != TIMERR_NOERROR)
    return 0;
  const UINT clamped = capabilities.wPeriodMin <= milliseconds ? milliseconds : capabilities.wPeriodMin;
  g_timerPeriod = capabilities.wPeriodMax;
  if (clamped < capabilities.wPeriodMax)
    g_timerPeriod = clamped;
  if (g_timerPeriod > milliseconds)
    return 0;
  timeBeginPeriod(g_timerPeriod);
  g_timerPeriodActive = 1;
  return 1;
}

int EndTimerPeriod()
{
  if (g_timerPeriodActive == 0)
    return 0;
  timeEndPeriod(g_timerPeriod);
  g_timerPeriodActive = 0;
  return 1;
}

CoolTimer::CoolTimer(UINT delay, UINT type, UINT resolution)
{
  InitializeCriticalSection(&m_lock);
  EnterCriticalSection(&m_lock);
  m_pauseCount = 1;
  m_running = 0;
  m_timerId = 0;
  m_delay = delay;
  m_type = type;
  m_resolution = resolution;
  m_tickMissed = 0;
  if (resolution <= g_timerPeriod)
    resolution = g_timerPeriod;
  m_resolution = resolution;
  if (delay != 0 && g_timerPeriodActive != 0)
  {
    m_timerId = timeSetEvent(delay, resolution, TimerProc, reinterpret_cast<DWORD_PTR>(this), type);
    if (m_timerId != 0)
      m_running = 1;
  }
  m_pauseCount = 0;
  LeaveCriticalSection(&m_lock);
}

CoolTimer::~CoolTimer()
{
  Stop();
  DeleteCriticalSection(&m_lock);
}

void CoolTimer::Pause()
{
  m_pauseCount = m_pauseCount + 1;
}

void CoolTimer::Stop()
{
  m_running = 0;
  EnterCriticalSection(&m_lock);
  m_pauseCount = m_pauseCount + 1;
  if (m_timerId != 0)
  {
    timeKillEvent(m_timerId);
    m_timerId = 0;
  }
  LeaveCriticalSection(&m_lock);
}

void CoolTimer::Restart(UINT delay, UINT type)
{
  Stop();
  UINT effectiveDelay = delay;
  if (delay == 0)
    effectiveDelay = m_delay;
  m_delay = effectiveDelay;
  if (type == 0)
    type = m_type;
  m_type = type;
  if (g_timerPeriodActive != 0 && (delay != 0 || effectiveDelay != 0))
  {
    m_timerId = timeSetEvent(effectiveDelay, m_resolution, TimerProc, reinterpret_cast<DWORD_PTR>(this), type);
    if (m_timerId != 0)
      m_running = 1;
  }
  m_pauseCount = 0;
}

void CALLBACK CoolTimer::TimerProc(UINT, UINT, DWORD_PTR user, DWORD_PTR, DWORD_PTR)
{
  auto* timer = reinterpret_cast<CoolTimer*>(user);
  if (timer->m_pauseCount != 0)
  {
    timer->m_tickMissed = timer->m_type == 1;
    return;
  }
  EnterCriticalSection(&timer->m_lock);
  if (timer->m_pauseCount == 0)
  {
    timer->m_pauseCount = 1;
    timer->OnTick();
    if (timer->m_pauseCount > 0)
      timer->m_pauseCount = timer->m_pauseCount - 1;
    timer->m_tickMissed = 0;
    LeaveCriticalSection(&timer->m_lock);
    return;
  }
  timer->m_tickMissed = timer->m_type == 1;
  LeaveCriticalSection(&timer->m_lock);
}
