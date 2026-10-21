#pragma once

#include <functional>
#include <string>

class RebellionApplication;

namespace reb::native {

bool RunLiveBattleSmoke(RebellionApplication& app, const std::string& savePath,
                        const std::function<bool()>& save, const std::function<bool()>& reload,
                        std::string& error);

} // namespace reb::native
