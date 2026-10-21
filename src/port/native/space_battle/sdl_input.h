// SDL events -> battle_ui input events, in device pixels.
#pragma once
#include <SDL3/SDL.h>

#include <optional>

#include "battle_ui/input.h"

namespace reb::native::space_battle
{

battle_ui::Key KeyFromScancode(SDL_Scancode scancode);
uint8_t ModifiersFrom(SDL_Keymod mod);

class InputTranslator
{
  public:
  // `window` and `renderer` map window coordinates to output pixels; `screen` is reported on size changes.
  std::optional<battle_ui::InputEvent> Translate(const SDL_Event& event, SDL_Window* window, SDL_Renderer* renderer,
                                                 const battle_ui::ScreenInfo& screen);

  private:
  battle_ui::Point ToPixels(float x, float y, SDL_Window* window, SDL_Renderer* renderer) const;
  uint8_t m_buttons = 0;
};

} // namespace reb::native::space_battle
