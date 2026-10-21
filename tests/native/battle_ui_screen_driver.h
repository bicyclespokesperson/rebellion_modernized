// Drives a BattleScreen with synthetic input, as the port's translator would.
#pragma once
#include "battle_ui/battle_screen.h"
#include "battle_ui_test_support.h"

namespace reb::battle_ui::test
{

class ScreenDriver
{
  public:
  explicit ScreenDriver(BattleScreenOptions options = {})
      : screen(source, text, sprites, options)
  {
    screen.HandleInput(ScreenChanged{ScreenInfo{1280, 720, 1.0f, 1.0f}});
    Frame();
  }

  // One rendered frame: the widget kit hit-tests input against what the last frame drew.
  DrawList& Frame(double seconds = 0.0)
  {
    screen.Update(seconds);
    frame.Clear();
    screen.Build(frame);
    return frame;
  }
  Point At(battle::UnitIndex unit) const
  {
    return screen.GetCamera().WorldToScreen(FindUnit(screen.View(), unit)->position);
  }
  void Move(Point to, uint8_t modifiers = 0)
  {
    screen.HandleInput(MouseMove{to, m_buttons, modifiers});
  }
  void Button(Point at, MouseButton button, bool down, uint8_t modifiers = 0, int clicks = 1)
  {
    m_buttons = down ? m_buttons | ButtonBit(button) : m_buttons & ~ButtonBit(button);
    screen.HandleInput(MouseButtonEvent{at, button, down, clicks, modifiers});
  }
  void Click(Point at, MouseButton button = MouseButton::Left, uint8_t modifiers = 0, int clicks = 1)
  {
    Move(at, modifiers);
    Button(at, button, true, modifiers, clicks);
    Button(at, button, false, modifiers, clicks);
  }
  void Drag(Point from, Point to, MouseButton button = MouseButton::Left, uint8_t modifiers = 0)
  {
    Move(from, modifiers);
    Button(from, button, true, modifiers);
    Move({(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f}, modifiers);
    Move(to, modifiers);
    Button(to, button, false, modifiers);
  }
  void Press(Key key, uint8_t modifiers = 0)
  {
    screen.HandleInput(KeyEvent{key, true, false, modifiers});
    screen.HandleInput(KeyEvent{key, false, false, modifiers});
  }
  std::vector<battle::UnitIndex> Selected() const
  {
    return screen.GetSelection().Units();
  }

  RecordingSource source;
  FixedTextMeasurer text;
  NoSprites sprites;
  BattleScreen screen;
  DrawList frame;

  private:
  uint8_t m_buttons = 0;
};

} // namespace reb::battle_ui::test
