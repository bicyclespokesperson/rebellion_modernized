#include "battle_ui/battle_screen.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <locale>

namespace reb::battle_ui
{
namespace
{
void WriteUnits(std::ostream& out, const std::vector<battle::UnitIndex>& units)
{
  out << units.size();
  for (auto unit : units) {
    out << ' ' << unit;
  }
  out << '\n';
}

bool ReadUnits(std::istream& in, std::vector<battle::UnitIndex>& units, size_t unitCount)
{
  size_t count = 0;
  if (!(in >> count) || count > unitCount) {
    return false;
  }
  units.resize(count);
  for (auto& unit : units) {
    if (!(in >> unit) || unit < 0 || static_cast<size_t>(unit) >= unitCount) {
      return false;
    }
  }
  return true;
}
} // namespace

std::string BattleScreen::SaveUiState() const
{
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10);
  out << "REB_BATTLE_UI 1\n";
  out << m_camera.Center().x << ' ' << m_camera.Center().y << ' ' << m_camera.Zoom() / m_camera.UiScale()
      << ' ' << m_camera.RotationStep() << ' ' << static_cast<int>(m_camera.View()) << '\n';
  out << m_paused << ' ' << m_speedIndex << ' ' << m_accumulator << ' ' << m_firstOrderGiven << ' '
      << m_follow << ' ' << (m_overlayOverride ? (*m_overlayOverride ? 1 : 0) : -1) << '\n';
  WriteUnits(out, m_selection.Units());
  for (int key = 0; key < HotkeyGroups::kCount; ++key) {
    WriteUnits(out, m_hotkeys.Get(key));
  }
  for (const auto& bookmark : m_bookmarks) {
    out << static_cast<bool>(bookmark);
    if (bookmark) {
      out << ' ' << bookmark->center.x << ' ' << bookmark->center.y << ' ' << bookmark->zoom << ' '
          << bookmark->rotation;
    }
    out << '\n';
  }
  out << m_panels.size() << '\n';
  for (const auto& panel : m_panels) {
    out << std::quoted(panel->SaveUiState()) << '\n';
  }
  return out.str();
}

bool BattleScreen::RestoreUiState(std::string_view state)
{
  std::istringstream in{std::string(state)};
  in.imbue(std::locale::classic());
  std::string magic;
  int version = 0;
  battle::Vec2 center;
  float zoom = 0;
  int rotation = 0;
  int view = 0;
  bool paused = false;
  int speed = 0;
  double accumulator = 0;
  bool firstOrder = false;
  battle::UnitIndex follow = battle::kNoUnit;
  int overlay = 0;
  if (!(in >> magic >> version) || magic != "REB_BATTLE_UI" || version != 1
      || !(in >> center.x >> center.y >> zoom >> rotation >> view >> paused >> speed >> accumulator >> firstOrder
           >> follow >> overlay)
      || !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(zoom)
      || zoom < Camera::kMinZoom || zoom > Camera::kMaxZoom || rotation < 0 || rotation >= Camera::kRotationSteps
      || view < 0 || view > 1 || speed < 0 || speed >= static_cast<int>(kSpeeds.size())
      || !std::isfinite(accumulator) || accumulator < 0 || accumulator >= 1 || overlay < -1 || overlay > 1
      || follow < battle::kNoUnit || (follow >= 0 && static_cast<size_t>(follow) >= m_labels.size())) {
    return false;
  }
  std::vector<battle::UnitIndex> selection;
  HotkeyGroups hotkeys;
  if (!ReadUnits(in, selection, m_labels.size())) {
    return false;
  }
  for (int key = 0; key < HotkeyGroups::kCount; ++key) {
    std::vector<battle::UnitIndex> units;
    if (!ReadUnits(in, units, m_labels.size())) {
      return false;
    }
    hotkeys.Store(key, std::move(units));
  }
  decltype(m_bookmarks) bookmarks;
  for (auto& bookmark : bookmarks) {
    bool present = false;
    if (!(in >> present)) {
      return false;
    }
    if (present) {
      CameraBookmark value;
      if (!(in >> value.center.x >> value.center.y >> value.zoom >> value.rotation)
          || !std::isfinite(value.center.x) || !std::isfinite(value.center.y) || !std::isfinite(value.zoom)
          || value.zoom < Camera::kMinZoom || value.zoom > Camera::kMaxZoom
          || value.rotation < 0 || value.rotation >= Camera::kRotationSteps) {
        return false;
      }
      bookmark = value;
    }
  }
  size_t panelCount = 0;
  if (!(in >> panelCount) || panelCount != m_panels.size()) {
    return false;
  }
  std::vector<std::string> panelStates(panelCount);
  for (auto& panelState : panelStates) {
    if (!(in >> std::quoted(panelState))) {
      return false;
    }
  }
  in >> std::ws;
  if (!in.eof()) {
    return false;
  }
  for (size_t i = 0; i < panelStates.size(); ++i) {
    if (!m_panels[i]->RestoreUiState(panelStates[i])) {
      return false;
    }
  }
  m_camera.SetCenter(center);
  m_camera.SetZoom(zoom * m_camera.UiScale());
  m_camera.Rotate(rotation - m_camera.RotationStep());
  m_camera.SetView(static_cast<CameraView>(view));
  m_cameraPlaced = true;
  m_paused = paused;
  m_speedIndex = speed;
  m_accumulator = accumulator;
  m_firstOrderGiven = firstOrder;
  m_follow = follow;
  m_overlayOverride = overlay < 0 ? std::nullopt : std::optional<bool>(overlay != 0);
  m_selection.Set(std::move(selection));
  m_hotkeys = std::move(hotkeys);
  m_bookmarks = bookmarks;
  m_drag = Drag::None;
  m_panKeys = {};
  m_quit = false;
  return true;
}
} // namespace reb::battle_ui
