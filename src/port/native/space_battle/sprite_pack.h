#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The tactical battle sprite pack that tools/build_tactical_sprites.py renders (see docs/tactical_sprites.md): a directory holding
// manifest.txt and QOI atlases. Each ship class (tactical ship-type index) has a top view, one frame that the game
// rotates, and an angled view of frames at evenly spaced headings, each at a large and a small size.
//
// Conventions, set by the renderer (tools/tactical_sprites_bpy.py):
// - Headings are radians, 0 = +x, counter-clockwise seen from above, as in the simulator.
// - Top view: the frame shows heading 0, bow toward image right and port toward image top. To draw heading h with
//   world +y up on screen, rotate it counter-clockwise by h about the pivot (SDL_RenderTextureRotated takes
//   clockwise degrees, so pass -h in degrees).
// - Angled view: the camera looks toward world +y, pitched SpritePack::PitchDegrees() below the horizon; frame k
//   of n shows heading 2*pi*k/n, so frame n/4 points away from the camera. Pass the heading relative to the camera's
//   yaw. A battle-plane point (x, y) projects to screen (x, -y * sin(pitch)) times pixels per metre.
// - The pivot is where the unit's position falls, in pixels from the frame rectangle's top-left corner.
// - Pixels are RGBA bytes (SDL_PIXELFORMAT_RGBA32) with straight alpha. Each rectangle has a transparent one-pixel
//   border carrying the neighbouring colours, so filtered scaling has no dark fringe.
namespace reb::battle_sprites
{

enum class SpriteView : uint8_t { Top, Angled };
enum class SpriteSize : uint8_t { Large, Small };

struct RgbaImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels; // width * height * 4 bytes, rows top to bottom
};

// Decodes a QOI image (https://qoiformat.org) to RGBA. Returns false and leaves `image` empty on malformed input.
bool DecodeQoi(std::span<const uint8_t> bytes, RgbaImage& image);

struct SpriteFrame {
  int atlas = 0;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  float pivotX = 0.0f;
  float pivotY = 0.0f;
};

struct SpriteSet {
  float metresPerPixel = 0.0f;
  std::vector<SpriteFrame> frames; // in heading order; one for the top view
};

struct SpriteClass {
  int typeIndex = -1;
  std::string slug;
  std::string name;
  std::string origin; // "original" (TACTICAL.DLL mesh), "remastered" (Blender model) or "procedural" (recipe)
  float lengthMetres = 0.0f;
  std::array<std::array<SpriteSet, 2>, 2> sets; // [view][size]

  const SpriteSet& Set(SpriteView view, SpriteSize size) const
  {
    return sets[static_cast<size_t>(view)][static_cast<size_t>(size)];
  }
};

struct AtlasFile {
  std::string file;
  int width = 0;
  int height = 0;
};

class SpritePack
{
  public:
  // Reads and validates manifest.txt; atlases are decoded on demand. Returns nothing, with a reason in `error`, when
  // the manifest is missing, malformed or inconsistent.
  static std::optional<SpritePack> Load(const std::filesystem::path& directory, std::string* error = nullptr);

  const SpriteClass* Find(int typeIndex) const;
  // The frame nearest `heading`; nullptr when the pack has no such class or set.
  const SpriteFrame* Frame(int typeIndex, SpriteView view, SpriteSize size, float heading) const;
  // The atlases a class uses at one size, to load just the classes present in a battle.
  std::vector<int> AtlasesFor(int typeIndex, SpriteSize size) const;
  // Decodes an atlas on first use and keeps it, or the failure, until ReleaseAtlases(). nullptr when the file is
  // missing or corrupt.
  const RgbaImage* Atlas(int atlas);
  void ReleaseAtlases();

  const std::vector<SpriteClass>& Classes() const
  {
    return m_classes;
  }
  const std::vector<AtlasFile>& AtlasFiles() const
  {
    return m_atlasFiles;
  }
  float PitchDegrees() const
  {
    return m_pitchDegrees;
  }

  private:
  std::filesystem::path m_directory;
  float m_pitchDegrees = 50.0f;
  std::vector<SpriteClass> m_classes; // sorted by type index
  std::vector<AtlasFile> m_atlasFiles;
  std::vector<std::optional<RgbaImage>> m_atlases;
  std::vector<bool> m_atlasFailed;
};

// The nearest of `count` evenly spaced headings to `heading`, as an index in [0, count).
int NearestHeadingIndex(float heading, int count);

// The pack directory the game should use: config key tactical_sprite_dir (relative to the config file), else
// $REB_TACTICAL_SPRITE_DIR; empty when neither is set. launch_game.sh exports the variable for the default
// work/hd/tactical/pack when that exists.
std::filesystem::path ConfiguredSpritePackDirectory();

} // namespace reb::battle_sprites
