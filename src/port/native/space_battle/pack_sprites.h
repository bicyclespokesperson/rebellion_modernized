// The tactical sprite pack (sprite_pack.h) as the executor's sprite source: a class's atlases become SDL textures the
// first time one of its frames is drawn, and each draw picks the small or large frame set by the on-screen size.
#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "port/native/space_battle/sprite_pack.h"
#include "port/native/space_battle/sprite_source.h"

namespace reb::native::space_battle
{

class PackSpriteSource : public SpriteSource
{
  public:
  explicit PackSpriteSource(battle_sprites::SpritePack pack);
  ~PackSpriteSource() override;
  PackSpriteSource(const PackSpriteSource&) = delete;
  PackSpriteSource& operator=(const PackSpriteSource&) = delete;

  bool Has(int32_t typeIndex, battle_ui::SpriteView view) const override;
  float LengthMeters(int32_t typeIndex) const override;
  bool Frame(SDL_Renderer* renderer, int32_t typeIndex, battle_ui::SpriteView view, battle_ui::SpriteLayer layer,
             float heading, float length, SpriteFrame& out) override;
  void ReleaseTextures() override;

  private:
  SDL_Texture* Texture(SDL_Renderer* renderer, int atlas);

  battle_sprites::SpritePack m_pack;
  std::vector<SDL_Texture*> m_textures; // by atlas id
  SDL_Renderer* m_renderer = nullptr;
};

// The pack configured by tactical_sprite_dir / REB_TACTICAL_SPRITE_DIR as a sprite source, or a source without any
// classes when none is configured or it does not load (the reason goes to stderr).
std::unique_ptr<SpriteSource> LoadConfiguredSprites();

} // namespace reb::native::space_battle
