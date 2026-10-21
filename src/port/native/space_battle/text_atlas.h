// Anti-aliased text for the battle screen: the game's fonts (found as port/minwin_impl/text does) rasterised by
// FreeType into a true-colour glyph atlas. The atlas also holds a white block, so shapes and text share one texture
// and batch together.
#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "battle_ui/draw_list.h"
#include "battle_ui/text_measure.h"

namespace reb::native::space_battle
{

class TextAtlas : public battle_ui::TextMeasurer
{
  public:
  TextAtlas();
  ~TextAtlas() override;
  TextAtlas(const TextAtlas&) = delete;
  TextAtlas& operator=(const TextAtlas&) = delete;

  float Width(std::string_view text, float size, battle_ui::FontWeight weight) const override;
  float Ascent(float size) const override;
  float Descent(float size) const override;

  // Appends the glyph quads of a laid-out text command (alignment, truncation) to `vertices`.
  void Append(const battle_ui::DrawText& text, std::vector<SDL_Vertex>& vertices);
  // Glyph and white-block texture coordinates are in texels; divide by Size() for the texture.
  SDL_FPoint WhiteTexel() const;
  int Size() const
  {
    return m_atlasSize;
  }
  // The atlas texture, uploading glyphs added since the last call. Recreated when the renderer changes.
  SDL_Texture* Texture(SDL_Renderer* renderer);
  void ReleaseTexture();

  private:
  struct Glyph {
    float advance = 0.0f;
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    int x = 0; // atlas position
    int y = 0;
    uint32_t index = 0; // the face's glyph index (for kerning)
  };
  struct SizeMetrics {
    float ascent = 0.0f;
    float descent = 0.0f;
  };

  bool EnsureFaces() const;
  void* FaceFor(battle_ui::FontWeight weight) const;
  void SetSize(void* face, int pixels) const;
  const Glyph* GlyphFor(uint32_t codepoint, int pixels, battle_ui::FontWeight weight) const;
  float Kerning(void* face, int pixels, uint32_t left, uint32_t right) const;
  bool Pack(int width, int height, int& x, int& y) const;
  void ResetAtlas() const;
  const SizeMetrics& Metrics(int pixels) const;

  mutable void* m_library = nullptr; // FT_Library
  mutable void* m_regular = nullptr; // FT_Face
  mutable void* m_bold = nullptr;    // FT_Face; null: emboldened regular
  mutable bool m_triedFaces = false;
  mutable std::unordered_map<void*, int> m_faceSize;
  mutable std::unordered_map<uint64_t, Glyph> m_glyphs;
  mutable std::unordered_map<int, SizeMetrics> m_metrics;
  mutable std::vector<uint32_t> m_pixels; // RGBA, white with coverage alpha
  mutable int m_atlasSize = 1024;
  mutable int m_shelfX = 0;
  mutable int m_shelfY = 0;
  mutable int m_shelfHeight = 0;
  mutable bool m_dirty = true;
  SDL_Texture* m_texture = nullptr;
  SDL_Renderer* m_textureRenderer = nullptr;
  int m_textureSize = 0;
};

// UTF-8 to code points; a byte that does not start a valid sequence is read as Windows-1252.
std::vector<uint32_t> DecodeText(std::string_view text);

} // namespace reb::native::space_battle
