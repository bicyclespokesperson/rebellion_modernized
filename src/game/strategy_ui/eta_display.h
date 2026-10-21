#pragma once

#include <cstdint>
#include <string>

#include "app/enhancements.h"
#include "economy/manu_mgr.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"

namespace strategy_ui
{

constexpr uint32_t kEnrouteState = 0x10;
constexpr int kEtaFontSlot = 8;
constexpr uint32_t kManuStatusCategory = 0xa0;
constexpr uint32_t kManuNoFacility = 3;
constexpr uint32_t kManuFacilitiesOutOfService = 4;
constexpr uint32_t kManuWaitingForInput = 6;

// The smaller ETA font with the enhancements on, otherwise the original's `original`.
inline int EtaFontSlot(int original)
{
  return EnhancementsEnabled() ? kEtaFontSlot : original;
}

inline void AppendEtaSuffix(std::string* text, GameObj* object)
{
  if (!EnhancementsEnabled() || (object->m_stateFlags & kEnrouteState) == 0) {
    return;
  }

  StatusCode status(1, static_cast<uint32_t>(-1));
  int arrivalDay = 0;
  const int queryOk = QueryRuleValueB(object, &status, &arrivalDay);
  if (queryOk && status.m_second == static_cast<uint32_t>(-1) && arrivalDay != 0) {
    text->append(" (ETA ");
    text->append(std::to_string(arrivalDay));
    text->push_back(')');
  }
}

inline void AppendManufacturingEtaSuffix(std::string* text, ManuMgr* manager)
{
  if (!EnhancementsEnabled() || manager->m_remainingGameObjCount == 0 || !manager->m_seedKey.IsSet()
      || (manager->ViewSide() != 0 && manager->ViewSide() != manager->ControlSide())) {
    return;
  }
  StatusCode status(1, static_cast<uint32_t>(-1));
  int days = 0;
  if (!manager->EstimateBuildTime(&manager->m_seedKey, 1, 1, &status, &days)) {
    return;
  }
  if (status.m_first == kManuStatusCategory) {
    if (status.m_second == kManuWaitingForInput) {
      text->append(" (Waiting for materials)");
    } else if (status.m_second == kManuNoFacility) {
      text->append(" (No facility)");
    } else if (status.m_second == kManuFacilitiesOutOfService) {
      text->append(" (Facilities paused)");
    }
    return;
  }
  if (status.m_second == static_cast<uint32_t>(-1)) {
    text->append(" (ETA ");
    text->append(std::to_string(CallGameHook2B08() + days));
    text->push_back(')');
  }
}

inline std::string DisplayName(GameObj* object)
{
  std::string name = object->GetName()->c_str();
  AppendEtaSuffix(&name, object);
  return name;
}

} // namespace strategy_ui
