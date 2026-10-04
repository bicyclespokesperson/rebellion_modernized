#pragma once
// Recurring timers (ids 0x380-0x396): base class 0x585fe0 (VT 0x66a230) and the delay record they carry. How they
// are armed and fired is described in docs/code_map.md (events); the concrete timers live in the modules that own
// the object they act on (character 0x57cca0-0x57d330, incident timers 0x565400-0x566410, ...).
#include <cstdint>

#include "core/layout.h"
#include "events/event_base.h"
#include "gameobj/tracked_stat.h"

// The timers' delay record is a TrackedStat (+0x44): a change counter, and in the packed word the armed flag (bit 0),
// the delay spread (the 'progress') and the minimum delay (the 'level'). A timer whose target's counter differs from
// its copy was cancelled.

class TimerEvent : public TimedEvent {
public:
    TimerEvent();                          // 0x585fe0
    TimerEvent(const TimerEvent& other);   // 0x5860a0
    virtual ~TimerEvent();                 // 0x586050

    virtual int Read(istream* in) override;                 // 2: 0x586100 -> ReadTimer
    virtual int Write(ostream* out) override;               // 3: 0x586110 -> WriteTimer
    virtual int Checksum(Crc32* crc) override;              // 4: 0x586120 -> ChecksumTimer
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x586150 (target and range checks)
    virtual int CheckValid(StatusCode* status) override;    // 7: 0x586290 (calls CheckEnabled)
    virtual int Execute() override;                         // 8: 0x5862a0, true if the timer must be rescheduled

    virtual FamilyRange* GetTargetRange(FamilyRange* out) = 0;  // 10: the object families the timer may target
    virtual TrackedStat* GetTargetSpec() = 0;                      // 11: the target's delay record for this timer kind
    virtual void Fire() = 0;                                     // 12: does the timer's work

    // delay = rand(0..spread) + minimum (0x586130).
    int RollDelay();
    int ReadTimer(istream* in);      // 0x586340
    int WriteTimer(ostream* out);    // 0x586360
    int ChecksumTimer(Crc32* crc);   // 0x586380

    TrackedStat m_spec;  // +0x44
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(TimerEvent, 0x4c);
REB_ASSERT_OFFSET(TimerEvent, m_spec, 0x44);
#pragma clang diagnostic pop
