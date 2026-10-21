#include "battle_ui/layout.h"

namespace reb::battle_ui
{

ScreenLayout ComputeLayout(const ScreenInfo& screen)
{
  const float s = screen.uiScale;
  const float width = static_cast<float>(screen.width);
  const float height = static_cast<float>(screen.height);
  const float margin = 8.0f * s;
  const float topBarHeight = 34.0f * s;
  const float sideWidth = std::min(240.0f * s, width * 0.2f);
  const float cardHeight = 118.0f * s;
  const float infoWidth = std::min(330.0f * s, width * 0.32f);
  const float infoHeight = 168.0f * s;

  ScreenLayout layout;
  layout.screen = {0.0f, 0.0f, width, height};
  layout.topBar = {0.0f, 0.0f, width, topBarHeight};
  layout.view = {0.0f, topBarHeight, width, height - topBarHeight};
  const float columnsTop = topBarHeight + margin;
  const float columnsBottom = height - cardHeight - 2.0f * margin;
  layout.roster = {margin, columnsTop, sideWidth, columnsBottom - columnsTop};
  layout.events = {width - margin - sideWidth, columnsTop, sideWidth, columnsBottom - columnsTop};
  layout.card = {margin, height - margin - cardHeight, width - 2.0f * margin, cardHeight};
  layout.battleArea = {layout.roster.Right() + margin, columnsTop,
                       layout.events.x - layout.roster.Right() - 2.0f * margin, layout.card.y - margin - columnsTop};
  // Above the card, in the corners of the battle view between the HUD columns.
  layout.infoBox = {layout.roster.Right() + margin, layout.card.y - margin - infoHeight, infoWidth, infoHeight};
  const float hintsWidth = std::max(0.0f, layout.events.x - layout.infoBox.Right() - 2.0f * margin);
  layout.hints = {layout.events.x - margin - hintsWidth, layout.card.y - margin - 18.0f * s, hintsWidth, 18.0f * s};
  return layout;
}

} // namespace reb::battle_ui
