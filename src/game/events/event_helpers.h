#pragma once
#include "core/reb_abi.h"
// Shared building blocks of the concrete event, command and timer classes (system_events.h, incident_timers.h, ...).
#include <cstdint>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "events/event_base.h"
#include "events/timer_event.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_queries.h"

namespace events {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusMissingTarget = 0x12;
constexpr int kStatusRefused = 1;
constexpr int kStatusNoDestination = 0x22;
constexpr int kStatusNoRoom = 0xf3;

inline bool succeeded(const StatusCode& status) { return status.m_second == static_cast<uint32_t>(kStatusOk); }

// The checks of events that need none: report success.
inline int succeedWithoutChecks(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    return 1;
}

// The prologue of slots 11 and 12 of the timers: the timer's own validity check (slot 7), then the lookup of the object
// the timer acts on, which both must find alive. Returns the object, or null with *ok cleared.
template <typename Resolve>
GameObjNotifier* findTimerOwner(TimerEvent* timer, Resolve resolve, int* ok) {
    StatusCode status(kStatusCategory, kStatusOk);
    GameObjNotifier* owner = nullptr;
    *ok = timer->CheckValid(&status);
    if (*ok) {
        if (succeeded(status)) {
            const StatusCode missing(kStatusCategory, kStatusMissingTarget);
            *ok = resolve(&timer->m_key, &owner, &missing, &status);
        }
        if (!*ok || !succeeded(status)) return nullptr;
    }
    return owner;
}

template <typename Owner, typename Resolve, typename Method>
TrackedStat* timerStatOfOwner(TimerEvent* timer, Resolve resolve, Method method) {
    int ok = 0;
    Owner* owner = static_cast<Owner*>(findTimerOwner(timer, resolve, &ok));
    return ok && owner ? method(owner) : nullptr;
}

template <typename Owner, typename Resolve, typename Method>
void fireOnTimerOwner(TimerEvent* timer, Resolve resolve, Method method) {
    int ok = 0;
    Owner* owner = static_cast<Owner*>(findTimerOwner(timer, resolve, &ok));
    if (ok && owner) {
        NotifyContext ctx(timer->m_context);
        method(owner, &ctx);
    }
}

// The int-typed argument slots of the object virtuals that really carry a pointer (TryRename's name).
inline int asIntArg(const void* pointer) { return static_cast<int>(reinterpret_cast<intptr_t>(pointer)); }

// The checks of commands: set *status to success, look the command's target up, and ask it. `ask` gets the target and
// returns the check's result; it only runs for a found, live target.
template <typename Resolve, typename Ask>
int checkOnTarget(Command* command, Resolve resolve, StatusCode* status, Ask ask) {
    GameObjNotifier* target = nullptr;
    int ok = 1;
    *status = StatusCode(kStatusCategory, kStatusOk);
    if (succeeded(*status)) {
        const StatusCode missing(kStatusCategory, kStatusMissingTarget);
        ok = resolve(&command->m_key, &target, &missing, status);
    }
    if (ok && succeeded(*status)) ok = ask(static_cast<GameObj*>(target));
    return ok;
}

// The execution of commands: look the target up and run `act(target, ctx)` for a found, live one. Returns 0 (the
// command is not queued again).
template <typename Resolve, typename Act>
int executeOnTarget(Command* command, Resolve resolve, Act act) {
    NotifyContext ctx(command->m_context);
    GameObjNotifier* target = nullptr;
    StatusCode status(kStatusCategory, kStatusOk);
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    const int ok = resolve(&command->m_key, &target, &missing, &status);
    if (ok && succeeded(status)) act(static_cast<GameObj*>(target), &ctx);
    return 0;
}

// Calls the virtual in `slot` of an object whose class is not reconstructed.
template <typename Result, typename... Args>
Result callVirtualSlot(void* object, int slot, Args... args) {
    using Method = Result(REB_THISCALL*)(void*, Args...);
    Method* vtable = *reinterpret_cast<Method**>(object);
    return vtable[slot](object, args...);
}

}  // namespace events
