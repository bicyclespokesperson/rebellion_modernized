#include "battle_ui/chrome.h"

#include <cmath>
#include <ranges>
#include <vector>

namespace reb::battle_ui::chrome
{
namespace
{

const Color kBlack = Rgb(0x000000);
const Color kWhite = Rgb(0xffffff);

Rect Snapped(const Rect& rect)
{
  const float x = std::round(rect.x);
  const float y = std::round(rect.y);
  return {x, y, std::round(rect.Right()) - x, std::round(rect.Bottom()) - y};
}

// A rectangle with its corners clipped at 45 degrees, clockwise from the top left.
std::vector<Point> Clipped(const Rect& r, float corner)
{
  const float c = std::clamp(corner, 0.0f, std::min(r.w, r.h) * 0.45f);
  return {{r.x + c, r.y},        {r.Right() - c, r.y}, {r.Right(), r.y + c}, {r.Right(), r.Bottom() - c},
          {r.Right() - c, r.Bottom()}, {r.x + c, r.Bottom()}, {r.x, r.Bottom() - c}, {r.x, r.y + c}};
}

// One colour per point, from `top` at the rect's top edge to `bottom` at its bottom edge.
std::vector<Color> VerticalShade(const std::vector<Point>& points, const Rect& r, Color top, Color bottom)
{
  std::vector<Color> colors(points.size());
  std::ranges::transform(points, colors.begin(), [&](Point p) {
    return Mix(top, bottom, r.h > 0.0f ? (p.y - r.y) / r.h : 0.0f);
  });
  return colors;
}

void ShadedClipped(DrawList& draw, const Rect& r, float corner, Color top, Color bottom)
{
  std::vector<Point> points = Clipped(r, corner);
  std::vector<Color> colors = VerticalShade(points, r, top, bottom);
  draw.FillConvexShaded(std::move(points), std::move(colors));
}

Theme EmpireSkin(Theme theme)
{
  theme.panel = Rgb(0x121a2e, 236);
  theme.panelBottom = Rgb(0x080c18, 242);
  theme.panelBorder = Rgb(0x8c98c4, 200);
  theme.textDim = Rgb(0x9ca6c4);
  theme.buttonFace = Rgb(0x525d85);
  theme.buttonFaceBottom = Rgb(0x272e4e);
  theme.buttonHover = Rgb(0x67739f);
  theme.buttonPressed = Rgb(0x1a1f36);
  theme.buttonOn = Rgb(0x5d5a66);
  theme.tooltip = Rgb(0x0e1324, 246);
  theme.metalHigh = Rgb(0xdfe6fa);
  theme.metal = Rgb(0x8c96ba);
  theme.metalShade = Rgb(0x2e3558);
  theme.captionTop = Rgb(0x3652cc);
  theme.captionBottom = Rgb(0x141e5c);
  theme.readout = Rgb(0x46ff5e);
  theme.skin = battle::Side::Empire;
  return theme;
}

// The starbird: a crescent (between the unit circle and a smaller circle raised inside it) and a central blade.
void Starbird(DrawList& draw, Point c, float r, Color color)
{
  constexpr int kSteps = 24;
  const float degrees = kPi / 180.0f;
  const float outerFrom = -48.6f * degrees;
  const float innerFrom = -34.2f * degrees;
  std::vector<Point> outline;
  std::vector<Vertex> fill;
  const auto outer = [&](int k) {
    const float a = outerFrom + (kPi - 2.0f * outerFrom) * static_cast<float>(k) / kSteps;
    return Point{c.x + std::cos(a) * r, c.y + std::sin(a) * r};
  };
  const auto inner = [&](int k) {
    const float a = innerFrom + (kPi - 2.0f * innerFrom) * static_cast<float>(k) / kSteps;
    return Point{c.x + std::cos(a) * r * 0.8f, c.y - r * 0.3f + std::sin(a) * r * 0.8f};
  };
  for (int k = 0; k < kSteps; ++k) {
    fill.insert(fill.end(), {{outer(k), color}, {outer(k + 1), color}, {inner(k + 1), color},
                             {outer(k), color}, {inner(k + 1), color}, {inner(k), color}});
  }
  for (int k = 0; k <= kSteps; ++k) {
    outline.push_back(outer(k));
  }
  for (int k = kSteps; k >= 0; --k) {
    outline.push_back(inner(k));
  }
  draw.Triangles(std::move(fill));
  draw.Polyline(std::move(outline), color, Stroke{1.0f}, true);
  draw.FillConvex({{c.x, c.y - r * 1.08f}, {c.x + r * 0.15f, c.y - r * 0.05f}, {c.x + r * 0.07f, c.y + r * 0.6f},
                   {c.x - r * 0.07f, c.y + r * 0.6f}, {c.x - r * 0.15f, c.y - r * 0.05f}},
                  color);
}

void ImperialCrest(DrawList& draw, Point c, float r, Color color)
{
  draw.Ring(c, r * 0.8f, r, color);
  draw.Ring(c, r * 0.2f, r * 0.36f, color);
  for (int i = 0; i < 6; ++i) {
    const float a = kPi * 0.5f + static_cast<float>(i) * kPi / 3.0f;
    const Point along = ScreenDirection(a);
    const Point across = ScreenDirection(a + kPi * 0.5f);
    draw.FillConvex({c + along * (r * 0.34f) + across * (r * 0.06f), c + along * (r * 0.82f) + across * (r * 0.17f),
                     c + along * (r * 0.82f) - across * (r * 0.17f), c + along * (r * 0.34f) - across * (r * 0.06f)},
                    color);
  }
}

} // namespace

Theme SkinnedTheme(battle::Side side, bool colorBlind)
{
  Theme theme = side == battle::Side::Empire ? EmpireSkin(Theme{}) : Theme{};
  if (colorBlind) {
    // Orange against sky blue stays distinct under red-green and blue-yellow colour blindness.
    theme.alliance = Rgb(0xf5a43a);
    theme.empire = Rgb(0x5cc6f2);
    // Bolts follow the sides' colours; ion fire, otherwise blue like the Empire's, is told apart by brightness.
    theme.allianceBolt = Rgb(0xff9a1e);
    theme.empireBolt = Rgb(0x3cc8ff);
    theme.ionBolt = Rgb(0xf0ecff);
  }
  return theme;
}

void Plate(DrawList& draw, const Rect& rect, const Theme& theme, float s, float rim)
{
  const Rect r = Snapped(rect);
  if (r.Empty()) {
    return;
  }
  const float corner = std::round(5.0f * s);
  const float edge = std::max(1.0f, std::round(rim * s));
  const float drop = std::max(1.0f, std::round(2.0f * s));
  draw.FillConvex(Clipped({r.x + drop, r.y + drop * 1.5f, r.w, r.h}, corner), kBlack.WithAlpha(80));
  ShadedClipped(draw, r, corner, theme.metalHigh, theme.metalShade);
  const Rect body = r.Inset(edge);
  ShadedClipped(draw, body, corner - edge * 0.4f, theme.panel, theme.panelBottom);
  draw.Polyline(Clipped(body, corner - edge * 0.4f), kBlack.WithAlpha(170), Stroke{1.0f}, true);
  draw.Polyline(Clipped(r, corner), kBlack.WithAlpha(150), Stroke{1.0f}, true);
}

Rect Caption(DrawList& draw, const Rect& plate, float height, std::string_view title, float textSize,
             const Theme& theme, float s)
{
  const Rect r = Snapped(plate);
  const float edge = std::max(1.0f, std::round(2.0f * s));
  const float corner = std::round(5.0f * s) - edge * 0.4f;
  const Rect band{r.x + edge, r.y + edge, r.w - 2.0f * edge, std::round(height) - edge};
  if (band.Empty()) {
    return band;
  }
  const float c = std::clamp(corner, 0.0f, band.h * 0.45f);
  std::vector<Point> points{{band.x + c, band.y},      {band.Right() - c, band.y}, {band.Right(), band.y + c},
                            {band.Right(), band.Bottom()}, {band.x, band.Bottom()},    {band.x, band.y + c}};
  std::vector<Color> colors = VerticalShade(points, band, theme.captionTop, theme.captionBottom);
  draw.FillConvexShaded(std::move(points), std::move(colors));
  draw.Line({band.x + c, band.y + 0.5f}, {band.Right() - c, band.y + 0.5f}, kWhite.WithAlpha(80));
  draw.Line({band.x, band.Bottom() - 0.5f}, {band.Right(), band.Bottom() - 0.5f}, kBlack.WithAlpha(190));
  const float emblem = band.h * 0.34f;
  const Point mark{band.x + band.h * 0.55f, band.Center().y};
  Emblem(draw, mark + Point{0.0f, std::max(1.0f, s)}, emblem, theme.skin, kBlack.WithAlpha(110));
  Emblem(draw, mark, emblem, theme.skin, theme.captionText.WithAlpha(215));
  const float x = mark.x + emblem + 6.0f * s;
  ShadowText(draw,
             DrawText{std::string(title), {x, band.Center().y}, textSize, theme.captionText, Align::Start,
                      VAlign::Middle, FontWeight::Bold, std::max(0.0f, band.Right() - x - 4.0f * s)},
             s);
  return band;
}

Face FaceFor(bool hovered, bool pressed, bool on, bool enabled)
{
  if (!enabled) {
    return Face::Disabled;
  }
  if (pressed) {
    return Face::Pressed;
  }
  if (on) {
    return Face::On;
  }
  return hovered ? Face::Hovered : Face::Up;
}

void ButtonCap(DrawList& draw, const Rect& rect, Face face, const Theme& theme, float s)
{
  const Rect r = Snapped(rect);
  if (r.Empty()) {
    return;
  }
  Color top = theme.buttonFace;
  Color bottom = theme.buttonFaceBottom;
  int highlight = 70;
  switch (face) {
    case Face::Up:
      break;
    case Face::Hovered:
      top = theme.buttonHover;
      bottom = Mix(theme.buttonFaceBottom, theme.buttonHover, 0.45f);
      break;
    case Face::Pressed:
      top = theme.buttonPressed;
      bottom = Mix(theme.buttonPressed, theme.buttonFace, 0.45f);
      highlight = 0;
      break;
    case Face::On:
      top = Mix(theme.buttonFaceBottom, kBlack, 0.25f);
      bottom = theme.buttonOn;
      highlight = 18;
      break;
    case Face::Disabled:
      top = Mix(theme.buttonFace, kBlack, 0.45f).WithAlpha(215);
      bottom = Mix(theme.buttonFaceBottom, kBlack, 0.45f).WithAlpha(215);
      highlight = 22;
      break;
  }
  draw.GradientRect(r, top, bottom);
  if (highlight > 0) {
    draw.FillRect({r.x + 1.0f, r.y + 1.0f, r.w - 2.0f, std::max(1.0f, std::round(s))},
                  kWhite.WithAlpha(static_cast<uint8_t>(highlight)));
  }
  draw.StrokeRect(r, theme.buttonBorder, 1.0f);
  if (r.h < 17.0f * s || r.w < 14.0f * s) {
    return;
  }
  // The lamp slit of the original's buttons, lit for the current choice.
  const float slitWidth = std::round(std::clamp(r.w * 0.42f, 6.0f * s, 34.0f * s));
  const Rect slit{std::round(r.Center().x - slitWidth * 0.5f), r.Bottom() - std::round(4.0f * s), slitWidth,
                  std::max(1.0f, std::round(2.0f * s))};
  if (face == Face::On) {
    draw.Glow(slit.Center(), slitWidth * 0.75f, theme.accent.WithAlpha(70));
    draw.FillRect(slit, Mix(theme.accent, kWhite, 0.25f));
  } else {
    draw.FillRect(slit, kBlack.WithAlpha(face == Face::Disabled ? 60 : 120));
  }
}

void Well(DrawList& draw, const Rect& rect, const Theme& theme, float s)
{
  const Rect r = Snapped(rect);
  if (r.Empty()) {
    return;
  }
  const float line = std::max(1.0f, std::round(s));
  draw.FillRect(r, theme.well);
  draw.FillRect({r.x, r.y, r.w, line}, kBlack.WithAlpha(230));
  draw.FillRect({r.x, r.y, line, r.h}, kBlack.WithAlpha(160));
  draw.FillRect({r.x, r.Bottom() - line, r.w, line}, theme.metalHigh.WithAlpha(46));
}

void Gauge(DrawList& draw, const Rect& rect, float fraction, Color fill, const Theme& theme, float s, int segments)
{
  Well(draw, rect, theme, s);
  const float inset = std::max(1.0f, std::round(s));
  const Rect inner = Snapped(rect).Inset(inset);
  if (inner.Empty()) {
    return;
  }
  const float f = std::clamp(fraction, 0.0f, 1.0f);
  const Color lit = Mix(fill, kWhite, 0.3f);
  const Color dark = Mix(fill, kBlack, 0.2f);
  if (segments <= 1) {
    draw.FillRect(inner, fill.WithAlpha(26));
    if (f > 0.0f) {
      draw.GradientRect({inner.x, inner.y, std::max(1.0f, std::round(inner.w * f)), inner.h}, lit, dark);
    }
    return;
  }
  const float gap = std::max(1.0f, std::round(s));
  const float width = (inner.w - gap * static_cast<float>(segments - 1)) / static_cast<float>(segments);
  const int litCount = f <= 0.0f ? 0 : std::max(1, static_cast<int>(std::lround(f * static_cast<float>(segments))));
  for (int i = 0; i < segments; ++i) {
    const Rect block{inner.x + static_cast<float>(i) * (width + gap), inner.y, width, inner.h};
    if (i < litCount) {
      draw.GradientRect(block, lit, dark);
    } else {
      draw.FillRect(block, fill.WithAlpha(30));
    }
  }
}

void Lamp(DrawList& draw, Point center, float radius, Color color, bool lit)
{
  if (lit) {
    draw.Glow(center, radius * 2.6f, color.WithAlpha(110));
    draw.FillCircle(center, radius, Mix(color, kWhite, 0.15f));
    draw.FillCircle(center - Point{radius * 0.3f, radius * 0.3f}, radius * 0.38f, kWhite.WithAlpha(170));
    return;
  }
  draw.FillCircle(center, radius, Mix(color, kBlack, 0.78f));
  draw.Circle(center, radius, kBlack.WithAlpha(200), Stroke{1.0f});
}

void Groove(DrawList& draw, Point from, Point to, const Theme& theme, float s)
{
  const float line = std::max(1.0f, std::round(s));
  draw.Line(from, to, kBlack.WithAlpha(170), Stroke{line});
  draw.Line(from + Point{0.0f, line}, to + Point{0.0f, line}, theme.metalHigh.WithAlpha(34), Stroke{line});
}

void Emblem(DrawList& draw, Point center, float radius, battle::Side side, Color color)
{
  if (side == battle::Side::Empire) {
    ImperialCrest(draw, center, radius, color);
  } else {
    Starbird(draw, center, radius, color);
  }
}

void ShadowText(DrawList& draw, DrawText text, float s)
{
  DrawText shadow = text;
  const float offset = std::max(1.0f, std::round(s));
  shadow.position = text.position + Point{offset, offset};
  shadow.color = kBlack.WithAlpha(static_cast<uint8_t>(text.color.a * 0.75f));
  draw.Text(std::move(shadow));
  draw.Text(std::move(text));
}

void ConsoleBar(DrawList& draw, const Rect& rect, const Theme& theme, float s)
{
  const Rect r = Snapped(rect);
  const float line = std::max(1.0f, std::round(s));
  draw.GradientRect({r.x, r.Bottom(), r.w, std::round(7.0f * s)}, kBlack.WithAlpha(130), kBlack.WithAlpha(0));
  draw.GradientRect(r, Mix(theme.metal, kBlack, 0.38f), Mix(theme.metalShade, kBlack, 0.42f));
  draw.FillRect({r.x, r.y, r.w, line}, theme.metalHigh.WithAlpha(120));
  draw.FillRect({r.x, r.Bottom() - 2.0f * line, r.w, line}, theme.metal.WithAlpha(110));
  draw.FillRect({r.x, r.Bottom() - line, r.w, line}, kBlack.WithAlpha(220));
}

void Readout(DrawList& draw, const Rect& rect, std::string_view text, std::string_view ghost, float size, Color color,
             const Theme& theme, float s, Align align)
{
  Well(draw, rect, theme, s);
  const Rect inner = rect.Inset(4.0f * s, 0.0f);
  const float x = align == Align::Center ? inner.Center().x : align == Align::End ? inner.Right() : inner.x;
  const Point at{x, rect.Center().y};
  if (!ghost.empty()) {
    draw.Text(DrawText{std::string(ghost), at, size, color.WithAlpha(26), align, VAlign::Middle, FontWeight::Bold,
                       inner.w, TextOverflow::Clip});
  }
  draw.Glow(rect.Center(), std::min(rect.w, rect.h * 2.2f) * 0.55f, color.WithAlpha(24));
  draw.Text(DrawText{std::string(text), at, size, color, align, VAlign::Middle, FontWeight::Bold, inner.w});
}

void CommandButton(DrawList& draw, const Rect& rect, Face face, const Theme& theme, float s)
{
  const Rect r = Snapped(rect);
  const float corner = std::round(4.0f * s);
  const float edge = std::max(1.0f, std::round(1.5f * s));
  ShadedClipped(draw, r, corner, theme.metalHigh, theme.metalShade);
  Color top = Mix(theme.captionTop, kWhite, face == Face::Hovered ? 0.18f : 0.0f);
  Color bottom = theme.captionBottom;
  if (face == Face::Pressed) {
    std::swap(top, bottom);
  }
  ShadedClipped(draw, r.Inset(edge), corner - edge * 0.4f, top, bottom);
  draw.Line({r.x + corner, r.y + edge + 0.5f}, {r.Right() - corner, r.y + edge + 0.5f},
            kWhite.WithAlpha(face == Face::Pressed ? 20 : 90));
  draw.Polyline(Clipped(r, corner), kBlack.WithAlpha(170), Stroke{1.0f}, true);
}

void Keycap(DrawList& draw, const Rect& rect, std::string_view label, float size, const Theme& theme, float s)
{
  const Rect r = Snapped(rect);
  draw.GradientRect(r, Mix(theme.buttonFace, kWhite, 0.08f), theme.buttonFaceBottom);
  draw.FillRect({r.x, r.Bottom() - std::max(1.0f, std::round(s)), r.w, std::max(1.0f, std::round(s))},
                kBlack.WithAlpha(150));
  draw.StrokeRect(r, theme.buttonBorder, 1.0f);
  draw.FillRect({r.x + 1.0f, r.y + 1.0f, r.w - 2.0f, 1.0f}, kWhite.WithAlpha(60));
  draw.Text(DrawText{std::string(label), {r.Center().x, r.Center().y - 0.5f * s}, size, theme.title, Align::Center,
                     VAlign::Middle, FontWeight::Bold, r.w});
}

void StatusBars(DrawList& draw, const Rect& rect, float shield, float hull, Color hullColor, const Theme& theme,
                float s)
{
  Well(draw, rect, theme, s);
  const float inset = std::max(1.0f, std::round(s));
  const Rect inner = Snapped(rect).Inset(inset);
  if (inner.Empty()) {
    return;
  }
  const float split = std::round(inner.h * 0.42f);
  const Rect top{inner.x, inner.y, inner.w, split};
  const Rect bottom{inner.x, inner.y + split + inset, inner.w, inner.h - split - inset};
  const auto fill = [&](const Rect& lane, float fraction, Color color) {
    draw.FillRect(lane, color.WithAlpha(24));
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    if (f > 0.0f) {
      draw.GradientRect({lane.x, lane.y, std::max(1.0f, std::round(lane.w * f)), lane.h}, Mix(color, kWhite, 0.25f),
                        Mix(color, kBlack, 0.15f));
    }
  };
  fill(top, shield, theme.shield);
  fill(bottom, hull, hullColor);
}

void Notice(DrawList& draw, const Rect& rect, Color signal, const Theme& theme, float s, float fade)
{
  Theme faded = theme;
  for (Color* color : {&faded.panel, &faded.panelBottom, &faded.metalHigh, &faded.metalShade}) {
    *color = color->Faded(fade);
  }
  faded.panel = faded.panel.WithAlpha(static_cast<uint8_t>(std::min(255.0f, theme.tooltip.a * fade)));
  faded.panelBottom = faded.panel;
  Plate(draw, rect, faded, s, 1.0f);
  const Rect r = Snapped(rect);
  const float inset = std::max(1.0f, std::round(2.0f * s));
  const Rect strip{r.x + inset, r.y + inset + std::round(2.0f * s), std::max(2.0f, std::round(3.0f * s)),
                   r.h - 2.0f * inset - std::round(4.0f * s)};
  draw.Glow(strip.Center(), strip.h * 0.7f, signal.WithAlpha(static_cast<uint8_t>(60 * fade)));
  draw.FillRect(strip, signal.Faded(fade));
}

void Medallion(DrawList& draw, Point center, float radius, const Theme& theme)
{
  draw.FillCircle(center + Point{radius * 0.08f, radius * 0.12f}, radius, kBlack.WithAlpha(90));
  draw.GradientSector(center, 0.0f, radius, 0.0f, kTwoPi, theme.metal, theme.metalShade);
  draw.Circle(center, radius, kBlack.WithAlpha(170), Stroke{1.0f});
  draw.FillCircle(center, radius * 0.84f, Mix(theme.metalShade, kBlack, 0.35f));
  const float r = radius * 0.62f;
  const float offset = std::max(1.0f, radius * 0.06f);
  Emblem(draw, center + Point{offset, offset}, r, theme.skin, kBlack.WithAlpha(150));
  Emblem(draw, center - Point{offset * 0.5f, offset * 0.5f}, r, theme.skin, theme.metalHigh.WithAlpha(120));
  Emblem(draw, center, r, theme.skin, theme.metal);
}

} // namespace reb::battle_ui::chrome
