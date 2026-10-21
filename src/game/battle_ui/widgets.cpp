#include "battle_ui/widgets.h"

#include <ranges>

namespace reb::battle_ui
{

const Theme& DefaultTheme()
{
  static const Theme theme;
  return theme;
}

Color HullColor(const Theme& theme, float fraction)
{
  if (fraction >= 0.5f) {
    return Mix(theme.hullFair, theme.hullGood, (fraction - 0.5f) * 2.0f);
  }
  return Mix(theme.hullPoor, theme.hullFair, std::max(0.0f, fraction - 0.15f) / 0.35f);
}

std::vector<std::string> WrapText(const TextMeasurer& measurer, std::string_view text, float size, float maxWidth,
                                  int maxLines, FontWeight weight)
{
  std::vector<std::string> lines;
  std::string line;
  size_t position = 0;
  while (position < text.size()) {
    const size_t space = text.find(' ', position);
    const std::string_view word
        = text.substr(position, space == std::string_view::npos ? std::string_view::npos : space - position);
    position = space == std::string_view::npos ? text.size() : space + 1;
    const std::string candidate = line.empty() ? std::string(word) : line + " " + std::string(word);
    if (line.empty() || measurer.Width(candidate, size, weight) <= maxWidth) {
      line = candidate;
      continue;
    }
    lines.push_back(std::move(line));
    line = std::string(word);
    if (maxLines > 0 && static_cast<int>(lines.size()) == maxLines) {
      std::string& last = lines.back();
      while (!last.empty() && measurer.Width(last + "...", size, weight) > maxWidth) {
        last.pop_back();
      }
      last += "...";
      return lines;
    }
  }
  if (!line.empty()) {
    lines.push_back(std::move(line));
  }
  return lines;
}

Ui::Ui(const Theme& theme)
    : m_theme(theme)
{
}

const Ui::Hit* Ui::HitAt(Point p) const
{
  for (const Hit& hit : m_lastHits | std::views::reverse) {
    if (hit.rect.Contains(p)) {
      return &hit;
    }
  }
  return nullptr;
}

bool Ui::MouseOverUi() const
{
  if (m_active != 0) {
    return true;
  }
  return std::ranges::any_of(m_lastBlockers, [this](const Rect& r) {
    return r.Contains(m_mouse);
  });
}

bool Ui::ProcessEvent(const InputEvent& event)
{
  if (const auto* move = std::get_if<MouseMove>(&event)) {
    m_mouse = move->position;
    return MouseOverUi();
  }
  if (const auto* button = std::get_if<MouseButtonEvent>(&event)) {
    m_mouse = button->position;
    if (button->button == MouseButton::Left && !button->down && m_active != 0) {
      m_released = m_active;
      const Hit* hit = HitAt(button->position);
      if (hit && hit->id == m_active) {
        m_clicked = m_active;
        m_clickPoint = button->position;
        m_clickCount = button->clicks;
      }
      m_active = 0;
      return true;
    }
    if (button->button == MouseButton::Left && button->down) {
      if (const Hit* hit = HitAt(button->position)) {
        m_active = hit->id;
        m_pressPoint = button->position;
        return true;
      }
    }
    return MouseOverUi();
  }
  if (const auto* wheel = std::get_if<MouseWheel>(&event)) {
    m_mouse = wheel->position;
    if (const Hit* hit = HitAt(wheel->position); hit && m_scroll.contains(hit->id)) {
      m_pendingScroll[hit->id] -= wheel->dy;
      return true;
    }
    return MouseOverUi();
  }
  if (std::holds_alternative<FocusLost>(event)) {
    m_active = 0;
  }
  return false;
}

void Ui::BeginFrame(DrawList& draw, const TextMeasurer& text, const ScreenInfo& screen, double seconds)
{
  m_draw = &draw;
  m_text = &text;
  m_scale = screen.uiScale;
  m_screen = {0.0f, 0.0f, static_cast<float>(screen.width), static_cast<float>(screen.height)};
  m_seconds = seconds;
  m_hits.clear();
  m_blockers.clear();
  m_tooltip.clear();
  m_hoveredId = 0;
}

void Ui::EndFrame()
{
  if (m_hoveredId != m_tooltipId) {
    m_tooltipId = m_hoveredId;
    m_hoverStart = m_seconds;
  }
  if (!m_tooltip.empty()) {
    const float size = S(m_theme.textSize);
    const float pad = S(m_theme.padding);
    const std::vector<std::string> lines = WrapText(*m_text, m_tooltip, size, S(380.0f));
    float width = 0.0f;
    for (const std::string& line : lines) {
      width = std::max(width, m_text->Width(line, size));
    }
    width += 2.0f * pad;
    const float lineHeight = m_text->LineHeight(size) + S(2.0f);
    const float height = lineHeight * static_cast<float>(lines.size()) + 2.0f * pad - S(2.0f);
    Rect box{m_mouse.x + S(14.0f), m_mouse.y + S(18.0f), width, height};
    box.x = std::max(0.0f, std::min(box.x, m_screen.Right() - box.w));
    if (box.Bottom() > m_screen.Bottom()) {
      box.y = m_mouse.y - S(8.0f) - box.h;
    }
    m_draw->FillRect(box, m_theme.tooltip);
    m_draw->StrokeRect(box, m_theme.panelBorder, S(m_theme.border));
    float y = box.y + pad;
    for (const std::string& line : lines) {
      m_draw->Text(line, {box.x + pad, y}, size, m_theme.text, Align::Start, VAlign::Top);
      y += lineHeight;
    }
  }
  m_lastHits.swap(m_hits);
  m_lastBlockers.swap(m_blockers);
  m_clicked = 0;
  m_released = 0;
  m_pendingScroll.clear();
}

bool Ui::MouseIn(const Rect& rect) const
{
  return rect.Contains(m_mouse);
}

float Ui::TextWidth(std::string_view text, float uiSize, FontWeight weight) const
{
  return m_text->Width(text, S(uiSize), weight);
}

void Ui::Block(const Rect& rect)
{
  m_blockers.push_back(rect);
}

Rect Ui::Panel(const Rect& rect, std::string_view title)
{
  Block(rect);
  m_draw->FillRect(rect, m_theme.panel);
  m_draw->StrokeRect(rect, m_theme.panelBorder, S(m_theme.border));
  if (title.empty()) {
    return rect.Inset(S(m_theme.padding));
  }
  const float titleHeight = S(m_theme.rowHeight);
  const Rect titleRect{rect.x, rect.y, rect.w, titleHeight};
  m_draw->FillRect(titleRect.Inset(S(m_theme.border)), m_theme.buttonFace.WithAlpha(150));
  Label(titleRect.Inset(S(m_theme.padding), 0.0f), title,
        {m_theme.titleSize, m_theme.title, Align::Start, VAlign::Middle, FontWeight::Bold});
  return Rect{rect.x, rect.y + titleHeight, rect.w, rect.h - titleHeight}.Inset(S(m_theme.padding));
}

void Ui::Label(const Rect& rect, std::string_view text, const LabelStyle& style)
{
  const float size = S(style.size > 0.0f ? style.size : m_theme.textSize);
  const Color color = style.color.a == 0 ? m_theme.text : style.color;
  Point anchor{rect.x, rect.y};
  if (style.align == Align::Center) {
    anchor.x = rect.Center().x;
  } else if (style.align == Align::End) {
    anchor.x = rect.Right();
  }
  if (style.valign == VAlign::Middle) {
    anchor.y = rect.Center().y;
  } else if (style.valign == VAlign::Bottom || style.valign == VAlign::Baseline) {
    anchor.y = rect.Bottom();
  }
  m_draw->Text(DrawText{std::string(text), anchor, size, color, style.align, style.valign, style.weight, rect.w});
}

bool Ui::Interact(WidgetId id, const Rect& rect, bool enabled)
{
  m_hits.push_back({id, rect});
  m_blockers.push_back(rect);
  if (MouseIn(rect) && (m_active == 0 || m_active == id)) {
    m_hoveredId = id;
  }
  const bool clicked = enabled && m_clicked == id;
  if (clicked) {
    m_clicked = 0;
  }
  return clicked;
}

void Ui::DrawButtonFace(const Rect& rect, bool hovered, bool pressed, bool on, bool enabled)
{
  Color face = on ? m_theme.buttonOn : m_theme.buttonFace;
  if (enabled && pressed) {
    face = m_theme.buttonPressed;
  } else if (enabled && hovered) {
    face = on ? Mix(m_theme.buttonOn, m_theme.text, 0.15f) : m_theme.buttonHover;
  }
  m_draw->FillRect(rect, face);
  m_draw->StrokeRect(rect, on ? m_theme.accent : m_theme.buttonBorder, S(m_theme.border));
}

bool Ui::Button(WidgetId id, const Rect& rect, std::string_view label, bool enabled)
{
  return Selectable(id, rect, label, false, enabled);
}

bool Ui::Selectable(WidgetId id, const Rect& rect, std::string_view label, bool on, bool enabled)
{
  const bool clicked = Interact(id, rect, enabled);
  DrawButtonFace(rect, Hovered(id), m_active == id && MouseIn(rect), on, enabled);
  Label(rect.Inset(S(4.0f), 0.0f), label,
        {0.0f, enabled ? m_theme.text : m_theme.disabledText, Align::Center, VAlign::Middle,
         on ? FontWeight::Bold : FontWeight::Regular});
  return clicked;
}

bool Ui::Toggle(WidgetId id, const Rect& rect, std::string_view label, bool& value)
{
  const bool clicked = Interact(id, rect, true);
  if (clicked) {
    value = !value;
  }
  const bool hovered = Hovered(id);
  const float box = std::min(rect.h, S(14.0f));
  const Rect check{rect.x, rect.Center().y - box * 0.5f, box, box};
  m_draw->FillRect(check, hovered ? m_theme.buttonHover : m_theme.buttonFace);
  m_draw->StrokeRect(check, value ? m_theme.accent : m_theme.buttonBorder, S(m_theme.border));
  if (value) {
    m_draw->FillRect(check.Inset(box * 0.25f), m_theme.accent);
  }
  Label({check.Right() + S(m_theme.padding), rect.y, rect.Right() - check.Right() - S(m_theme.padding), rect.h}, label);
  return clicked;
}

void Ui::Bar(const Rect& rect, float fraction, Color fill, Color back)
{
  m_draw->FillRect(rect, back.a == 0 ? m_theme.barBack : back);
  const float clamped = std::clamp(fraction, 0.0f, 1.0f);
  if (clamped > 0.0f) {
    m_draw->FillRect({rect.x, rect.y, rect.w * clamped, rect.h}, fill);
  }
}

int Ui::List(WidgetId id, const Rect& rect, int rows, float rowHeight, int selectedRow, const RowPainter& drawRow)
{
  const Point clickPoint = m_clickPoint;
  const bool clicked = Interact(id, rect, true);
  const float contentHeight = rowHeight * static_cast<float>(rows);
  const float maxScroll = std::max(0.0f, contentHeight - rect.h);
  float& scroll = m_scroll[id];
  if (auto pending = m_pendingScroll.find(id); pending != m_pendingScroll.end()) {
    scroll += pending->second * rowHeight;
  }
  scroll = std::clamp(scroll, 0.0f, maxScroll);

  int clickedRow = -1;
  m_draw->PushClip(rect);
  const int first = static_cast<int>(scroll / rowHeight);
  for (int row = first; row < rows; ++row) {
    const Rect rowRect{rect.x, rect.y + row * rowHeight - scroll, rect.w, rowHeight};
    if (rowRect.y >= rect.Bottom()) {
      break;
    }
    const bool hovered = Hovered(id) && rowRect.Contains(m_mouse);
    const bool selected = row == selectedRow;
    if (selected) {
      m_draw->FillRect(rowRect, m_theme.rowSelected);
    } else if (hovered) {
      m_draw->FillRect(rowRect, m_theme.rowHover);
    }
    drawRow(row, rowRect, hovered, selected);
    if (clicked && rowRect.Contains(clickPoint)) {
      clickedRow = row;
    }
  }
  m_draw->PopClip();
  if (maxScroll > 0.0f) {
    const float trackHeight = rect.h * rect.h / contentHeight;
    const float trackY = rect.y + (rect.h - trackHeight) * (scroll / maxScroll);
    m_draw->FillRect({rect.Right() - S(3.0f), trackY, S(3.0f), trackHeight}, m_theme.panelBorder);
  }
  return clickedRow;
}

float Ui::ScrollOffset(WidgetId id, float contentHeight, float viewHeight, float wheelStep)
{
  float& scroll = m_scroll[id];
  if (auto pending = m_pendingScroll.find(id); pending != m_pendingScroll.end()) {
    scroll += pending->second * wheelStep;
  }
  scroll = std::clamp(scroll, 0.0f, std::max(0.0f, contentHeight - viewHeight));
  return scroll;
}

void Ui::Tooltip(WidgetId id, std::string_view text)
{
  if (Hovered(id) && id == m_tooltipId && m_seconds - m_hoverStart >= m_theme.tooltipDelay) {
    m_tooltip = text;
  }
}

} // namespace reb::battle_ui
