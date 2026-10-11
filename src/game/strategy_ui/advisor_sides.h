#pragma once
#include <cstdint>

#include "core/containers.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/advisor_notices.h"

// Code shared by the two side advisors (AllianceAdvisor / EmpireAdvisor): the original compiled the same source twice
// with different cue-table offsets, so the bodies of the cue-table functions differ only in constants.
class OrderBase;
class GameObjNotifier;

namespace advisor_sides
{

inline bool Is(const IntPair* status, uint32_t category, uint32_t code)
{
  return status->m_first == category && status->m_second == code;
}

inline bool KeyIn(const ObjectKey& key, uint32_t firstFamily, uint32_t lastFamily)
{
  return key.Family() >= firstFamily && key.Family() < lastFamily;
}

inline bool StatusCategoryIn(const IntPair* status, uint32_t first, uint32_t last)
{
  return status->m_first >= first && status->m_first < last;
}

inline void Raise(AdvisorBase* advisor, unsigned byteOffset, uint32_t ticks)
{
  advisor->CueDue(byteOffset) = g_advisorNow + ticks;
}

constexpr uint32_t kShortCue = 10;
constexpr uint32_t kQuickCue = 5;
constexpr uint32_t kLongCue = 0x32;
constexpr uint32_t kNeverTicks = 40000;

// The cue offsets that differ between the sides in the "refused order" handlers.
struct RefusalOffsets
{
  unsigned otherTarget; // refused move to a system / mission target (Alliance 0x5c, Empire 0x60)
  unsigned fleetWork; // 0x58 / 0x5c
  unsigned missionOrder; // 0x50 / 0x54
};

using ScheduleFallbackFn = void (*)(AdvisorBase*);

void RefusedGeneric(AdvisorBase* advisor,
                    OrderBase* order,
                    IntPair* status,
                    const RefusalOffsets& offsets,
                    ScheduleFallbackFn fallback);
void RefusedUnits(AdvisorBase* advisor, OrderBase* order, IntPair* status, int view, ScheduleFallbackFn fallback);

} // namespace advisor_sides

// The location key (+0x7c) of a manufacturing manager, copied into `out`.
ObjectKey* ManagerGetLocationKey(GameObjNotifier* manager, ObjectKey* out); // 0x4c27b0
