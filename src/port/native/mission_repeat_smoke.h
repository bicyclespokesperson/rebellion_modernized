#pragma once

#include <cstdint>
#include <functional>
#include <string>

class RebellionApplication;

namespace reb::native {

struct MissionRepeatSmokeHooks {
  std::function<bool(uint32_t ticks)> advance;
  std::function<bool()> saveAndReload;
  std::function<void(const std::string&)> log;
};

// Sends a sabotage team from the loaded game, repeats it from its report through a save and reload, and checks that
// the team goes out again on its own once home: against the same troop regiment, or the next one if the first was
// destroyed. Also covers the Repeat button of the message index and dropping a queued repeat when the team gets
// other orders.
bool RunMissionRepeatSmoke(RebellionApplication& app, const MissionRepeatSmokeHooks& hooks, std::string& error);

} // namespace reb::native
