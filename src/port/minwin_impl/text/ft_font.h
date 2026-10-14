// A GDI font realised with FreeType: pixel size from the LOGFONT, text metrics, and 1-bit hinted glyph bitmaps (the 8-bit
// palettised display has no anti-aliasing; Wine renders GGO_BITMAP glyphs the same way).
#pragma once
#include <stdint.h>

#include <vector>

#include "port/minwin/gdi32.h"

namespace mwtext
{

struct Glyph
{
  bool loaded = false;
  int advance = 0; // pixels: the pen step when drawing (hinted for the monochrome target)
  int extentAdvance = 0; // pixels: what GetTextExtentPoint32 and DrawText layout count
  int left = 0; // bitmap origin relative to the glyph origin (x right)
  int top = 0; // y of the bitmap's first row above the baseline
  int width = 0, height = 0;
  int pitch = 0;
  std::vector<uint8_t> bits; // 1 bpp, MSB first, `height` rows of `pitch` bytes
  bool Pixel(int x, int y) const
  {
    return (bits[static_cast<size_t>(y) * pitch + (x >> 3)] >> (7 - (x & 7))) & 1;
  }
};

struct TextMetrics
{
  int height = 0, ascent = 0, descent = 0, internalLeading = 0, externalLeading = 0;
  int aveCharWidth = 0, maxCharWidth = 0, weight = 400;
  int firstChar = 0, lastChar = 0, defaultChar = 0, breakChar = 0;
  int pitchAndFamily = 0, charSet = 0;
};

class FontInstance
{
public:
  // Null when no font file can be found or loaded.
  static FontInstance* Create(const LOGFONTA& lf);
  ~FontInstance();

  const TextMetrics& Metrics() const
  {
    return m_metrics;
  }
  const Glyph& GetGlyph(uint8_t ansiChar);
  int Ppem() const
  {
    return m_ppem;
  }

private:
  FontInstance() = default;
  bool Realize(const LOGFONTA& lf);
  void ComputeMetrics(int weight);
  void LoadGlyph(uint8_t ch, Glyph* out);

  void* m_face = nullptr; // FT_Face
  int m_ppem = 0;
  bool m_synthBold = false;
  TextMetrics m_metrics;
  Glyph m_glyphs[256];
};

// Windows-1252 byte to Unicode.
uint32_t AnsiToUnicode(uint8_t c);

} // namespace mwtext
