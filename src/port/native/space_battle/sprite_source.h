// Where the executor gets ship images for DrawSprite: the sprite pack (pack_sprites.h), or without one NoSpriteSource,
// which reports every class missing so that the battle screen draws silhouettes instead.
#pragma once
#include <SDL3/SDL.h>

#include <cstdint>

#include "battle_ui/sprite_catalog.h"

namespace reb::native::space_battle
{

struct SpriteFrame {
  SDL_Texture* texture = nullptr;
  SDL_FRect source{};           // texels
  float length = 0.0f;          // the hull's long axis in source texels
  float artHeading = 0.0f;      // screen angle the art faces in the source (0 = bow to the right)
  SDL_FPoint pivot{0.5f, 0.5f}; // the ship's centre within `source`, as fractions
};

class SpriteSource : public battle_ui::SpriteCatalog
{
  public:
  // The frame for a heading (screen radians; for the angled view, the relative heading that picks the frame), chosen
  // for an on-screen hull length in pixels; false when the class or layer is missing.
  virtual bool Frame(SDL_Renderer* renderer, int32_t typeIndex, battle_ui::SpriteView view,
                     battle_ui::SpriteLayer layer, float heading, float length, SpriteFrame& out) = 0;
  // Drops GPU textures, before the renderer goes away or the screen closes.
  virtual void ReleaseTextures() {}
};

class NoSpriteSource : public SpriteSource
{
  public:
  bool Has(int32_t, battle_ui::SpriteView) const override
  {
    return false;
  }
  bool Frame(SDL_Renderer*, int32_t, battle_ui::SpriteView, battle_ui::SpriteLayer, float, float, SpriteFrame&) override
  {
    return false;
  }
};

} // namespace reb::native::space_battle
