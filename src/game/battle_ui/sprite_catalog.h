#pragma once
#include <cstdint>

#include "battle_ui/draw_list.h"

// Which ship classes the loaded sprite pack can draw. The battle screen emits DrawSprite for those and procedural
// silhouettes for the rest, so a battle always renders without the pack.
namespace reb::battle_ui
{

class SpriteCatalog
{
  public:
  virtual ~SpriteCatalog() = default;
  virtual bool Has(int32_t typeIndex, SpriteView view) const = 0;
  // The hull length the art is drawn at, in meters (the pack's class length; fighters are exaggerated there); 0 when
  // unknown. Sprites are drawn at this length times the zoom.
  virtual float LengthMeters(int32_t /*typeIndex*/) const
  {
    return 0.0f;
  }
};

class NoSprites : public SpriteCatalog
{
  public:
  bool Has(int32_t, SpriteView) const override
  {
    return false;
  }
};

} // namespace reb::battle_ui
