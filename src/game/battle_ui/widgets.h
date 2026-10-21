#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "battle_ui/draw_list.h"
#include "battle_ui/input.h"
#include "battle_ui/text_measure.h"

// A small immediate-mode widget kit for the battle HUD: panels, labels, buttons, toggles, lists, bars and tooltips.
//
// Each frame the screen calls BeginFrame, then the widget functions, which draw into the draw list and report clicks,
// then EndFrame. Input arrives earlier, as events: ProcessEvent hit-tests them against the widgets and panels drawn in
// the previous frame and returns true when the UI took the event, so the battle view underneath does not also act on
// it. A click is reported by the widget function in the frame after the button was released.
//
// Positions are device pixels. Sizes in the theme are UI pixels; multiply by Scale() (or call S()) to place things.
namespace reb::battle_ui
{

using WidgetId = uint64_t;

constexpr WidgetId Id(std::string_view name, WidgetId seed = 14695981039346656037ULL)
{
  WidgetId hash = seed;
  for (char c : name) {
    hash = (hash ^ static_cast<uint8_t>(c)) * 1099511628211ULL;
  }
  return hash;
}

constexpr WidgetId Id(WidgetId parent, int64_t index)
{
  WidgetId hash = parent;
  for (int byte = 0; byte < 8; ++byte) {
    hash = (hash ^ static_cast<uint8_t>(static_cast<uint64_t>(index) >> (byte * 8))) * 1099511628211ULL;
  }
  return hash;
}

struct Theme {
  Color panel = Rgb(0x08101c, 224);
  Color panelBorder = Rgb(0x5a8cbe, 150);
  Color title = Rgb(0x96c8f0);
  Color text = Rgb(0xd2dceb);
  Color textDim = Rgb(0x8496aa);
  Color accent = Rgb(0x5ab4ff);
  Color buttonFace = Rgb(0x1c2c40, 235);
  Color buttonHover = Rgb(0x2c4664, 240);
  Color buttonPressed = Rgb(0x162232, 240);
  Color buttonOn = Rgb(0x3c6ea0, 245);
  Color buttonBorder = Rgb(0x5a8cbe, 170);
  Color disabledText = Rgb(0x56606c);
  Color rowHover = Rgb(0x2c4664, 160);
  Color rowSelected = Rgb(0x3c6ea0, 200);
  Color tooltip = Rgb(0x0c1420, 245);
  Color barBack = Rgb(0x202a36, 220);
  Color alliance = Rgb(0xeb5a46);
  Color empire = Rgb(0x6ed26e);
  Color ion = Rgb(0x5aa0ff);
  // The bolt colour code (§10.3): red Alliance, green Empire, blue ion fire.
  Color allianceBolt = Rgb(0xff3a2e);
  Color empireBolt = Rgb(0x4cff5a);
  Color ionBolt = Rgb(0x5a9cff);
  Color shield = Rgb(0x50aaff);
  Color hullGood = Rgb(0x64d264);
  Color hullFair = Rgb(0xe6c846);
  Color hullPoor = Rgb(0xe65a46);
  float textSize = 13.0f;
  float smallTextSize = 11.0f;
  float titleSize = 13.0f;
  float padding = 6.0f;
  float rowHeight = 22.0f;
  float buttonHeight = 24.0f;
  float border = 1.0f;
  float tooltipDelay = 0.35f; // seconds of hovering before a tooltip shows
};

const Theme& DefaultTheme();

// Hull colour for a remaining fraction: green, then yellow, then red.
Color HullColor(const Theme& theme, float fraction);

// Breaks text into lines no wider than `maxWidth` at spaces (a word wider than a line is left to be clipped). With
// `maxLines`, the last line ends with an ellipsis when text remains.
std::vector<std::string> WrapText(const TextMeasurer& measurer, std::string_view text, float size, float maxWidth,
                                  int maxLines = 0, FontWeight weight = FontWeight::Regular);

struct LabelStyle {
  float size = 0.0f;       // UI pixels; 0 = theme text size
  Color color{0, 0, 0, 0}; // alpha 0 = theme text colour
  Align align = Align::Start;
  VAlign valign = VAlign::Middle;
  FontWeight weight = FontWeight::Regular;
};

class Ui
{
  public:
  explicit Ui(const Theme& theme = DefaultTheme());

  // Hit-tests an input event against the previous frame; true when the UI consumed it.
  bool ProcessEvent(const InputEvent& event);
  // `seconds` is a monotonic clock (for tooltip delays); the screen's uiScale multiplies the theme's sizes.
  void BeginFrame(DrawList& draw, const TextMeasurer& text, const ScreenInfo& screen, double seconds);
  void EndFrame();

  // True when the mouse is over anything the UI drew last frame, or a widget holds the mouse.
  bool MouseOverUi() const;
  bool MouseCaptured() const
  {
    return m_active != 0;
  }
  Point Mouse() const
  {
    return m_mouse;
  }

  const Theme& GetTheme() const
  {
    return m_theme;
  }
  void SetTheme(const Theme& theme)
  {
    m_theme = theme;
  }
  float Scale() const
  {
    return m_scale;
  }
  float S(float uiPixels) const
  {
    return uiPixels * m_scale;
  }
  DrawList& Draw()
  {
    return *m_draw;
  }
  const TextMeasurer& Text() const
  {
    return *m_text;
  }
  float TextWidth(std::string_view text, float uiSize, FontWeight weight = FontWeight::Regular) const;

  // A background that blocks clicks from reaching the battle view. With a title, returns the content area below it.
  Rect Panel(const Rect& rect, std::string_view title = {});
  // Records an area that blocks the battle view without drawing anything.
  void Block(const Rect& rect);
  void Label(const Rect& rect, std::string_view text, const LabelStyle& style = {});
  bool Button(WidgetId id, const Rect& rect, std::string_view label, bool enabled = true);
  // A button that shows an on state; returns true when clicked (the caller decides what the click selects).
  bool Selectable(WidgetId id, const Rect& rect, std::string_view label, bool on, bool enabled = true);
  // Flips `value` when clicked; returns true when it changed.
  bool Toggle(WidgetId id, const Rect& rect, std::string_view label, bool& value);
  void Bar(const Rect& rect, float fraction, Color fill, Color back = {0, 0, 0, 0});
  // A scrolling list of `rows` rows; `drawRow` paints row `index` into `rowRect`. Returns the clicked row or -1.
  using RowPainter = std::function<void(int index, const Rect& rowRect, bool hovered, bool selected)>;
  int List(WidgetId id, const Rect& rect, int rows, float rowHeight, int selectedRow, const RowPainter& drawRow);
  // Shows `text` beside the mouse at the end of the frame, once the widget has been hovered for the theme's delay.
  void Tooltip(WidgetId id, std::string_view text);
  // An invisible button that the caller draws: hit-tested, hovered and clicked like one.
  bool Region(WidgetId id, const Rect& rect, bool enabled = true)
  {
    return Interact(id, rect, enabled);
  }
  // The widget holding the left button (pressed on it, not yet released), or 0; with where the press happened, for
  // dragging out of a widget.
  WidgetId ActiveId() const
  {
    return m_active;
  }
  Point PressPoint() const
  {
    return m_pressPoint;
  }
  Point ClickPoint() const
  {
    return m_clickPoint;
  }
  // The scroll offset of an area the caller lays out itself; `id` must be a widget drawn over the area (a Region), so
  // that the wheel over it scrolls by `wheelStep` pixels per notch. Clamped to the content.
  float ScrollOffset(WidgetId id, float contentHeight, float viewHeight, float wheelStep);
  // The widget whose press ended with a real button release since the last frame (a drag's drop); a lost focus
  // abandons the press without one.
  WidgetId ReleasedId() const
  {
    return m_released;
  }
  // The click count (2 = double-click) of the click being reported this frame.
  int ClickCount() const
  {
    return m_clickCount;
  }

  bool Hovered(WidgetId id) const
  {
    return m_hoveredId == id && id != 0;
  }
  bool MouseIn(const Rect& rect) const;

  private:
  struct Hit {
    WidgetId id = 0;
    Rect rect;
  };

  bool Interact(WidgetId id, const Rect& rect, bool enabled);
  void DrawButtonFace(const Rect& rect, bool hovered, bool pressed, bool on, bool enabled);
  const Hit* HitAt(Point p) const;

  Theme m_theme;
  DrawList* m_draw = nullptr;
  const TextMeasurer* m_text = nullptr;
  float m_scale = 1.0f;
  Rect m_screen;
  double m_seconds = 0.0;

  Point m_mouse{-1.0f, -1.0f};
  WidgetId m_active = 0;  // the widget holding the left button
  WidgetId m_clicked = 0; // released over the widget it was pressed on; reported next frame
  WidgetId m_released = 0;
  Point m_clickPoint;
  Point m_pressPoint;
  int m_clickCount = 1;
  WidgetId m_hoveredId = 0;
  WidgetId m_tooltipId = 0;
  double m_hoverStart = 0.0;
  std::string m_tooltip;
  std::vector<Hit> m_hits;     // widgets drawn this frame
  std::vector<Hit> m_lastHits; // ... and in the previous frame, which input is tested against
  std::vector<Rect> m_blockers;
  std::vector<Rect> m_lastBlockers;
  std::unordered_map<WidgetId, float> m_scroll;
  std::unordered_map<WidgetId, float> m_pendingScroll;
};

} // namespace reb::battle_ui
