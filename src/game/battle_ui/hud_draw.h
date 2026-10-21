#pragma once
#include <string_view>

#include "battle_ui/hud_icons.h"
#include "battle_ui/widgets.h"

// Drawing helpers shared by the command HUD's panels.
namespace reb::battle_ui::hud
{

// A button with an icon and a label (icon only when the label is empty). `on` shows it as the current choice.
bool Button(Ui& ui, WidgetId id, const Rect& rect, Icon icon, std::string_view label, bool enabled = true,
            bool on = false);
// The width a Button needs, in device pixels.
float ButtonWidth(Ui& ui, std::string_view label, bool icon = true);
// A text button for segmented choices (no icon).
bool Segment(Ui& ui, WidgetId id, const Rect& rect, std::string_view label, bool on, bool enabled = true);
// The fold chevron at the right end of a panel's title bar (FoldRect); true when clicked.
bool FoldButton(Ui& ui, WidgetId id, const Rect& titleBar, bool folded, std::string_view what);
Rect FoldRect(const Rect& titleBar);
// A small caption, upper case and dim.
void Caption(Ui& ui, const Rect& rect, std::string_view text, Align align = Align::Start);
// A box that blocks clicks to whatever is under it, for popups.
Rect PopupFrame(Ui& ui, WidgetId id, const Rect& rect, std::string_view title);

} // namespace reb::battle_ui::hud
