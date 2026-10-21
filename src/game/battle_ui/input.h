#pragma once
#include <cstdint>
#include <string>
#include <variant>

#include "battle_ui/ui_types.h"

// Abstract input for the battle screen. The port translates SDL events into these; tests build them directly.
// Positions are device pixels (the same space as the draw list).
namespace reb::battle_ui
{

// The size of the output and how large the UI should be drawn on it.
struct ScreenInfo {
  int width = 1280;
  int height = 720;
  float pixelDensity = 1.0f; // device pixels per window point
  float uiScale = 1.0f;      // multiplies every UI size (text, panels, markers)
  friend bool operator==(const ScreenInfo&, const ScreenInfo&) = default;
};

enum Modifier : uint8_t {
  kModShift = 1U << 0,
  kModCtrl = 1U << 1, // also set for Cmd on macOS, so Ctrl-click and Ctrl+number work the platform's way
  kModAlt = 1U << 2,
};

enum class MouseButton : uint8_t { Left, Right, Middle };

inline constexpr uint8_t ButtonBit(MouseButton button)
{
  return static_cast<uint8_t>(1U << static_cast<uint8_t>(button));
}

enum class Key : uint8_t {
  Unknown,
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,
  Num0,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  Escape,
  Tab,
  Space,
  Return,
  Backspace,
  Delete,
  Insert,
  Home,
  End,
  PageUp,
  PageDown,
  Left,
  Right,
  Up,
  Down,
  Minus,
  Equals,
  LeftBracket,
  RightBracket,
  Comma,
  Period,
  Slash,
  Backquote,
  KeypadPlus,
  KeypadMinus,
  Shift,
  Ctrl,
  Alt,
};

inline constexpr Key DigitKey(int digit)
{
  return static_cast<Key>(static_cast<int>(Key::Num0) + digit);
}

// -1 when the key is not a digit.
inline constexpr int KeyDigit(Key key)
{
  const int value = static_cast<int>(key) - static_cast<int>(Key::Num0);
  return value >= 0 && value <= 9 ? value : -1;
}

// A position outside the screen means the mouse left the window.
struct MouseMove {
  Point position;
  uint8_t buttons = 0; // ButtonBit mask of the buttons held
  uint8_t modifiers = 0;
};

struct MouseButtonEvent {
  Point position;
  MouseButton button = MouseButton::Left;
  bool down = true;
  int clicks = 1; // 2 for a double-click
  uint8_t modifiers = 0;
};

struct MouseWheel {
  Point position;
  float dx = 0.0f;
  float dy = 0.0f;      // positive = away from the user (zoom in)
  bool precise = false; // a trackpad's continuous scrolling rather than wheel notches
  uint8_t modifiers = 0;
};

struct KeyEvent {
  Key key = Key::Unknown;
  bool down = true;
  bool repeat = false;
  uint8_t modifiers = 0;
};

struct TextInput {
  std::string text; // UTF-8
};

// The window lost the keyboard focus: held keys and buttons are released without their own events.
struct FocusLost {
};

// The user asked to close the window.
struct CloseRequested {
};

struct ScreenChanged {
  ScreenInfo screen;
};

using InputEvent = std::variant<MouseMove, MouseButtonEvent, MouseWheel, KeyEvent, TextInput, FocusLost, CloseRequested,
                                ScreenChanged>;

} // namespace reb::battle_ui
