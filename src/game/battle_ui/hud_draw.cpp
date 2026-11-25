#include "battle_ui/hud_draw.h"

#include "battle_ui/chrome.h"

namespace reb::battle_ui::hud
{

namespace
{

constexpr float kIconSize = 14.0f;
constexpr float kTextSize = 12.0f;

void Cap(Ui& ui, WidgetId id, const Rect& rect, bool on, bool enabled)
{
  const bool pressed = ui.ActiveId() == id && ui.MouseIn(rect);
  chrome::ButtonCap(ui.Draw(), rect, chrome::FaceFor(ui.Hovered(id), pressed, on, enabled), ui.GetTheme(), ui.Scale());
}

// Button labels sit a little above centre, clear of the lamp slit.
float LabelLift(Ui& ui, const Rect& rect)
{
  return rect.h >= ui.S(17.0f) ? ui.S(1.0f) : 0.0f;
}

} // namespace

bool Button(Ui& ui, WidgetId id, const Rect& rect, Icon icon, std::string_view label, bool enabled, bool on)
{
  const Theme& theme = ui.GetTheme();
  const bool clicked = ui.Region(id, rect, enabled);
  Cap(ui, id, rect, on, enabled);
  const Color color = enabled ? (on ? theme.title : theme.text) : theme.disabledText;
  const float iconSize = std::min(ui.S(kIconSize), rect.h - ui.S(8.0f));
  const Rect face{rect.x, rect.y - LabelLift(ui, rect), rect.w, rect.h};
  // A label that would not fit in full drops its icon first, then ellipsizes.
  if (!label.empty() && iconSize + ui.S(13.0f) + ui.TextWidth(label, kTextSize) > rect.w) {
    ui.Label(face.Inset(ui.S(4.0f), 0.0f), label,
             {kTextSize, color, Align::Center, VAlign::Middle, on ? FontWeight::Bold : FontWeight::Regular});
    return clicked;
  }
  if (label.empty()) {
    DrawIcon(ui.Draw(), icon,
             {rect.Center().x - iconSize * 0.5f, face.Center().y - iconSize * 0.5f, iconSize, iconSize}, color,
             ui.Scale());
    return clicked;
  }
  const float textWidth = ui.TextWidth(label, kTextSize, on ? FontWeight::Bold : FontWeight::Regular);
  const float contentWidth = std::min(rect.w - ui.S(8.0f), iconSize + ui.S(5.0f) + textWidth);
  const float x = rect.Center().x - contentWidth * 0.5f;
  DrawIcon(ui.Draw(), icon, {x, face.Center().y - iconSize * 0.5f, iconSize, iconSize}, color, ui.Scale());
  ui.Label({x + iconSize + ui.S(5.0f), face.y, contentWidth - iconSize - ui.S(5.0f), face.h}, label,
           {kTextSize, color, Align::Start, VAlign::Middle, on ? FontWeight::Bold : FontWeight::Regular});
  return clicked;
}

float ButtonWidth(Ui& ui, std::string_view label, bool icon)
{
  const float text = label.empty() ? 0.0f : ui.TextWidth(label, kTextSize, FontWeight::Bold);
  const float iconWidth = icon ? ui.S(kIconSize) + (label.empty() ? 0.0f : ui.S(5.0f)) : 0.0f;
  return text + iconWidth + ui.S(16.0f);
}

bool Segment(Ui& ui, WidgetId id, const Rect& rect, std::string_view label, bool on, bool enabled)
{
  const Theme& theme = ui.GetTheme();
  const bool clicked = ui.Region(id, rect, enabled);
  Cap(ui, id, rect, on, enabled);
  const Rect face{rect.x, rect.y - LabelLift(ui, rect), rect.w, rect.h};
  ui.Label(face.Inset(ui.S(3.0f), 0.0f), label,
           {11.0f, enabled ? (on ? theme.title : theme.text) : theme.disabledText, Align::Center, VAlign::Middle,
            on ? FontWeight::Bold : FontWeight::Regular});
  return clicked;
}

Rect FoldRect(const Rect& titleBar)
{
  return {titleBar.Right() - titleBar.h, titleBar.y, titleBar.h, titleBar.h};
}

bool FoldButton(Ui& ui, WidgetId id, const Rect& titleBar, bool folded, std::string_view what)
{
  const Theme& theme = ui.GetTheme();
  const Rect button = FoldRect(titleBar);
  const bool clicked = ui.Region(id, button);
  DrawIcon(ui.Draw(), folded ? Icon::ChevronDown : Icon::ChevronUp, button.Inset(button.h * 0.3f),
           theme.captionText.WithAlpha(ui.Hovered(id) ? 255 : 170), ui.Scale());
  ui.Tooltip(id, std::string(folded ? "Show " : "Hide ") + std::string(what));
  return clicked;
}

void Caption(Ui& ui, const Rect& rect, std::string_view text, Align align)
{
  ui.Label(rect, text, {10.0f, ui.GetTheme().textDim, align, VAlign::Middle, FontWeight::Bold});
  if (align != Align::Start || rect.w < ui.S(80.0f)) {
    return;
  }
  // An engraved rule runs on from the caption to the end of its line, as on the original's panels.
  const float after = rect.x + ui.TextWidth(text, 10.0f, FontWeight::Bold) + ui.S(8.0f);
  const float right = rect.Right() - ui.S(2.0f);
  if (right - after > ui.S(16.0f)) {
    chrome::Groove(ui.Draw(), {after, std::round(rect.Center().y)}, {right, std::round(rect.Center().y)},
                   ui.GetTheme(), ui.Scale());
  }
}

Rect PopupFrame(Ui& ui, WidgetId id, const Rect& rect, std::string_view title)
{
  const Theme& theme = ui.GetTheme();
  ui.Region(id, rect);
  ui.Draw().FillRect({rect.x + ui.S(4.0f), rect.y + ui.S(6.0f), rect.w, rect.h}, Rgb(0x000000, 70));
  chrome::Plate(ui.Draw(), rect, theme, ui.Scale());
  const float titleHeight = ui.S(22.0f);
  chrome::Caption(ui.Draw(), rect, titleHeight, title, ui.S(11.0f), theme, ui.Scale());
  return {rect.x + ui.S(8.0f), rect.y + titleHeight + ui.S(5.0f), rect.w - ui.S(16.0f),
          rect.h - titleHeight - ui.S(11.0f)};
}

} // namespace reb::battle_ui::hud
