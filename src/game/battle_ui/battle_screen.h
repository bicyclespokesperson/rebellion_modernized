#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "battle_ui/battle_audio.h"
#include "battle_ui/battle_source.h"
#include "battle_ui/camera.h"
#include "battle_ui/draw_list.h"
#include "battle_ui/input.h"
#include "battle_ui/layout.h"
#include "battle_ui/selection.h"
#include "battle_ui/sprite_catalog.h"
#include "battle_ui/text_measure.h"
#include "battle_ui/widgets.h"

// The live battle screen (tactical_combat.md §5, §10): owns the camera, selection and clock, turns input into
// orders, advances the battle in real time and builds each frame's draw list. SDL-free; the port feeds it input and
// executes what it draws.
namespace reb::battle_ui
{

class BattleScreen;

// A HUD panel drawn over the battle view each frame, after the world and in the order added. Panels read the
// screen's state and act through its public interface (Submit, selection, clock).
class HudPanel
{
  public:
  virtual ~HudPanel() = default;
  virtual void Draw(Ui& ui, BattleScreen& screen) = 0;
  virtual std::string SaveUiState() const { return {}; }
  virtual bool RestoreUiState(std::string_view state) { return state.empty(); }
};

// Lets the HUD take input that the battle view would otherwise act on: a left click on the view while an order waits
// for its target, Escape or a right click to cancel it, and the keyboard while a text field has focus.
class ScreenInputHook
{
  public:
  virtual ~ScreenInputHook() = default;
  // A left click on the battle view; true consumes it (the selection does not change).
  virtual bool ViewClick(battle::UnitIndex unit, battle::Vec2 world, uint8_t modifiers) = 0;
  // Escape, or a right click on the battle view; true consumes it.
  virtual bool Cancel() = 0;
  // Key and text events, before the screen's own shortcuts; true consumes them.
  virtual bool Keyboard(const InputEvent& event) = 0;
  // Every order the player submits, from the view's gestures as well as from the HUD.
  virtual void Submitted(const battle::Command&) {}
};

struct BattleScreenOptions {
  battle::Side playerSide = battle::Side::Alliance;
  bool startPaused = true;
  // Both sides stay under the AI until the player gives the first order (Observe Battle, then Take Command).
  bool aiUntilFirstOrder = true;
  bool preserveAiControl = false;
  // BattleParams::systemPictureId, the planet type drawn behind the defender; 0 draws a generic one.
  uint32_t systemPicture = 0;
};

class BattleScreen
{
  public:
  static constexpr std::array<float, 5> kSpeeds{0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
  static constexpr int kNormalSpeed = 1;
  static constexpr int kMaxTicksPerUpdate = 40; // a slow frame drops battle time rather than spiralling

  BattleScreen(BattleSource& source, const TextMeasurer& text, const SpriteCatalog& sprites,
               const BattleScreenOptions& options = {});
  ~BattleScreen();

  void SetScreen(const ScreenInfo& screen);
  void HandleInput(const InputEvent& event);
  // Advances real time: the camera's key panning, and battle time at the chosen speed in whole 20 Hz ticks.
  void Update(double elapsedSeconds);
  // Appends the frame to `out`.
  void Build(DrawList& out);

  bool QuitRequested() const
  {
    return m_quit;
  }
  void ClearQuitRequest() { m_quit = false; }
  bool BattleFinished() const
  {
    return m_view.finished;
  }

  // ---- state for HUD panels and tests ----
  void AddPanel(std::unique_ptr<HudPanel> panel);
  void SetInputHook(ScreenInputHook* hook)
  {
    m_hook = hook;
  }
  // Accessibility (tactical_combat.md §12 phase 4): the palette, and a factor on the platform's UI scale.
  void SetTheme(const Theme& theme)
  {
    m_ui.SetTheme(theme);
  }
  void SetHudScale(float scale);
  const battle::ViewModel& View() const
  {
    return m_view;
  }
  battle::ViewUnitDetail Detail(battle::UnitIndex unit) const
  {
    return m_source.Detail(unit);
  }
  // Submits a player order (stamped with the side and tick); the first one takes the side off the AI.
  bool Submit(battle::Command command);
  std::vector<AudioCue> TakeAudioCues()
  {
    return m_audio.Take();
  }
  battle::Side PlayerSide() const
  {
    return m_options.playerSide;
  }
  const ScreenInfo& Screen() const
  {
    return m_screen;
  }
  const ScreenLayout& Layout() const
  {
    return m_layout;
  }
  Camera& GetCamera()
  {
    return m_camera;
  }
  const Camera& GetCamera() const
  {
    return m_camera;
  }
  Selection& GetSelection()
  {
    return m_selection;
  }
  const Selection& GetSelection() const
  {
    return m_selection;
  }
  HotkeyGroups& Hotkeys()
  {
    return m_hotkeys;
  }
  battle::UnitIndex Hovered() const
  {
    return m_hovered;
  }
  bool Paused() const
  {
    return m_paused;
  }
  void SetPaused(bool paused);
  int SpeedIndex() const
  {
    return m_speedIndex;
  }
  void SetSpeedIndex(int index);
  bool OverlayVisible() const;
  void SetOverlay(bool on)
  {
    m_overlayOverride = on;
  }
  battle::UnitIndex Following() const
  {
    return m_follow;
  }
  // Locks the camera on a unit until it leaves the battle or the player pans; kNoUnit releases it.
  void Follow(battle::UnitIndex unit);
  // What F and the top bar's Follow button do: follow the first selected unit, or release the camera.
  void ToggleFollow();
  std::string SaveUiState() const;
  bool RestoreUiState(std::string_view state);
  // Frames every unit into the battle area between the HUD's panels (Home).
  void FrameAll();
  void CenterOn(battle::UnitIndex unit);
  // Centres the battle area on a point and stops following a unit.
  void CenterAt(battle::Vec2 position);
  // Switches between the top-down and the angled view (V), keeping the battle area's centre in place.
  void ToggleView();
  bool AngledView() const
  {
    return m_camera.View() == CameraView::Angled;
  }
  // The info box shows the selection without the command HUD; the HUD's roster and card replace it.
  void SetInfoBoxVisible(bool visible)
  {
    m_infoBoxVisible = visible;
  }
  bool InfoBoxVisible() const
  {
    return m_infoBoxVisible;
  }
  double Seconds() const
  {
    return m_seconds;
  }
  // Names, fetched once per unit from the battle's detail.
  struct UnitLabel {
    std::string name; // the unit's own name, else its class and an ordinal (unit_names.h)
    std::string className;
    bool known = false;
    bool ownName = false;
  };
  const UnitLabel& Label(battle::UnitIndex unit) const;

  private:
  enum class Drag : uint8_t { None, PendingSelect, BoxSelect, PendingOrder, OrderDrag, Pan };

  void HandleMouseButton(const MouseButtonEvent& event);
  void HandleMouseMove(const MouseMove& event);
  void HandleWheel(const MouseWheel& event);
  void HandleKey(const KeyEvent& event);
  void FinishLeftClick(Point at, int clicks, uint8_t modifiers);
  void FinishBoxSelect(uint8_t modifiers);
  void FinishRightClick(Point at, uint8_t modifiers);
  void FinishRightDrag(uint8_t modifiers);
  void SelectHotkeyGroup(int key, uint8_t modifiers);
  void PlaceAt(battle::Vec2 position);     // puts a point at the battle area's centre
  void UseBookmark(int index, bool store); // Ctrl+F1-F4 stores the camera, F1-F4 recalls it
  void RebuildView(float alpha);
  void UpdateHover();
  void DrawGestures(DrawList& out) const;
  void DrawHoverCard(DrawList& out) const;
  void DrawOutro();
  void DrawBanners(DrawList& out) const;
  void DrawFollowBanner(DrawList& out, float top) const;
  float DragThreshold() const
  {
    return 5.0f * m_screen.uiScale;
  }
  float MinPickPixels() const
  {
    return 12.0f * m_screen.uiScale;
  }

  BattleSource& m_source;
  const TextMeasurer& m_text;
  const SpriteCatalog& m_sprites;
  BattleScreenOptions m_options;
  ScreenInfo m_screen;
  ScreenLayout m_layout;
  Camera m_camera;
  Ui m_ui;
  std::vector<std::unique_ptr<HudPanel>> m_panels;
  ScreenInputHook* m_hook = nullptr;
  ScreenInfo m_platformScreen; // as the port reports it, before the HUD scale
  float m_hudScale = 1.0f;
  battle::ViewModel m_view;
  BattleAudioCues m_audio;
  Selection m_selection;
  HotkeyGroups m_hotkeys;

  bool m_paused = true;
  int m_speedIndex = kNormalSpeed;
  double m_accumulator = 0.0; // ticks of battle time not yet stepped (counted in ticks so whole seconds are exact)
  double m_seconds = 0.0;     // real time since the screen opened
  std::optional<bool> m_overlayOverride;
  bool m_firstOrderGiven = false;
  bool m_cameraPlaced = false;
  bool m_infoBoxVisible = true; // the player moved the camera: a resize no longer reframes the battle
  bool m_quit = false;
  battle::UnitIndex m_follow = battle::kNoUnit;

  Point m_mouse{-1.0f, -1.0f};
  bool m_mouseKnown = false;
  uint8_t m_modifiers = 0;
  std::array<bool, 4> m_panKeys{}; // left, right, up, down
  Drag m_drag = Drag::None;
  MouseButton m_dragButton = MouseButton::Left;
  Point m_dragStart;
  battle::UnitIndex m_dragUnit = battle::kNoUnit;
  int m_lastDigit = -1;
  double m_lastDigitTime = -10.0;
  battle::UnitIndex m_hovered = battle::kNoUnit;
  double m_hoverSince = 0.0;
  struct Marker {
    battle::Vec2 position;
    double time = 0.0;
    bool attack = false;
  };
  std::vector<Marker> m_markers;
  std::vector<UnitLabel> m_labels;
  std::vector<std::string> m_names;
  std::map<std::pair<int, int32_t>, int> m_classCounts; // (side, type index) -> units named so far
  struct CameraBookmark {
    battle::Vec2 center;
    float zoom = 0.0f; // UI pixels per meter
    int rotation = 0;
  };
  std::array<std::optional<CameraBookmark>, 4> m_bookmarks;
};

} // namespace reb::battle_ui
