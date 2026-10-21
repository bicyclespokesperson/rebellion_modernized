// WINMM multimedia timers. The original delivers timeSetEvent callbacks on a separate winmm thread; minwin is
// single-threaded, so the callbacks run on the main thread from the message pump (documented deviation,
// docs/minwin_user.md): they fire when the game calls PeekMessage/GetMessage, at most once per call and timer, and a
// callback that is still running is not re-entered.
#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/mmtimer.h"
#include "port/minwin_impl/user/user_internal.h"

namespace minwin
{
namespace
{

struct MmTimer {
  UINT id;
  UINT delay;
  DWORD_PTR user;
  LPTIMECALLBACK proc;
  bool periodic;
  bool running;
  bool dead;
  unsigned due;
  MmTimer* next;
};

MmTimer* g_timers = nullptr;
UINT g_nextId = 0x100;
int g_serviceDepth = 0;

void ReapDead()
{
  MmTimer** link = &g_timers;
  while (*link) {
    if ((*link)->dead && !(*link)->running) {
      MmTimer* dead = *link;
      *link = dead->next;
      Free(dead);
    } else {
      link = &(*link)->next;
    }
  }
}

} // namespace

void ServiceMmTimers()
{
  if (!g_timers) {
    return;
  }
  ++g_serviceDepth;
  const unsigned now = PlatformTicksMs();
  for (MmTimer* t = g_timers; t; t = t->next) {
    if (t->dead || t->running || static_cast<int>(now - t->due) < 0) {
      continue;
    }
    t->running = true;
    if (t->periodic) {
      t->due += t->delay;
      if (static_cast<int>(now - t->due) > 0) {
        t->due = now + t->delay;
      }
    } else {
      t->dead = true;
    }
    t->proc(t->id, 0, t->user, 0, 0);
    t->running = false;
  }
  if (--g_serviceDepth == 0) {
    ReapDead();
  }
}

} // namespace minwin

using namespace minwin;

// timeGetTime and timeSetEvent are reached through src/port/native/winmm_clock.cpp, which calls these when no virtual
// clock is requested.
DWORD minwin::RealTimeMs()
{
  return PlatformTicksMs();
}

UINT minwin::SetMultimediaTimer(UINT delay, UINT, LPTIMECALLBACK proc, DWORD_PTR user, UINT flags)
{
  if (!proc) {
    return 0;
  }
  auto* t = static_cast<MmTimer*>(Alloc(sizeof(MmTimer)));
  t->id = g_nextId++;
  t->delay = delay ? delay : 1;
  t->user = user;
  t->proc = proc;
  t->periodic = (flags & TIME_PERIODIC) != 0;
  t->due = PlatformTicksMs() + t->delay;
  t->next = g_timers;
  g_timers = t;
  return t->id;
}

MMRESULT REB_STDCALL reb::compat::timeKillEvent(UINT id)
{
  for (MmTimer* t = g_timers; t; t = t->next) {
    if (t->id == id && !t->dead) {
      t->dead = true;
      if (!g_serviceDepth) {
        ReapDead();
      }
      return TIMERR_NOERROR;
    }
  }
  return 97; // TIMERR_NOCANDO
}
