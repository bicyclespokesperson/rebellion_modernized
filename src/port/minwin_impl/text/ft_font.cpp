#include "ft_font.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_OUTLINE_H
#include FT_BITMAP_H

#include <cstdlib>
#include <cstring>

#include <map>
#include <string>

#include "font_files.h"

namespace mwtext
{

namespace
{

FT_Library Library()
{
  static FT_Library lib = [] {
    FT_Library l = nullptr;
    FT_Init_FreeType(&l);
    return l;
  }();
  return lib;
}

struct FileInfo {
  std::string regular, bold;
  bool searched = false;
};

FileInfo& Files()
{
  static FileInfo info;
  if (!info.searched) {
    FontFiles f = FindArial();
    info.regular = f.regular;
    info.bold = f.bold;
    info.searched = true;
  }
  return info;
}

int MulDivRound(int a, int b, int c)
{
  if (c == 0) {
    return -1;
  }
  long long n = static_cast<long long>(a) * b;
  return static_cast<int>(n >= 0 ? (n + c / 2) / c : (n - c / 2) / c);
}

int Round26_6(FT_Long v)
{
  return static_cast<int>((v + 32) >> 6);
}

// Monochrome target: hinted for 1-bit rendering, the form of text Wine produces on a palettised display.
constexpr int kLoadFlags = FT_LOAD_DEFAULT | FT_LOAD_TARGET_MONO;

} // namespace

uint32_t AnsiToUnicode(uint8_t c)
{
  static const uint16_t kHigh[32]
      = {0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160,
         0x2039, 0x0152, 0x008d, 0x017d, 0x008f, 0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022,
         0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178};
  return (c >= 0x80 && c < 0xa0) ? kHigh[c - 0x80] : c;
}

FontInstance::~FontInstance()
{
  if (m_face) {
    FT_Done_Face(static_cast<FT_Face>(m_face));
  }
}

FontInstance* FontInstance::Create(const LOGFONTA& lf)
{
  auto* f = new FontInstance;
  if (!f->Realize(lf)) {
    delete f;
    return nullptr;
  }
  return f;
}

bool FontInstance::Realize(const LOGFONTA& lf)
{
  FileInfo& files = Files();
  bool wantBold = lf.lfWeight >= 600;
  const std::string* path = &files.regular;
  if (wantBold && !files.bold.empty()) {
    path = &files.bold;
  }
  m_synthBold = wantBold && (path == &files.regular);
  if (path->empty() || !Library()) {
    return false;
  }
  FT_Face face = nullptr;
  if (FT_New_Face(Library(), path->c_str(), 0, &face) != 0) {
    return false;
  }
  m_face = face;
  auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
  auto* hhea = static_cast<TT_HoriHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_HHEA));
  int upem = face->units_per_EM;
  int cell = os2 ? os2->usWinAscent + os2->usWinDescent : 0;
  if (cell == 0 && hhea) {
    cell = hhea->Ascender - hhea->Descender;
  }
  if (cell == 0) {
    cell = upem;
  }
  int height = lf.lfHeight;
  if (height == 0) {
    height = 16;
  }
  m_ppem = height > 0 ? MulDivRound(upem, height, cell) : -height;
  if (m_ppem < 1) {
    m_ppem = 1;
  }
  if (m_ppem > 65535) {
    m_ppem = 65535;
  }
  if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(m_ppem)) != 0) {
    return false;
  }
  ComputeMetrics(wantBold ? 700 : 400);
  // The rounded em size can make the cell taller than requested; step down until it fits (as Wine's font mapper does).
  while (height > 0 && m_metrics.height > height && m_ppem > 1) {
    --m_ppem;
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(m_ppem)) != 0) {
      return false;
    }
    ComputeMetrics(wantBold ? 700 : 400);
  }
  return true;
}

void FontInstance::ComputeMetrics(int weight)
{
  auto face = static_cast<FT_Face>(m_face);
  auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
  auto* hhea = static_cast<TT_HoriHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_HHEA));
  FT_Fixed ys = face->size->metrics.y_scale, xs = face->size->metrics.x_scale;
  int winAscent = os2 ? os2->usWinAscent : (hhea ? hhea->Ascender : face->ascender);
  int winDescent = os2 ? os2->usWinDescent : (hhea ? -hhea->Descender : -face->descender);
  TextMetrics& m = m_metrics;
  m.ascent = Round26_6(FT_MulFix(winAscent, ys));
  m.descent = Round26_6(FT_MulFix(winDescent, ys));
  m.height = m.ascent + m.descent;
  m.internalLeading = Round26_6(FT_MulFix(winAscent + winDescent - face->units_per_EM, ys));
  m.externalLeading = hhea ? Round26_6(FT_MulFix(hhea->Line_Gap, ys)) : 0;
  if (m.externalLeading < 0) {
    m.externalLeading = 0;
  }
  m.aveCharWidth = os2 ? Round26_6(FT_MulFix(os2->xAvgCharWidth, xs)) : m.height / 2;
  m.maxCharWidth = Round26_6(FT_MulFix(face->bbox.xMax - face->bbox.xMin, xs));
  m.weight = weight;
  m.firstChar = 30;
  m.lastChar = 255;
  m.defaultChar = 31;
  m.breakChar = 32;
  m.pitchAndFamily = 0x27; // FF_SWISS | TMPF_TRUETYPE | TMPF_VECTOR | TMPF_FIXED_PITCH (variable pitch)
  m.charSet = 0;
}

void FontInstance::LoadGlyph(uint8_t ch, Glyph* out)
{
  auto face = static_cast<FT_Face>(m_face);
  out->loaded = true;
  FT_UInt index = FT_Get_Char_Index(face, AnsiToUnicode(ch));
  if (FT_Load_Glyph(face, index, FT_LOAD_DEFAULT) == 0) {
    out->extentAdvance = Round26_6(face->glyph->advance.x) + (m_synthBold ? 1 : 0);
  }
  if (FT_Load_Glyph(face, index, kLoadFlags) != 0) {
    return;
  }
  FT_GlyphSlot slot = face->glyph;
  if (m_synthBold && slot->format == FT_GLYPH_FORMAT_OUTLINE) {
    FT_Outline_Embolden(&slot->outline, 1 << 6);
    slot->metrics.horiAdvance += 1 << 6;
    slot->advance.x += 1 << 6;
  }
  out->advance = Round26_6(slot->advance.x);
  if (FT_Render_Glyph(slot, FT_RENDER_MODE_MONO) != 0) {
    return;
  }
  const FT_Bitmap& bm = slot->bitmap;
  out->left = slot->bitmap_left;
  out->top = slot->bitmap_top;
  out->width = static_cast<int>(bm.width);
  out->height = static_cast<int>(bm.rows);
  out->pitch = bm.pitch < 0 ? -bm.pitch : bm.pitch;
  out->bits.assign(static_cast<size_t>(out->pitch) * out->height, 0);
  for (int y = 0; y < out->height; ++y) {
    const unsigned char* src = bm.buffer + (bm.pitch < 0 ? (out->height - 1 - y) : y) * out->pitch;
    memcpy(&out->bits[static_cast<size_t>(y) * out->pitch], src, out->pitch);
  }
}

const Glyph& FontInstance::GetGlyph(uint8_t ch)
{
  Glyph& g = m_glyphs[ch];
  if (!g.loaded) {
    LoadGlyph(ch, &g);
  }
  return g;
}

} // namespace mwtext
