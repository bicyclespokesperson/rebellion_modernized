#pragma once
#include <cstdint>

#include "app/player_controller_decl.h"
#include "core/game_string.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"

class CoolLabel;
class GalaxyWindow;
class Notice8;
class TextScrollField;

// The "do you want to ...?" window of the galaxy view (child type 0x10): a picture, a title, the text of the
// request and two buttons. Yes (child 0x14, Return) hands the request's order to the human player, No (0x15,
// Escape) discards it.
class OrderConfirmWindow : public CoolGameWindow
{
public:
  OrderConfirmWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, Notice8* request); // 0x44f060
  ~OrderConfirmWindow() override; // 0x44f0f0 (scalar deleting destructor 0x44f0d0)

  void OnCreate1() override; // slot 14, 0x44f180
  void OnCommand(int a, int b, int c) override; // slot 18, 0x44f5e0
  void OnKeyDown(WPARAM key, LPARAM lParam) override; // slot 19, 0x44f640

  GalaxyWindow* m_galaxy; // +0x114
  Notice8* m_request; // +0x118 owned
  TextScrollField* m_textField; // +0x11c owned
  CoolLabel* m_titleLabel; // +0x120 owned
};
