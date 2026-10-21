#include "port/native/space_battle/pack_sprites.h"

#include <cstdio>
#include <memory>

namespace reb::native::space_battle
{
namespace
{

using battle_sprites::SpriteSize;

// The small set (4x coarser) serves until it would be enlarged by more than this on screen.
constexpr float kSmallSetMaxScale = 1.25f;

battle_sprites::SpriteView PackView(battle_ui::SpriteView view)
{
  return view == battle_ui::SpriteView::Angled ? battle_sprites::SpriteView::Angled : battle_sprites::SpriteView::Top;
}

} // namespace

PackSpriteSource::PackSpriteSource(battle_sprites::SpritePack pack)
    : m_pack(std::move(pack))
    , m_textures(m_pack.AtlasFiles().size(), nullptr)
{
}

PackSpriteSource::~PackSpriteSource()
{
  ReleaseTextures();
}

bool PackSpriteSource::Has(int32_t typeIndex, battle_ui::SpriteView view) const
{
  const battle_sprites::SpriteClass* spriteClass = m_pack.Find(typeIndex);
  return spriteClass && !spriteClass->Set(PackView(view), SpriteSize::Large).frames.empty();
}

float PackSpriteSource::LengthMeters(int32_t typeIndex) const
{
  const battle_sprites::SpriteClass* spriteClass = m_pack.Find(typeIndex);
  return spriteClass ? spriteClass->lengthMetres : 0.0f;
}

bool PackSpriteSource::Frame(SDL_Renderer* renderer, int32_t typeIndex, battle_ui::SpriteView view,
                             battle_ui::SpriteLayer layer, float heading, float length, SpriteFrame& out)
{
  const battle_sprites::SpriteClass* spriteClass = m_pack.Find(typeIndex);
  if (!spriteClass || layer != battle_ui::SpriteLayer::Color || spriteClass->lengthMetres <= 0.0f) {
    return false;
  }
  const battle_sprites::SpriteView packView = PackView(view);
  const battle_sprites::SpriteSet& small = spriteClass->Set(packView, SpriteSize::Small);
  const bool smallSuffices
      = !small.frames.empty() && length * small.metresPerPixel / spriteClass->lengthMetres <= kSmallSetMaxScale;
  const SpriteSize size = smallSuffices ? SpriteSize::Small : SpriteSize::Large;
  const battle_sprites::SpriteFrame* frame = m_pack.Frame(typeIndex, packView, size, heading);
  if (!frame) {
    return false;
  }
  SDL_Texture* texture = Texture(renderer, frame->atlas);
  if (!texture) {
    return false;
  }
  const float metresPerPixel = spriteClass->Set(packView, size).metresPerPixel;
  out.texture = texture;
  out.source = {static_cast<float>(frame->x), static_cast<float>(frame->y), static_cast<float>(frame->width),
                static_cast<float>(frame->height)};
  out.length = spriteClass->lengthMetres / metresPerPixel;
  out.artHeading = 0.0f;
  out.pivot = {frame->pivotX / static_cast<float>(frame->width), frame->pivotY / static_cast<float>(frame->height)};
  return true;
}

SDL_Texture* PackSpriteSource::Texture(SDL_Renderer* renderer, int atlas)
{
  if (renderer != m_renderer) {
    ReleaseTextures();
    m_renderer = renderer;
  }
  SDL_Texture*& texture = m_textures[static_cast<size_t>(atlas)];
  if (texture) {
    return texture;
  }
  const battle_sprites::RgbaImage* image = m_pack.Atlas(atlas);
  if (!image) {
    return nullptr;
  }
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image->width, image->height);
  if (texture) {
    SDL_UpdateTexture(texture, nullptr, image->pixels.data(), image->width * 4);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
  }
  // The texture holds the pixels now; an atlas is decoded again only if the renderer changes.
  m_pack.ReleaseAtlases();
  return texture;
}

void PackSpriteSource::ReleaseTextures()
{
  for (SDL_Texture*& texture : m_textures) {
    if (texture) {
      SDL_DestroyTexture(texture);
    }
    texture = nullptr;
  }
  m_renderer = nullptr;
}

std::unique_ptr<SpriteSource> LoadConfiguredSprites()
{
  const std::filesystem::path directory = battle_sprites::ConfiguredSpritePackDirectory();
  if (directory.empty()) {
    return std::make_unique<NoSpriteSource>();
  }
  std::string error;
  std::optional<battle_sprites::SpritePack> pack = battle_sprites::SpritePack::Load(directory, &error);
  if (!pack) {
    std::fprintf(stderr, "space battle: sprite pack %s not loaded (%s); drawing silhouettes\n",
                 directory.string().c_str(), error.c_str());
    return std::make_unique<NoSpriteSource>();
  }
  return std::make_unique<PackSpriteSource>(std::move(*pack));
}

} // namespace reb::native::space_battle
