#include "battle_ui/hud_draw.h"

namespace reb::battle_ui::hud
{

namespace
{

constexpr float kIconSize = 14.0f;
constexpr float kTextSize = 12.0f;

Color Face(const Theme& theme, bool hovered, bool pressed, bool on, bool enabled)
{
  if (!enabled) {
    return theme.buttonFace.WithAlpha(150);
  }
  if (pressed) {
    return theme.buttonPressed;
  }
  if (on) {
    return hovered ? Mix(theme.buttonOn, theme.text, 0.15f) : theme.buttonOn;
  }
  return hovered ? theme.buttonHover : theme.buttonFace;
}

} // namespace

bool Button(Ui& ui, WidgetId id, const Rect& rect, Icon icon, std::string_view label, bool enabled, bool on)
{
  const Theme& theme = ui.GetTheme();
  const bool clicked = ui.Region(id, rect, enabled);
  const bool hovered = ui.Hovered(id);
  const bool pressed = ui.ActiveId() == id && ui.MouseIn(rect);
  ui.Draw().FillRect(rect, Face(theme, hovered, pressed, on, enabled));
  ui.Draw().StrokeRect(rect, on ? theme.accent : theme.buttonBorder, ui.S(theme.border));
  const Color color = enabled ? theme.text : theme.disabledText;
  const float iconSize = std::min(ui.S(kIconSize), rect.h - ui.S(6.0f));
  // A label that would not fit in full drops its icon first, then ellipsizes.
  if (!label.empty() && iconSize + ui.S(13.0f) + ui.TextWidth(label, kTextSize) > rect.w) {
    ui.Label(rect.Inset(ui.S(4.0f), 0.0f), label,
             {kTextSize, color, Align::Center, VAlign::Middle, on ? FontWeight::Bold : FontWeight::Regular});
    return clicked;
  }
  if (label.empty()) {
    DrawIcon(ui.Draw(), icon,
             {rect.Center().x - iconSize * 0.5f, rect.Center().y - iconSize * 0.5f, iconSize, iconSize}, color,
             ui.Scale());
    return clicked;
  }
  const float textWidth = ui.TextWidth(label, kTextSize, on ? FontWeight::Bold : FontWeight::Regular);
  const float contentWidth = std::min(rect.w - ui.S(8.0f), iconSize + ui.S(5.0f) + textWidth);
  const float x = rect.Center().x - contentWidth * 0.5f;
  DrawIcon(ui.Draw(), icon, {x, rect.Center().y - iconSize * 0.5f, iconSize, iconSize}, color, ui.Scale());
  ui.Label({x + iconSize + ui.S(5.0f), rect.y, contentWidth - iconSize - ui.S(5.0f), rect.h}, label,
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
  const bool hovered = ui.Hovered(id);
  const bool pressed = ui.ActiveId() == id && ui.MouseIn(rect);
  ui.Draw().FillRect(rect, Face(theme, hovered, pressed, on, enabled));
  ui.Draw().StrokeRect(rect, on ? theme.accent : theme.buttonBorder, ui.S(theme.border));
  ui.Label(rect.Inset(ui.S(3.0f), 0.0f), label,
           {11.0f, enabled ? theme.text : theme.disabledText, Align::Center, VAlign::Middle,
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
           ui.Hovered(id) ? theme.text : theme.textDim, ui.Scale());
  ui.Tooltip(id, std::string(folded ? "Show " : "Hide ") + std::string(what));
  return clicked;
}

void Caption(Ui& ui, const Rect& rect, std::string_view text, Align align)
{
  ui.Label(rect, text, {10.0f, ui.GetTheme().textDim, align, VAlign::Middle, FontWeight::Bold});
}

Rect PopupFrame(Ui& ui, WidgetId id, const Rect& rect, std::string_view title)
{
  const Theme& theme = ui.GetTheme();
  ui.Region(id, rect);
  ui.Draw().FillRect({rect.x + ui.S(3.0f), rect.y + ui.S(4.0f), rect.w, rect.h}, Rgb(0x000000, 110));
  ui.Draw().FillRect(rect, theme.tooltip);
  ui.Draw().StrokeRect(rect, theme.accent.WithAlpha(190), ui.S(1.0f));
  const float titleHeight = ui.S(22.0f);
  ui.Label({rect.x + ui.S(8.0f), rect.y, rect.w - ui.S(16.0f), titleHeight}, title,
           {11.0f, theme.title, Align::Start, VAlign::Middle, FontWeight::Bold});
  ui.Draw().FillRect({rect.x + ui.S(1.0f), rect.y + titleHeight, rect.w - ui.S(2.0f), ui.S(1.0f)},
                     theme.panelBorder.WithAlpha(90));
  return {rect.x + ui.S(8.0f), rect.y + titleHeight + ui.S(5.0f), rect.w - ui.S(16.0f),
          rect.h - titleHeight - ui.S(11.0f)};
}

} // namespace reb::battle_ui::hud
