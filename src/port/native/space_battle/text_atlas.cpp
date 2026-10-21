#include "port/native/space_battle/text_atlas.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <array>
#include <cmath>
#include <cstdio>

#include "port/minwin_impl/text/font_files.h"

namespace reb::native::space_battle
{
namespace
{

using battle_ui::Align;
using battle_ui::FontWeight;
using battle_ui::TextOverflow;
using battle_ui::VAlign;

constexpr int kWhiteBlock = 4;
constexpr int kPadding = 1;
constexpr int kMaxAtlas = 4096;
constexpr uint32_t kEllipsis = 0x2026;

FT_Face AsFace(void* face)
{
  return static_cast<FT_Face>(face);
}

int PixelSize(float size)
{
  return std::max(1, static_cast<int>(std::lround(size)));
}

uint32_t FromWindows1252(uint8_t byte)
{
  static constexpr std::array<uint16_t, 32> kHigh{0x20ac, 0x81,   0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
                                                  0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8d,   0x017d, 0x8f,
                                                  0x90,   0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
                                                  0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x9d,   0x017e, 0x0178};
  return byte >= 0x80 && byte < 0xa0 ? kHigh[byte - 0x80] : byte;
}

SDL_FColor ToFColor(battle_ui::Color color)
{
  return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
}

} // namespace

std::vector<uint32_t> DecodeText(std::string_view text)
{
  std::vector<uint32_t> out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    const auto lead = static_cast<uint8_t>(text[i]);
    int length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xe ? 3 : (lead >> 3) == 0x1e ? 4 : 0;
    uint32_t codepoint = length == 1 ? lead : length == 2 ? lead & 0x1f : length == 3 ? lead & 0x0f : lead & 0x07;
    bool valid = length > 0 && i + length <= text.size();
    for (int k = 1; valid && k < length; ++k) {
      const auto next = static_cast<uint8_t>(text[i + k]);
      valid = (next & 0xc0) == 0x80;
      codepoint = (codepoint << 6) | (next & 0x3f);
    }
    if (valid && length > 1 && codepoint < (length == 2 ? 0x80U : length == 3 ? 0x800U : 0x10000U)) {
      valid = false;
    }
    if (!valid) {
      out.push_back(FromWindows1252(lead));
      ++i;
      continue;
    }
    out.push_back(codepoint);
    i += length;
  }
  return out;
}

TextAtlas::TextAtlas()
{
  ResetAtlas();
}

TextAtlas::~TextAtlas()
{
  ReleaseTexture();
  if (m_bold) {
    FT_Done_Face(AsFace(m_bold));
  }
  if (m_regular) {
    FT_Done_Face(AsFace(m_regular));
  }
  if (m_library) {
    FT_Done_FreeType(static_cast<FT_Library>(m_library));
  }
}

bool TextAtlas::EnsureFaces() const
{
  if (m_triedFaces) {
    return m_regular != nullptr;
  }
  m_triedFaces = true;
  FT_Library library = nullptr;
  if (FT_Init_FreeType(&library) != 0) {
    return false;
  }
  m_library = library;
  const mwtext::FontFiles files = mwtext::FindArial();
  FT_Face face = nullptr;
  if (files.regular.empty() || FT_New_Face(library, files.regular.c_str(), 0, &face) != 0) {
    std::fprintf(stderr, "space battle: no font found; text will not be drawn\n");
    return false;
  }
  m_regular = face;
  if (!files.bold.empty() && FT_New_Face(library, files.bold.c_str(), 0, &face) == 0) {
    m_bold = face;
  }
  return true;
}

void* TextAtlas::FaceFor(FontWeight weight) const
{
  return weight == FontWeight::Bold && m_bold ? m_bold : m_regular;
}

void TextAtlas::SetSize(void* face, int pixels) const
{
  int& current = m_faceSize[face];
  if (current != pixels) {
    FT_Set_Pixel_Sizes(AsFace(face), 0, static_cast<FT_UInt>(pixels));
    current = pixels;
  }
}

const TextAtlas::SizeMetrics& TextAtlas::Metrics(int pixels) const
{
  if (const auto found = m_metrics.find(pixels); found != m_metrics.end()) {
    return found->second;
  }
  SizeMetrics metrics{pixels * 0.9f, pixels * 0.22f};
  if (EnsureFaces()) {
    SetSize(m_regular, pixels);
    const FT_Size_Metrics& size = AsFace(m_regular)->size->metrics;
    metrics = {static_cast<float>(size.ascender) / 64.0f, -static_cast<float>(size.descender) / 64.0f};
  }
  return m_metrics[pixels] = metrics;
}

float TextAtlas::Ascent(float size) const
{
  return Metrics(PixelSize(size)).ascent;
}

float TextAtlas::Descent(float size) const
{
  return Metrics(PixelSize(size)).descent;
}

void TextAtlas::ResetAtlas() const
{
  m_pixels.assign(static_cast<size_t>(m_atlasSize) * m_atlasSize, 0);
  for (int y = 0; y < kWhiteBlock; ++y) {
    for (int x = 0; x < kWhiteBlock; ++x) {
      m_pixels[static_cast<size_t>(y) * m_atlasSize + x] = 0xffffffffU;
    }
  }
  m_glyphs.clear();
  m_shelfX = kWhiteBlock + kPadding;
  m_shelfY = 0;
  m_shelfHeight = kWhiteBlock;
  m_dirty = true;
}

bool TextAtlas::Pack(int width, int height, int& x, int& y) const
{
  if (m_shelfX + width > m_atlasSize) {
    m_shelfY += m_shelfHeight + kPadding;
    m_shelfX = 0;
    m_shelfHeight = 0;
  }
  if (m_shelfY + height > m_atlasSize) {
    if (m_atlasSize >= kMaxAtlas) {
      return false;
    }
    const int oldSize = m_atlasSize;
    std::vector<uint32_t> old = std::move(m_pixels);
    m_atlasSize *= 2;
    m_pixels.assign(static_cast<size_t>(m_atlasSize) * m_atlasSize, 0);
    for (int row = 0; row < oldSize; ++row) {
      std::copy_n(old.begin() + static_cast<ptrdiff_t>(row) * oldSize, oldSize,
                  m_pixels.begin() + static_cast<ptrdiff_t>(row) * m_atlasSize);
    }
    m_shelfX = 0;
    m_shelfY = oldSize;
    m_shelfHeight = 0;
  }
  x = m_shelfX;
  y = m_shelfY;
  m_shelfX += width + kPadding;
  m_shelfHeight = std::max(m_shelfHeight, height);
  return true;
}

const TextAtlas::Glyph* TextAtlas::GlyphFor(uint32_t codepoint, int pixels, FontWeight weight) const
{
  const uint64_t key
      = codepoint | (static_cast<uint64_t>(pixels) << 32) | (static_cast<uint64_t>(weight == FontWeight::Bold) << 48);
  if (const auto found = m_glyphs.find(key); found != m_glyphs.end()) {
    return &found->second;
  }
  if (!EnsureFaces()) {
    return nullptr;
  }
  void* face = FaceFor(weight);
  const bool synthesise = weight == FontWeight::Bold && !m_bold;
  SetSize(face, pixels);
  FT_Face ft = AsFace(face);
  FT_UInt index = FT_Get_Char_Index(ft, codepoint);
  if (index == 0 && codepoint != ' ') {
    index = FT_Get_Char_Index(ft, '?');
  }
  Glyph glyph;
  glyph.index = index;
  if (FT_Load_Glyph(ft, index, FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT) != 0) {
    return &(m_glyphs[key] = glyph);
  }
  FT_GlyphSlot slot = ft->glyph;
  float extraAdvance = 0.0f;
  if (synthesise && slot->format == FT_GLYPH_FORMAT_OUTLINE) {
    const FT_Pos strength = std::max<FT_Pos>(32, pixels * 3);
    FT_Outline_EmboldenXY(&slot->outline, strength, strength / 2);
    extraAdvance = static_cast<float>(strength) / 64.0f;
  }
  if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
    return &(m_glyphs[key] = glyph);
  }
  const FT_Bitmap& bitmap = slot->bitmap;
  glyph.advance = static_cast<float>(slot->advance.x) / 64.0f + extraAdvance;
  glyph.left = slot->bitmap_left;
  glyph.top = slot->bitmap_top;
  glyph.width = static_cast<int>(bitmap.width);
  glyph.height = static_cast<int>(bitmap.rows);
  if (glyph.width > 0 && glyph.height > 0) {
    if (!Pack(glyph.width, glyph.height, glyph.x, glyph.y)) {
      ResetAtlas();
      Pack(glyph.width, glyph.height, glyph.x, glyph.y);
    }
    for (int row = 0; row < glyph.height; ++row) {
      const uint8_t* source = bitmap.buffer + static_cast<ptrdiff_t>(row) * bitmap.pitch;
      uint32_t* target = m_pixels.data() + static_cast<size_t>(glyph.y + row) * m_atlasSize + glyph.x;
      for (int column = 0; column < glyph.width; ++column) {
        target[column] = (static_cast<uint32_t>(source[column]) << 24) | 0x00ffffffU;
      }
    }
    m_dirty = true;
  }
  return &(m_glyphs[key] = glyph);
}

float TextAtlas::Kerning(void* face, int pixels, uint32_t left, uint32_t right) const
{
  if (!left || !right || !FT_HAS_KERNING(AsFace(face))) {
    return 0.0f;
  }
  SetSize(face, pixels); // FT_Get_Kerning scales by the face's current size
  FT_Vector delta{};
  FT_Get_Kerning(AsFace(face), left, right, FT_KERNING_DEFAULT, &delta);
  return static_cast<float>(delta.x) / 64.0f;
}

float TextAtlas::Width(std::string_view text, float size, FontWeight weight) const
{
  const int pixels = PixelSize(size);
  float width = 0.0f;
  uint32_t previous = 0;
  for (const uint32_t codepoint : DecodeText(text)) {
    const Glyph* glyph = GlyphFor(codepoint, pixels, weight);
    if (!glyph) {
      return static_cast<float>(text.size()) * size * 0.55f;
    }
    width += Kerning(FaceFor(weight), pixels, previous, glyph->index) + glyph->advance;
    previous = glyph->index;
  }
  return width;
}

void TextAtlas::Append(const battle_ui::DrawText& text, std::vector<SDL_Vertex>& vertices)
{
  const int pixels = PixelSize(text.size);
  std::vector<uint32_t> codepoints = DecodeText(text.text);
  struct Placed {
    Glyph glyph; // a copy: a full atlas resets the glyph cache while a line is being laid out
    float pen;
  };
  const auto layout = [&](const std::vector<uint32_t>& cps, std::vector<Placed>& placed) {
    placed.clear();
    float pen = 0.0f;
    uint32_t previous = 0;
    for (const uint32_t codepoint : cps) {
      const Glyph* glyph = GlyphFor(codepoint, pixels, text.weight);
      if (!glyph) {
        continue;
      }
      pen += Kerning(FaceFor(text.weight), pixels, previous, glyph->index);
      placed.push_back({*glyph, pen});
      pen += glyph->advance;
      previous = glyph->index;
    }
    return pen;
  };
  std::vector<Placed> placed;
  float width = layout(codepoints, placed);
  if (text.maxWidth > 0.0f && width > text.maxWidth) {
    if (text.overflow == TextOverflow::Ellipsis) {
      while (!codepoints.empty() && width > text.maxWidth) {
        codepoints.pop_back();
        while (!codepoints.empty() && codepoints.back() == ' ') {
          codepoints.pop_back();
        }
        std::vector<uint32_t> trial = codepoints;
        trial.push_back(kEllipsis);
        width = layout(trial, placed);
      }
    } else {
      std::erase_if(placed, [&](const Placed& p) {
        return p.pen + p.glyph.advance > text.maxWidth + 0.5f;
      });
      width = std::min(width, text.maxWidth);
    }
  }
  if (placed.empty()) {
    return;
  }
  const SizeMetrics& metrics = Metrics(pixels);
  float x = text.position.x;
  if (text.align == Align::Center) {
    x -= width * 0.5f;
  } else if (text.align == Align::End) {
    x -= width;
  }
  float baseline = text.position.y;
  switch (text.valign) {
    case VAlign::Top:
      baseline += metrics.ascent;
      break;
    case VAlign::Middle:
      baseline += (metrics.ascent - metrics.descent) * 0.5f;
      break;
    case VAlign::Bottom:
      baseline -= metrics.descent;
      break;
    case VAlign::Baseline:
      break;
  }
  x = std::round(x);
  baseline = std::round(baseline);
  const SDL_FColor color = ToFColor(text.color);
  for (const Placed& p : placed) {
    const Glyph& g = p.glyph;
    if (g.width == 0 || g.height == 0) {
      continue;
    }
    const float left = std::round(x + p.pen) + static_cast<float>(g.left);
    const float top = baseline - static_cast<float>(g.top);
    const float right = left + static_cast<float>(g.width);
    const float bottom = top + static_cast<float>(g.height);
    const float u0 = static_cast<float>(g.x);
    const float v0 = static_cast<float>(g.y);
    const float u1 = u0 + static_cast<float>(g.width);
    const float v1 = v0 + static_cast<float>(g.height);
    const SDL_Vertex a{{left, top}, color, {u0, v0}};
    const SDL_Vertex b{{right, top}, color, {u1, v0}};
    const SDL_Vertex c{{right, bottom}, color, {u1, v1}};
    const SDL_Vertex d{{left, bottom}, color, {u0, v1}};
    vertices.insert(vertices.end(), {a, b, c, a, c, d});
  }
}

SDL_FPoint TextAtlas::WhiteTexel() const
{
  return {kWhiteBlock * 0.5f, kWhiteBlock * 0.5f};
}

SDL_Texture* TextAtlas::Texture(SDL_Renderer* renderer)
{
  if (m_texture && (m_textureRenderer != renderer || m_textureSize != m_atlasSize)) {
    ReleaseTexture();
  }
  if (!m_texture) {
    m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, m_atlasSize, m_atlasSize);
    if (!m_texture) {
      return nullptr;
    }
    SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_LINEAR);
    m_textureRenderer = renderer;
    m_textureSize = m_atlasSize;
    m_dirty = true;
  }
  if (m_dirty) {
    SDL_UpdateTexture(m_texture, nullptr, m_pixels.data(), m_atlasSize * static_cast<int>(sizeof(uint32_t)));
    m_dirty = false;
  }
  return m_texture;
}

void TextAtlas::ReleaseTexture()
{
  if (m_texture) {
    SDL_DestroyTexture(m_texture);
  }
  m_texture = nullptr;
  m_textureRenderer = nullptr;
  m_textureSize = 0;
}

} // namespace reb::native::space_battle
