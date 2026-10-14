#include "app/ai_preferences.h"

#include "ai/ai_controller_factory.h"
#include "app/player_controller_decl.h"
#include "port/minwin_impl/config/config.h"

namespace
{

const char* PreferenceKey(int side)
{
  if (side == 1)
    return "alliance_ai";
  if (side == 2)
    return "empire_ai";
  return nullptr;
}

} // namespace

int PreferredAiKind(int side)
{
  const char* key = PreferenceKey(side);
  if (!key)
    return kPlayerAi;
  const int kind = AiControllerKindFromName(mw::config::Global().Get(key, "classic").c_str());
  return kind ? kind : kPlayerAi;
}

bool SetPreferredAiKind(int side, int kind)
{
  const char* key = PreferenceKey(side);
  const char* name = AiControllerName(kind);
  if (!key || !name)
    return false;
  mw::config::Config& config = mw::config::Global();
  config.Set(key, name);
  return config.Save(mw::config::GlobalPath());
}
