#include "port/native/space_battle/sdl_input.h"

#include <cmath>

namespace reb::native::space_battle
{

using battle_ui::Key;

Key KeyFromScancode(SDL_Scancode scancode)
{
  if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
    return static_cast<Key>(static_cast<int>(Key::A) + (scancode - SDL_SCANCODE_A));
  }
  if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
    return battle_ui::DigitKey(1 + (scancode - SDL_SCANCODE_1));
  }
  if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
    return static_cast<Key>(static_cast<int>(Key::F1) + (scancode - SDL_SCANCODE_F1));
  }
  switch (scancode) {
    case SDL_SCANCODE_0:
      return Key::Num0;
    case SDL_SCANCODE_ESCAPE:
      return Key::Escape;
    case SDL_SCANCODE_TAB:
      return Key::Tab;
    case SDL_SCANCODE_SPACE:
      return Key::Space;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
      return Key::Return;
    case SDL_SCANCODE_BACKSPACE:
      return Key::Backspace;
    case SDL_SCANCODE_DELETE:
      return Key::Delete;
    case SDL_SCANCODE_INSERT:
      return Key::Insert;
    case SDL_SCANCODE_HOME:
      return Key::Home;
    case SDL_SCANCODE_END:
      return Key::End;
    case SDL_SCANCODE_PAGEUP:
      return Key::PageUp;
    case SDL_SCANCODE_PAGEDOWN:
      return Key::PageDown;
    case SDL_SCANCODE_LEFT:
      return Key::Left;
    case SDL_SCANCODE_RIGHT:
      return Key::Right;
    case SDL_SCANCODE_UP:
      return Key::Up;
    case SDL_SCANCODE_DOWN:
      return Key::Down;
    case SDL_SCANCODE_MINUS:
      return Key::Minus;
    case SDL_SCANCODE_EQUALS:
      return Key::Equals;
    case SDL_SCANCODE_LEFTBRACKET:
      return Key::LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET:
      return Key::RightBracket;
    case SDL_SCANCODE_COMMA:
      return Key::Comma;
    case SDL_SCANCODE_PERIOD:
      return Key::Period;
    case SDL_SCANCODE_SLASH:
      return Key::Slash;
    case SDL_SCANCODE_GRAVE:
      return Key::Backquote;
    case SDL_SCANCODE_KP_PLUS:
      return Key::KeypadPlus;
    case SDL_SCANCODE_KP_MINUS:
      return Key::KeypadMinus;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
      return Key::Shift;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_RGUI:
      return Key::Ctrl;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
      return Key::Alt;
    default:
      return Key::Unknown;
  }
}

uint8_t ModifiersFrom(SDL_Keymod mod)
{
  uint8_t out = 0;
  if (mod & SDL_KMOD_SHIFT) {
    out |= battle_ui::kModShift;
  }
  if (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) {
    out |= battle_ui::kModCtrl;
  }
  if (mod & SDL_KMOD_ALT) {
    out |= battle_ui::kModAlt;
  }
  return out;
}

battle_ui::Point InputTranslator::ToPixels(float x, float y, SDL_Window* window, SDL_Renderer* renderer) const
{
  int windowWidth = 0;
  int windowHeight = 0;
  int outputWidth = 0;
  int outputHeight = 0;
  if (!window || !renderer || !SDL_GetWindowSize(window, &windowWidth, &windowHeight)
      || !SDL_GetRenderOutputSize(renderer, &outputWidth, &outputHeight) || windowWidth <= 0 || windowHeight <= 0) {
    return {x, y};
  }
  return {x * static_cast<float>(outputWidth) / static_cast<float>(windowWidth),
          y * static_cast<float>(outputHeight) / static_cast<float>(windowHeight)};
}

std::optional<battle_ui::InputEvent> InputTranslator::Translate(const SDL_Event& event, SDL_Window* window,
                                                                SDL_Renderer* renderer,
                                                                const battle_ui::ScreenInfo& screen)
{
  const uint8_t modifiers = ModifiersFrom(SDL_GetModState());
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      return battle_ui::MouseMove{ToPixels(event.motion.x, event.motion.y, window, renderer), m_buttons, modifiers};
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      {
        battle_ui::MouseButton button = battle_ui::MouseButton::Left;
        if (event.button.button == SDL_BUTTON_RIGHT) {
          button = battle_ui::MouseButton::Right;
        } else if (event.button.button == SDL_BUTTON_MIDDLE) {
          button = battle_ui::MouseButton::Middle;
        } else if (event.button.button != SDL_BUTTON_LEFT) {
          return std::nullopt;
        }
        m_buttons
            = event.button.down ? m_buttons | battle_ui::ButtonBit(button) : m_buttons & ~battle_ui::ButtonBit(button);
        SDL_CaptureMouse(m_buttons != 0);
        return battle_ui::MouseButtonEvent{ToPixels(event.button.x, event.button.y, window, renderer), button,
                                           event.button.down, std::max<int>(1, event.button.clicks), modifiers};
      }
    case SDL_EVENT_MOUSE_WHEEL:
      {
        const float flip = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
        const bool precise = event.wheel.y != std::floor(event.wheel.y) || event.wheel.x != std::floor(event.wheel.x);
        return battle_ui::MouseWheel{ToPixels(event.wheel.mouse_x, event.wheel.mouse_y, window, renderer),
                                     event.wheel.x * flip, event.wheel.y * flip, precise, modifiers};
      }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      return battle_ui::KeyEvent{KeyFromScancode(event.key.scancode), event.key.down, event.key.repeat,
                                 ModifiersFrom(event.key.mod)};
    case SDL_EVENT_TEXT_INPUT:
      return battle_ui::TextInput{event.text.text ? event.text.text : ""};
    case SDL_EVENT_WINDOW_MOUSE_LEAVE: // reported as a move off the screen, which ends hovering and edge scrolling
      return battle_ui::MouseMove{{-1.0f, -1.0f}, m_buttons, modifiers};
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MINIMIZED:
      m_buttons = 0;
      SDL_CaptureMouse(false);
      return battle_ui::FocusLost{};
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      return battle_ui::CloseRequested{};
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
      return battle_ui::ScreenChanged{screen};
    default:
      return std::nullopt;
  }
}

} // namespace reb::native::space_battle
