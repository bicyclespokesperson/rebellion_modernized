#include "strategy_ui/developer_menu.h"

#include <cstddef>
#include <string>

#include "ai/ai_controller_factory.h"
#include "app/ai_preferences.h"
#include "app/enhancements.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "strategy_ui/menu_window.h"
#include "strategy_ui/object_menu.h"
#include "ui/cool_app.h"
#include "ui/cool_string_field.h"
#include "ui/cool_window.h"
#include "ui/cool_label.h"
#include "ui/game_window_object.h"
#include "ui/text_scroll_field.h"

namespace
{

constexpr int kAiCommandFirst = 0xf100;
constexpr int kAiCommandsPerSide = 0x100;
constexpr int kMenuWidth = 250;
constexpr int kMenuRowHeight = 20;

void AddMenuChoice(GameWindowObject* window, int command, const std::string& label)
{
  auto* item
      = new MenuItem(command, 0, nullptr, nullptr, 0, 0, kMenuWidth, kMenuRowHeight, 0, 0, 0, 0, command, window, 1, 0);
  item->m_label->SetText(label.c_str());
  HDC dc = reb::compat::GetDC(window->m_hwnd);
  item->m_label->Measure(dc);
  reb::compat::ReleaseDC(window->m_hwnd, dc);
  item->SetColors(0x2ffffff, 0x2000000, 0x2808080);
  g_menuRoot.AddWithId(item);
}

} // namespace

void ShowAiDeveloperMenu(GameWindowObject* window, int x, int y, int resourceSlot, bool canSwitchSides)
{
  if (!window || !g_popupStack.empty() || IsTextInputActive()) {
    return;
  }
  DismissMenu();
  g_menuRoot.Clear();
  if (canSwitchSides) {
    AddMenuChoice(window, kDeveloperSwitchSides, "Switch sides");
  }
  AddMenuChoice(window, kDeveloperToggleEnhancements,
                EnhancementsEnabled() ? "* Enhancements: on" : "  Enhancements: off");
  const auto definitions = AiControllerDefinitions();
  GameSession* session = canSwitchSides ? GetGameSession() : nullptr;
  for (int side = 1; side <= 2; ++side) {
    int selectedKind = PreferredAiKind(side);
    if (session && session->m_controllers[side - 1] && AiControllerName(session->m_controllers[side - 1]->GetKind())) {
      selectedKind = session->m_controllers[side - 1]->GetKind();
    }
    const char* faction = side == 1 ? "Alliance" : "Empire";
    for (size_t index = 0; index < definitions.size() && index < kAiCommandsPerSide; ++index) {
      const auto& definition = definitions[index];
      std::string label = selectedKind == definition.kind ? "* " : "  ";
      label += faction;
      label += " AI: ";
      label += definition.displayName;
      AddMenuChoice(window, kAiCommandFirst + (side - 1) * kAiCommandsPerSide + static_cast<int>(index), label);
    }
  }
  ShowMenu(x, y, CoolAppGetResourceModule(resourceSlot), window, window, window->GetBackground());
}

bool HandleAiDeveloperMenuCommand(int command)
{
  if (command == kDeveloperToggleEnhancements) {
    SetEnhancementsEnabled(!EnhancementsEnabled());
    return true;
  }
  const int selection = command - kAiCommandFirst;
  if (selection < 0 || selection >= 2 * kAiCommandsPerSide) {
    return false;
  }
  const int side = selection / kAiCommandsPerSide + 1;
  const int index = selection % kAiCommandsPerSide;
  const auto definitions = AiControllerDefinitions();
  if (static_cast<size_t>(index) >= definitions.size()) {
    return true;
  }
  const int kind = definitions[index].kind;
  GameSession* session = AppGetGameType() == 1 ? GetGameSession() : nullptr;
  if (session && session->m_controllers[side - 1] && AiControllerName(session->m_controllers[side - 1]->GetKind())
      && !session->SetAiControllerKind(side, kind)) {
    return true;
  }
  SetPreferredAiKind(side, kind);
  return true;
}

bool IsTextInputActive()
{
  HWND focused = reb::compat::GetFocus();
  if (!focused) {
    return false;
  }
  char className[64]{};
  reb::compat::GetClassNameA(focused, className, sizeof(className));
  if (_stricmp(className, "edit") == 0) {
    return true;
  }
  CoolWindow* target = GetCoolWindow(focused);
  if (dynamic_cast<CoolStringField*>(target)) {
    return true;
  }
  auto* text = dynamic_cast<TextScrollField*>(target);
  return text && (text->m_state & kTextEditable) != 0;
}
