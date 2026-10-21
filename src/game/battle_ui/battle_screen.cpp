#include "battle_ui/battle_screen.h"

#include <cmath>

#include "battle_ui/battle_hud.h"
#include "battle_ui/orders.h"
#include "battle_ui/plane_shapes.h"
#include "battle_ui/unit_names.h"
#include "battle_ui/world_painter.h"
#include "space_battle/battle.h"

namespace reb::battle_ui
{

using battle::kNoUnit;
using battle::UnitIndex;

namespace
{

constexpr double kDoubleTapSeconds = 0.4;
constexpr double kHoverDelay = 0.25;
constexpr double kMarkerSeconds = 1.0;
constexpr float kPanSpeed = 900.0f;     // UI pixels per second
constexpr float kEdgeScrollBand = 4.0f; // UI pixels at the screen's edge that scroll the view
constexpr float kWheelStep = 1.2f;

} // namespace

BattleScreen::BattleScreen(BattleSource& source, const TextMeasurer& text, const SpriteCatalog& sprites,
                           const BattleScreenOptions& options)
    : m_source(source)
    , m_text(text)
    , m_sprites(sprites)
    , m_options(options)
    , m_audio(options.playerSide)
    , m_paused(options.startPaused)
{
  if (!m_options.preserveAiControl) {
    m_source.SetAiControlled(battle::Side::Alliance, true);
    m_source.SetAiControlled(battle::Side::Empire, true);
    if (!m_options.aiUntilFirstOrder) {
      m_source.SetAiControlled(m_options.playerSide, false);
    }
  }
  SetScreen(m_screen);
  RebuildView(0.0f);
  m_hotkeys.SeedFromGroups(m_view, m_options.playerSide);
  FrameAll();
  AddPanel(std::make_unique<TopBarPanel>());
  AddPanel(std::make_unique<InfoBoxPanel>());
  AddPanel(std::make_unique<KeyHintsPanel>());
}

BattleScreen::~BattleScreen() = default;

void BattleScreen::AddPanel(std::unique_ptr<HudPanel> panel)
{
  m_panels.push_back(std::move(panel));
}

void BattleScreen::SetHudScale(float scale)
{
  m_hudScale = scale;
  SetScreen(m_platformScreen);
}

void BattleScreen::SetScreen(const ScreenInfo& screen)
{
  m_platformScreen = screen;
  m_screen = screen;
  m_screen.uiScale = screen.uiScale * m_hudScale;
  m_layout = ComputeLayout(m_screen);
  m_camera.SetUiScale(m_screen.uiScale);
  m_camera.SetViewport(m_layout.view);
  if (!m_cameraPlaced && !m_view.units.empty()) {
    FrameAll();
  }
}

void BattleScreen::SetPaused(bool paused)
{
  if (paused != m_paused) {
    m_overlayOverride.reset();
  }
  m_paused = paused;
}

void BattleScreen::SetSpeedIndex(int index)
{
  m_speedIndex = std::clamp(index, 0, static_cast<int>(kSpeeds.size()) - 1);
}

bool BattleScreen::OverlayVisible() const
{
  return m_overlayOverride.value_or(m_paused);
}

void BattleScreen::FrameAll()
{
  WorldBox box{{1e30f, 1e30f}, {-1e30f, -1e30f}};
  bool any = false;
  for (const battle::ViewUnit& unit : m_view.units) {
    if (!Selectable(unit)) {
      continue;
    }
    any = true;
    box.min = {std::min(box.min.x, unit.position.x - unit.radius), std::min(box.min.y, unit.position.y - unit.radius)};
    box.max = {std::max(box.max.x, unit.position.x + unit.radius), std::max(box.max.y, unit.position.y + unit.radius)};
  }
  if (!any) {
    box = {m_view.boundsMin, m_view.boundsMax};
  }
  constexpr float kMargin = 1500.0f;
  box.min = {box.min.x - kMargin, box.min.y - kMargin};
  box.max = {box.max.x + kMargin, box.max.y + kMargin};
  m_camera.FrameBoxInto(box, m_layout.battleArea);
  m_follow = kNoUnit;
}

void BattleScreen::ToggleView()
{
  const battle::Vec2 center = m_camera.ScreenToWorld(m_layout.battleArea.Center());
  m_camera.SetView(m_camera.View() == CameraView::TopDown ? CameraView::Angled : CameraView::TopDown);
  PlaceAt(center);
}

void BattleScreen::PlaceAt(battle::Vec2 position)
{
  m_camera.SetCenter(position);
  m_camera.PanPixels(m_layout.battleArea.Center() - m_camera.Viewport().Center());
}

void BattleScreen::Follow(UnitIndex unit)
{
  m_follow = unit;
  m_cameraPlaced = true;
  if (const battle::ViewUnit* found = FindUnit(m_view, unit)) {
    PlaceAt(found->position);
  }
}

void BattleScreen::ToggleFollow()
{
  const UnitIndex selected = m_selection.Empty() ? kNoUnit : m_selection.Units().front();
  Follow(selected == m_follow ? kNoUnit : selected);
}

void BattleScreen::CenterOn(UnitIndex unit)
{
  if (const battle::ViewUnit* found = FindUnit(m_view, unit)) {
    CenterAt(found->position);
  }
}

void BattleScreen::CenterAt(battle::Vec2 position)
{
  PlaceAt(position);
  m_cameraPlaced = true;
  m_follow = kNoUnit;
}

bool BattleScreen::Submit(battle::Command command)
{
  command.side = m_options.playerSide;
  command.tick = m_source.Tick();
  if (!m_firstOrderGiven && m_options.aiUntilFirstOrder) {
    m_source.SetAiControlled(m_options.playerSide, false);
  }
  m_firstOrderGiven = true;
  if (m_hook) {
    m_hook->Submitted(command);
  }
  if (!m_source.Submit(command)) {
    return false;
  }
  m_audio.Submitted(command, m_view);
  return true;
}

// ---- input --------------------------------------------------------------------------------------------------------

void BattleScreen::HandleInput(const InputEvent& event)
{
  if (const auto* changed = std::get_if<ScreenChanged>(&event)) {
    SetScreen(changed->screen);
    return;
  }
  if (std::holds_alternative<CloseRequested>(event)) {
    m_quit = true;
    return;
  }
  if (std::holds_alternative<FocusLost>(event)) {
    m_drag = Drag::None;
    m_panKeys = {};
    m_modifiers = 0;
    m_ui.ProcessEvent(event);
    return;
  }
  if (const auto* key = std::get_if<KeyEvent>(&event)) {
    if (m_view.decided && !m_view.finished && key->down && !key->repeat
        && (key->key == Key::Space || key->key == Key::Return)) {
      m_source.SkipOutro();
      return;
    }
    m_modifiers = key->modifiers;
    if (!m_hook || !m_hook->Keyboard(event)) {
      HandleKey(*key);
    }
    return;
  }
  if (std::holds_alternative<TextInput>(event)) {
    if (m_hook) {
      m_hook->Keyboard(event);
    }
    return;
  }
  const bool viewDragging = m_drag != Drag::None;
  if (const auto* move = std::get_if<MouseMove>(&event)) {
    m_modifiers = move->modifiers;
    const bool overUi = m_ui.ProcessEvent(event);
    if (viewDragging || !overUi) {
      HandleMouseMove(*move);
    }
    m_mouse = move->position;
    m_mouseKnown = true;
    return;
  }
  if (const auto* button = std::get_if<MouseButtonEvent>(&event)) {
    m_modifiers = button->modifiers;
    m_mouse = button->position;
    m_mouseKnown = true;
    if (viewDragging || !m_ui.ProcessEvent(event)) {
      HandleMouseButton(*button);
    }
    return;
  }
  if (const auto* wheel = std::get_if<MouseWheel>(&event)) {
    m_modifiers = wheel->modifiers;
    if (!m_ui.ProcessEvent(event)) {
      HandleWheel(*wheel);
    }
  }
}

void BattleScreen::HandleMouseButton(const MouseButtonEvent& event)
{
  if (event.down) {
    if (m_drag != Drag::None) {
      return;
    }
    m_dragButton = event.button;
    m_dragStart = event.position;
    switch (event.button) {
      case MouseButton::Left:
        m_drag = Drag::PendingSelect;
        break;
      case MouseButton::Right:
        m_drag = Drag::PendingOrder;
        m_dragUnit = PickUnit(m_view, m_camera, event.position, MinPickPixels());
        break;
      case MouseButton::Middle:
        m_drag = Drag::Pan;
        break;
    }
    return;
  }
  if (event.button != m_dragButton) {
    return;
  }
  const Drag drag = m_drag;
  m_drag = Drag::None;
  switch (drag) {
    case Drag::PendingSelect:
      FinishLeftClick(event.position, event.clicks, event.modifiers);
      break;
    case Drag::BoxSelect:
      FinishBoxSelect(event.modifiers);
      break;
    case Drag::PendingOrder:
      FinishRightClick(event.position, event.modifiers);
      break;
    case Drag::OrderDrag:
      FinishRightDrag(event.modifiers);
      break;
    case Drag::Pan:
    case Drag::None:
      break;
  }
}

void BattleScreen::HandleMouseMove(const MouseMove& event)
{
  const Point delta = m_mouseKnown ? event.position - m_mouse : Point{};
  switch (m_drag) {
    case Drag::PendingSelect:
      if (Distance(event.position, m_dragStart) > DragThreshold()) {
        m_drag = Drag::BoxSelect;
      }
      break;
    case Drag::PendingOrder:
      if (Distance(event.position, m_dragStart) > DragThreshold()) {
        m_drag = Drag::OrderDrag;
      }
      break;
    case Drag::Pan:
      m_camera.PanPixels(delta);
      m_follow = kNoUnit;
      m_cameraPlaced = true;
      break;
    case Drag::BoxSelect:
    case Drag::OrderDrag:
    case Drag::None:
      break;
  }
}

void BattleScreen::HandleWheel(const MouseWheel& event)
{
  // Following, the zoom anchors on the followed unit; anchored on the cursor, the camera would jump back each frame.
  const Point anchor = m_follow != kNoUnit ? m_layout.battleArea.Center() : event.position;
  m_camera.ZoomAt(anchor, std::pow(kWheelStep, event.dy));
  m_cameraPlaced = true;
}

void BattleScreen::HandleKey(const KeyEvent& event)
{
  const auto setPan = [&](int index) {
    if (!event.repeat) {
      m_panKeys[index] = event.down;
    }
  };
  switch (event.key) {
    case Key::A:
    case Key::Left:
      setPan(0);
      return;
    case Key::D:
    case Key::Right:
      setPan(1);
      return;
    case Key::W:
    case Key::Up:
      setPan(2);
      return;
    case Key::S:
    case Key::Down:
      setPan(3);
      return;
    default:
      break;
  }
  if (!event.down) {
    return;
  }
  const bool ctrl = (event.modifiers & kModCtrl) != 0;
  if (const int digit = KeyDigit(event.key); digit >= 0) {
    SelectHotkeyGroup(digit, event.modifiers);
    return;
  }
  if (const int bookmark = static_cast<int>(event.key) - static_cast<int>(Key::F1);
      bookmark >= 0 && bookmark < static_cast<int>(m_bookmarks.size()) && !event.repeat) {
    UseBookmark(bookmark, ctrl);
    return;
  }
  if (event.repeat) {
    return;
  }
  switch (event.key) {
    case Key::Escape:
      if (m_drag != Drag::None) {
        m_drag = Drag::None;
      } else if (!m_hook || !m_hook->Cancel()) {
        m_quit = true;
      }
      break;
    case Key::Space:
      SetPaused(!m_paused);
      break;
    case Key::Tab:
      m_overlayOverride = !OverlayVisible();
      break;
    case Key::Q:
      m_camera.Rotate(1);
      m_cameraPlaced = true;
      break;
    case Key::E:
      m_camera.Rotate(-1);
      m_cameraPlaced = true;
      break;
    case Key::F:
      ToggleFollow();
      break;
    case Key::Home:
      FrameAll();
      break;
    case Key::V:
      ToggleView();
      break;
    case Key::Minus:
    case Key::KeypadMinus:
      SetSpeedIndex(m_speedIndex - 1);
      break;
    case Key::Equals:
    case Key::KeypadPlus:
      SetSpeedIndex(m_speedIndex + 1);
      break;
    case Key::H:
      if (!ctrl) {
        const OrderContext context{m_options.playerSide, m_source.Tick(), m_selection.Orderable(m_view, PlayerSide()),
                                   (event.modifiers & kModShift) != 0};
        if (auto command = SelectionOrder(context, battle::OrderHold{})) {
          Submit(*command);
        }
      }
      break;
    default:
      break;
  }
}

void BattleScreen::UseBookmark(int index, bool store)
{
  std::optional<CameraBookmark>& bookmark = m_bookmarks[index];
  if (store) {
    bookmark = CameraBookmark{m_camera.Center(), m_camera.Zoom() / m_camera.UiScale(), m_camera.RotationStep()};
    return;
  }
  if (!bookmark) {
    return;
  }
  m_camera.SetCenter(bookmark->center);
  m_camera.SetZoom(bookmark->zoom * m_camera.UiScale());
  m_camera.Rotate(bookmark->rotation - m_camera.RotationStep());
  m_follow = kNoUnit;
  m_cameraPlaced = true;
}

void BattleScreen::SelectHotkeyGroup(int key, uint8_t modifiers)
{
  if (modifiers & kModCtrl) {
    m_hotkeys.Store(key, m_selection.Orderable(m_view, PlayerSide()));
    return;
  }
  std::vector<UnitIndex> units;
  for (UnitIndex unit : m_hotkeys.Get(key)) {
    if (const battle::ViewUnit* found = FindUnit(m_view, unit); found && Selectable(*found)) {
      units.push_back(unit);
    }
  }
  if (units.empty()) {
    return;
  }
  const bool doubleTap = key == m_lastDigit && m_seconds - m_lastDigitTime < kDoubleTapSeconds;
  m_lastDigit = key;
  m_lastDigitTime = m_seconds;
  if (modifiers & kModShift) {
    m_selection.Add(units);
  } else {
    m_selection.Set(units);
  }
  if (doubleTap) {
    WorldBox box{FindUnit(m_view, units.front())->position, FindUnit(m_view, units.front())->position};
    for (UnitIndex unit : units) {
      const battle::Vec2 p = FindUnit(m_view, unit)->position;
      box.min = {std::min(box.min.x, p.x), std::min(box.min.y, p.y)};
      box.max = {std::max(box.max.x, p.x), std::max(box.max.y, p.y)};
    }
    PlaceAt({(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f});
    m_cameraPlaced = true;
  }
}

void BattleScreen::FinishLeftClick(Point at, int clicks, uint8_t modifiers)
{
  const UnitIndex unit = PickUnit(m_view, m_camera, at, MinPickPixels());
  if (m_hook && m_hook->ViewClick(unit, m_camera.ScreenToWorld(at), modifiers)) {
    return;
  }
  if (unit == kNoUnit) {
    if (!(modifiers & kModShift)) {
      m_selection.Clear();
    }
    return;
  }
  const bool own = FindUnit(m_view, unit)->side == PlayerSide();
  if (modifiers & kModShift) {
    m_selection.Toggle(unit, own);
  } else if (own) {
    m_selection.Set({unit});
  } else {
    m_selection.Clear();
    m_selection.Toggle(unit, false);
  }
  if (clicks >= 2) {
    CenterOn(unit);
  }
}

void BattleScreen::FinishBoxSelect(uint8_t modifiers)
{
  std::vector<UnitIndex> units = UnitsInRect(m_view, m_camera, RectFromCorners(m_dragStart, m_mouse), PlayerSide());
  if (modifiers & kModShift) {
    m_selection.Add(units);
  } else {
    m_selection.Set(std::move(units));
  }
}

void BattleScreen::FinishRightClick(Point at, uint8_t modifiers)
{
  if (m_hook && m_hook->Cancel()) {
    return;
  }
  const OrderContext context{m_options.playerSide, m_source.Tick(), m_selection.Orderable(m_view, PlayerSide()),
                             (modifiers & kModShift) != 0};
  const UnitIndex clicked = PickUnit(m_view, m_camera, at, MinPickPixels());
  const battle::Vec2 world = m_camera.ScreenToWorld(at);
  if (auto command = ClickOrder(context, m_view, clicked, world, (modifiers & kModCtrl) != 0)) {
    const bool attack = std::holds_alternative<battle::OrderAttack>(command->order);
    if (Submit(*command)) {
      m_markers.push_back({attack ? FindUnit(m_view, clicked)->position : world, m_seconds, attack});
    }
  }
}

void BattleScreen::FinishRightDrag(uint8_t modifiers)
{
  const OrderContext context{m_options.playerSide, m_source.Tick(), m_selection.Orderable(m_view, PlayerSide()),
                             (modifiers & kModShift) != 0};
  const battle::Vec2 from = m_camera.ScreenToWorld(m_dragStart);
  const battle::Vec2 to = m_camera.ScreenToWorld(m_mouse);
  if (auto command = DragOrder(context, m_view, m_dragUnit, from, to)) {
    const bool approach = std::holds_alternative<battle::OrderApproach>(command->order);
    if (Submit(*command)) {
      m_markers.push_back({approach ? FindUnit(m_view, m_dragUnit)->position : from, m_seconds, approach});
    }
  }
}

// ---- time ---------------------------------------------------------------------------------------------------------

void BattleScreen::Update(double elapsedSeconds)
{
  m_seconds += elapsedSeconds;
  const float pan = kPanSpeed * m_screen.uiScale * static_cast<float>(elapsedSeconds);
  const float edge = kEdgeScrollBand * m_screen.uiScale;
  // Edge scrolling never releases a followed unit: reaching for a panel at the window's edge would.
  const bool edgeScroll
      = m_follow == kNoUnit && m_mouseKnown && m_drag == Drag::None && m_layout.screen.Contains(m_mouse);
  const bool left = m_panKeys[0] || (edgeScroll && m_mouse.x < edge);
  const bool right = m_panKeys[1] || (edgeScroll && m_mouse.x >= m_layout.screen.Right() - edge);
  const bool up = m_panKeys[2] || (edgeScroll && m_mouse.y < edge);
  const bool down = m_panKeys[3] || (edgeScroll && m_mouse.y >= m_layout.screen.Bottom() - edge);
  const Point panDelta{(left ? pan : 0.0f) - (right ? pan : 0.0f), (up ? pan : 0.0f) - (down ? pan : 0.0f)};
  if (panDelta.x != 0.0f || panDelta.y != 0.0f) {
    m_camera.PanPixels(panDelta);
    m_follow = kNoUnit;
    m_cameraPlaced = true;
  }
  if (!m_paused && !m_source.Finished()) {
    m_accumulator += elapsedSeconds * kSpeeds[m_speedIndex] * battle::kTicksPerSecond;
    int ticks = 0;
    bool decided = m_view.decided;
    while (m_accumulator >= 1.0 && ticks < kMaxTicksPerUpdate && !m_source.Finished()) {
      m_source.Step();
      battle::ViewModel audioView;
      m_source.BuildViewModel(1.0f, audioView);
      m_audio.Observe(audioView);
      m_accumulator -= 1.0;
      ++ticks;
      if (audioView.decided && !decided) {
        decided = true;
        // The wind-down is battle time: drop to normal speed so it lasts long enough to see the end and hear
        // the result. The player can speed it up again or skip it.
        if (m_speedIndex > kNormalSpeed) {
          SetSpeedIndex(kNormalSpeed);
          m_accumulator = 0.0;
        }
      }
    }
    m_accumulator = std::min(m_accumulator, 0.999);
  }
  RebuildView(static_cast<float>(m_accumulator));
  if (m_follow != kNoUnit) {
    const battle::ViewUnit* followed = FindUnit(m_view, m_follow);
    if (followed && Selectable(*followed)) {
      PlaceAt(followed->position);
    } else {
      m_follow = kNoUnit;
    }
  }
  m_selection.Prune(m_view);
  std::erase_if(m_markers, [this](const Marker& marker) {
    return m_seconds - marker.time > kMarkerSeconds;
  });
  UpdateHover();
}

void BattleScreen::RebuildView(float alpha)
{
  m_source.BuildViewModel(alpha, m_view);
  if (m_source.Tick() == 0 && m_paused) {
    const auto unknownEnemy = [&](battle::UnitIndex index) {
      const auto* unit = FindUnit(m_view, index);
      return unit && unit->side != PlayerSide()
          && !m_source.Detail(index).briefingKnown[static_cast<size_t>(PlayerSide())];
    };
    std::erase_if(m_view.craft, [&](const battle::ViewCraft& craft) { return unknownEnemy(craft.squadron); });
    std::erase_if(m_view.units, [&](const battle::ViewUnit& unit) {
      return unit.side != PlayerSide()
          && !m_source.Detail(unit.unit).briefingKnown[static_cast<size_t>(PlayerSide())];
    });
  }
  m_audio.Observe(m_view);
  for (const battle::ViewUnit& unit : m_view.units) {
    if (unit.unit < 0) {
      continue;
    }
    if (static_cast<size_t>(unit.unit) >= m_labels.size()) {
      m_labels.resize(static_cast<size_t>(unit.unit) + 1);
    }
    UnitLabel& label = m_labels[unit.unit];
    if (!label.known) {
      const int ordinal = ++m_classCounts[{static_cast<int>(unit.side), unit.typeIndex}];
      const UnitNaming naming = NameUnit(unit, m_source.Detail(unit.unit), ordinal);
      label = {naming.name, naming.className, true, naming.ownName};
      m_names.resize(m_labels.size());
      m_names[unit.unit] = naming.name;
    }
  }
}

void BattleScreen::UpdateHover()
{
  UnitIndex hovered = kNoUnit;
  if (m_mouseKnown && !m_ui.MouseOverUi() && m_camera.Viewport().Contains(m_mouse)
      && (m_drag == Drag::None || m_drag == Drag::PendingOrder)) {
    hovered = PickUnit(m_view, m_camera, m_mouse, MinPickPixels());
  }
  if (hovered != m_hovered) {
    m_hovered = hovered;
    m_hoverSince = m_seconds;
  }
}

const BattleScreen::UnitLabel& BattleScreen::Label(UnitIndex unit) const
{
  static const UnitLabel kUnknown;
  return unit >= 0 && static_cast<size_t>(unit) < m_labels.size() ? m_labels[unit] : kUnknown;
}

// ---- drawing ------------------------------------------------------------------------------------------------------

void BattleScreen::Build(DrawList& out)
{
  std::vector<battle::ViewUnitDetail> details;
  if (OverlayVisible()) {
    for (UnitIndex unit : m_selection.Units()) {
      const battle::ViewUnit* found = FindUnit(m_view, unit);
      if (found && found->kind == battle::ViewUnitKind::CapitalShip) {
        details.push_back(m_source.Detail(unit));
      }
    }
  }
  const WorldScene scene{m_view,    m_camera,         m_sprites,    m_ui.GetTheme(), m_seconds, m_selection,
                         m_hovered, OverlayVisible(), PlayerSide(), details,         m_names,   m_options.systemPicture,
                         &m_text};
  PaintWorld(out, scene);
  DrawGestures(out);
  m_ui.BeginFrame(out, m_text, m_screen, m_seconds);
  DrawOutro();
  for (const std::unique_ptr<HudPanel>& panel : m_panels) {
    panel->Draw(m_ui, *this);
  }
  m_ui.EndFrame();
  DrawHoverCard(out);
  DrawBanners(out);
}

void BattleScreen::DrawGestures(DrawList& out) const
{
  const float ui = m_screen.uiScale;
  const Color own = Rgb(0xd8f0ff);
  out.PushClip(m_camera.Viewport());
  for (const Marker& marker : m_markers) {
    const float t = static_cast<float>((m_seconds - marker.time) / kMarkerSeconds);
    const Color color = marker.attack ? Rgb(0xff6a5a) : Rgb(0x8fe0ff);
    const Point at = m_camera.WorldToScreen(marker.position);
    PlaneCircle(out, m_camera, marker.position, m_camera.MetersFor((6.0f + 18.0f * t) * ui), color.Faded(1.0f - t),
                Stroke{2.0f * ui});
    out.FillCircle(at, 2.5f * ui, color.Faded(1.0f - t));
  }
  if (m_drag == Drag::BoxSelect) {
    const Rect box = RectFromCorners(m_dragStart, m_mouse);
    out.FillRect(box, own.WithAlpha(28));
    out.StrokeRect(box, own.WithAlpha(180), 1.0f * ui);
  }
  if (m_drag == Drag::OrderDrag) {
    const battle::ViewUnit* target = m_dragUnit == kNoUnit ? nullptr : FindUnit(m_view, m_dragUnit);
    const bool approach = target && target->side != PlayerSide();
    const Color color = approach ? Rgb(0xff8a6a) : Rgb(0x8fe0ff);
    const Point from = m_dragStart;
    const Point to = m_mouse;
    const float length = Distance(from, to);
    out.Line(from, to, color, Stroke{2.0f * ui});
    out.Circle(from, 6.0f * ui, color, Stroke{1.5f * ui});
    if (length > 1.0f) {
      const Point dir = (to - from) * (1.0f / length);
      const Point normal{-dir.y, dir.x};
      const float head = 10.0f * ui;
      out.FillTriangle(to, to - dir * head + normal * (head * 0.5f), to - dir * head - normal * (head * 0.5f), color);
    }
  }
  out.PopClip();
}

void BattleScreen::DrawHoverCard(DrawList& out) const
{
  if (m_hovered == kNoUnit || m_seconds - m_hoverSince < kHoverDelay || m_drag != Drag::None) {
    return;
  }
  const battle::ViewUnit* unit = FindUnit(m_view, m_hovered);
  if (!unit) {
    return;
  }
  const Theme& theme = m_ui.GetTheme();
  const float ui = m_screen.uiScale;
  const UnitLabel& label = Label(m_hovered);
  const std::string title
      = label.ownName && !label.className.empty() ? label.name + " - " + label.className : label.name;
  const float titleSize = 13.0f * ui;
  const float textSize = 12.0f * ui;
  const float pad = 7.0f * ui;
  const float maxWidth = 420.0f * ui;
  const std::vector<std::string> intent = unit->intent.empty()
                                              ? std::vector<std::string>{}
                                              : WrapText(m_text, unit->intent, textSize, maxWidth - 2.0f * pad, 2);
  float width = m_text.Width(title, titleSize, FontWeight::Bold);
  for (const std::string& line : intent) {
    width = std::max(width, m_text.Width(line, textSize));
  }
  width = std::min(width + 2.0f * pad, maxWidth);
  const float lineHeight = m_text.LineHeight(textSize) + 2.0f * ui;
  const float height = 2.0f * pad + m_text.LineHeight(titleSize) + lineHeight * static_cast<float>(intent.size());
  Rect box{m_mouse.x + 16.0f * ui, m_mouse.y + 20.0f * ui, width, height};
  if (box.Right() > m_layout.screen.Right()) {
    box.x = m_mouse.x - 12.0f * ui - width;
  }
  if (box.Bottom() > m_layout.screen.Bottom()) {
    box.y = m_mouse.y - 12.0f * ui - height;
  }
  out.FillRect(box, theme.tooltip);
  out.StrokeRect(box, FactionColor(theme, unit->side).WithAlpha(180), 1.0f * ui);
  float y = box.y + pad;
  out.Text(title, {box.x + pad, y}, titleSize, FactionColor(theme, unit->side), Align::Start, VAlign::Top,
           FontWeight::Bold, width - 2.0f * pad);
  y += m_text.LineHeight(titleSize) + 2.0f * ui;
  for (const std::string& line : intent) {
    out.Text(line, {box.x + pad, y}, textSize, theme.text, Align::Start, VAlign::Top, FontWeight::Regular,
             width - 2.0f * pad);
    y += lineHeight;
  }
}

void BattleScreen::DrawOutro()
{
  if (!m_view.decided || m_view.finished) return;
  std::string text;
  if (m_view.endReason == battle::ViewEndReason::Stalemate) {
    text = "STALEMATE · NEITHER SIDE CAN BREAK THE OTHER";
  } else if (m_view.endReason == battle::ViewEndReason::TimeLimit) {
    text = "TIME LIMIT REACHED";
  } else {
    bool ownPresent = false;
    bool ownWithdrew = false;
    bool enemyWithdrew = false;
    for (const battle::ViewUnit& unit : m_view.units) {
      const bool own = unit.side == PlayerSide();
      ownPresent |= own && (unit.flags & (battle::kViewDestroyed | battle::kViewWithdrawn)) == 0;
      ownWithdrew |= own && (unit.flags & battle::kViewWithdrawn);
      enemyWithdrew |= !own && (unit.flags & battle::kViewWithdrawn);
    }
    text = ownPresent ? (enemyWithdrew ? "VICTORY · THE ENEMY FLEET HAS WITHDRAWN"
                                     : "VICTORY · ALL ENEMY SHIPS DESTROYED")
                     : (ownWithdrew ? "WITHDRAWAL COMPLETE" : "DEFEAT · OUR FLEET HAS BEEN DESTROYED");
  }
  const float width = std::min(m_layout.view.w, m_ui.S(720));
  const Rect box{m_layout.view.Center().x - width / 2, m_layout.view.y + m_ui.S(48), width, m_ui.S(116)};
  const Rect content = m_ui.Panel(box, "Battle decided");
  const float textWidth = m_text.Width(text, m_ui.S(20), FontWeight::Bold);
  const float textSize = 20.0f * std::min(1.0f, (content.w - m_ui.S(16)) / std::max(1.0f, textWidth));
  m_ui.Label({content.x, content.y, content.w, m_ui.S(40)}, text,
             {.size = textSize, .align = Align::Center, .weight = FontWeight::Bold});
  if (m_ui.Button(Id("battle.continue"),
                  {content.Center().x - m_ui.S(90), content.y + m_ui.S(48), m_ui.S(180), m_ui.S(32)}, "Continue")) {
    m_source.SkipOutro();
  }
}

void BattleScreen::DrawBanners(DrawList& out) const
{
  const Theme& theme = m_ui.GetTheme();
  const float ui = m_screen.uiScale;
  std::string text;
  Color color = theme.title;
  if (m_view.finished) {
    std::array<bool, 2> present{};
    for (const battle::ViewUnit& unit : m_view.units) {
      present[static_cast<int>(unit.side)] |= Selectable(unit);
    }
    text = present[0] && present[1] ? "BATTLE OVER - STALEMATE"
           : present[0]             ? "BATTLE OVER - ALLIANCE HOLDS THE SYSTEM"
           : present[1]             ? "BATTLE OVER - EMPIRE HOLDS THE SYSTEM"
                                    : "BATTLE OVER";
  } else if (m_paused && !m_view.decided) {
    text = "PAUSED";
    color = theme.text;
  }
  const float followTop = m_layout.view.y + (text.empty() ? 10.0f : 42.0f) * ui;
  DrawFollowBanner(out, followTop);
  if (text.empty()) {
    return;
  }
  const float size = 15.0f * ui;
  const float width = m_text.Width(text, size, FontWeight::Bold) + 28.0f * ui;
  const Rect box{m_layout.view.Center().x - width * 0.5f, m_layout.view.y + 10.0f * ui, width, 26.0f * ui};
  out.FillRect(box, theme.panel);
  out.StrokeRect(box, theme.panelBorder, 1.0f * ui);
  out.Text(text, box.Center(), size, color, Align::Center, VAlign::Middle, FontWeight::Bold);
}

void BattleScreen::DrawFollowBanner(DrawList& out, float top) const
{
  if (m_follow == kNoUnit) {
    return;
  }
  const Theme& theme = m_ui.GetTheme();
  const float ui = m_screen.uiScale;
  const std::string& name = Label(m_follow).name;
  const std::string text = name.empty() ? "FOLLOWING  (F to release)" : "FOLLOWING " + name + "  (F to release)";
  const float size = 12.0f * ui;
  const float width = m_text.Width(text, size, FontWeight::Bold) + 20.0f * ui;
  const Rect box{m_layout.view.Center().x - width * 0.5f, top, width, 20.0f * ui};
  out.FillRect(box, theme.panel);
  out.StrokeRect(box, theme.panelBorder, 1.0f * ui);
  out.Text(text, box.Center(), size, theme.text, Align::Center, VAlign::Middle, FontWeight::Bold);
}

} // namespace reb::battle_ui
