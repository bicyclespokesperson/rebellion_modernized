#pragma once
#include <string_view>

#include "battle_ui/draw_list.h"

// Text metrics for layout. The port implements it with the same FreeType faces the executor draws with; tests use a
// fixed-advance stand-in.
namespace reb::battle_ui
{

class TextMeasurer
{
  public:
  virtual ~TextMeasurer() = default;
  virtual float Width(std::string_view text, float size, FontWeight weight = FontWeight::Regular) const = 0;
  virtual float Ascent(float size) const = 0;  // above the baseline
  virtual float Descent(float size) const = 0; // below the baseline, positive
  float LineHeight(float size) const
  {
    return Ascent(size) + Descent(size);
  }
};

// Approximates proportional text with a fixed advance per character; for tests and for running without fonts.
class FixedTextMeasurer : public TextMeasurer
{
  public:
  float Width(std::string_view text, float size, FontWeight weight = FontWeight::Regular) const override
  {
    return static_cast<float>(text.size()) * size * (weight == FontWeight::Bold ? 0.6f : 0.55f);
  }
  float Ascent(float size) const override
  {
    return size * 0.9f;
  }
  float Descent(float size) const override
  {
    return size * 0.25f;
  }
};

} // namespace reb::battle_ui
