// Lets one port-side client take over the game window, as the live battle screen does
// (docs/design/tactical_combat.md §11.6). While a client is active, SDL input, window and quit events go to it
// instead of the minwin window tree, and minwin stops presenting its palette canvas; the strategic game keeps
// running underneath, and its windows are repainted and presented again when the client ends.
#pragma once
#include <SDL3/SDL.h>

namespace minwin::takeover
{

class Client
{
  public:
  virtual ~Client() = default;
  virtual void HandleEvent(const SDL_Event& event) = 0;
};

// Brings the platform up if it is not yet; false when there is no window.
bool Begin(Client& client);
// A request to close the window (or quit) made while the client was active reaches the game here, once the client
// has had its say.
void End();
bool Active();
SDL_Window* Window();
SDL_Renderer* Renderer();
// One pass of the platform pump (events, audio, timers) for a loop outside the game's message loop.
void Pump();

} // namespace minwin::takeover
