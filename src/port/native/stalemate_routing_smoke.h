#pragma once

#include <string>

class RebellionApplication;

namespace reb::native {

// After a space battle that ends with both sides present (a stalemate or the time limit), the side that did not hold
// the system is routed away; any of its ships that cannot leave are destroyed (tactical_combat.md §6.11).
bool RunStalemateRoutingSmoke(RebellionApplication& app, std::string& error);

} // namespace reb::native
