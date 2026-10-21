#include "app/battle_display_names.h"

#include <memory>

#include "port/minwin_impl/res/formats.h"
#include "port/minwin_impl/res/modules.h"

std::string LoadBattleDisplayString(const ResourceStringId& resource)
{
  std::string text = LoadStringResource(&resource);
  if (!text.empty() || resource.m_group != 2) {
    return text;
  }

  // Headless campaigns skip the strategic screen that registers TEXTSTRA in UI slot 2.
  static const std::unique_ptr<mw::res::Module, decltype(&mw::res::FreeModule)> module{
      mw::res::LoadModule("textstra.dll"), &mw::res::FreeModule};
  if (module) {
    mw::res::LoadStringText(*module, static_cast<uint16_t>(resource.m_id), &text);
  }
  return text;
}
