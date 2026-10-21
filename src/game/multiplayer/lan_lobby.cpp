#include "multiplayer/lan_lobby.h"

#include <algorithm>
#include <charconv>
#include <ctime>
#include <filesystem>
#include <memory>

#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "port/native/lan_game.h"

namespace
{
constexpr UINT_PTR kRefreshTimer = 14;
constexpr UINT kRefreshMilliseconds = 150;
constexpr UINT kMouseWheelMessage = 0x020a;
constexpr size_t kCheckpointRows = 3;
constexpr uint32_t kLobbyChildId = 14;
constexpr int kMainMenuState = 3;
constexpr COLORREF kBackground = RGB(7, 14, 25);
constexpr COLORREF kPanel = RGB(13, 27, 42);
constexpr COLORREF kPanelLight = RGB(19, 38, 56);
constexpr COLORREF kEdge = RGB(42, 78, 101);
constexpr COLORREF kAccent = RGB(54, 213, 207);
constexpr COLORREF kText = RGB(222, 235, 241);
constexpr COLORREF kMuted = RGB(135, 159, 174);
constexpr COLORREF kGold = RGB(232, 184, 91);
constexpr COLORREF kDanger = RGB(243, 139, 127);

RECT Rect(int left, int top, int right, int bottom) { return {left, top, right, bottom}; }
bool Contains(RECT area, int x, int y) { return x >= area.left && x < area.right && y >= area.top && y < area.bottom; }

HFONT MakeFont(int height, LONG weight, bool italic = false)
{
  LOGFONTA font{};
  font.lfHeight = -height;
  font.lfWeight = weight;
  font.lfCharSet = 1;
  font.lfQuality = PROOF_QUALITY;
  font.lfPitchAndFamily = 0x31;
  font.lfItalic = italic;
  const char face[] = "Arial";
  for (size_t i = 0; i < sizeof(face); ++i) font.lfFaceName[i] = face[i];
  return reb::compat::CreateFontIndirectA(&font);
}

std::string DateLabel(int64_t timestamp)
{
  const std::time_t value = static_cast<std::time_t>(timestamp);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &value);
#else
  localtime_r(&value, &local);
#endif
  char text[32]{};
  if (std::strftime(text, sizeof(text), "%b %d, %Y", &local) == 0) return "Unknown date";
  return text;
}

} // namespace

LanLobbyWindow::LanLobbyWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent)
    : CoolWindow(instance, "LAN_LOBBY_WINDOW", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, x, y, width, height, parent,
                 nullptr, static_cast<HBRUSH>(reb::compat::GetStockObject(BLACK_BRUSH)), nullptr, reb::compat::LoadCursorA(nullptr, IDC_ARROW),
                 nullptr, 0, 0)
{
  m_childId = kLobbyChildId;
  m_headingFont = MakeFont(22, FW_BOLD);
  m_bodyFont = MakeFont(15, FW_NORMAL);
  m_smallFont = MakeFont(13, FW_NORMAL);
  m_hintFont = MakeFont(11, FW_NORMAL);
  if (auto* game = Game()) game->Discover();
}

LanLobbyWindow::~LanLobbyWindow()
{
  if (m_hwnd) reb::compat::KillTimer(m_hwnd, kRefreshTimer);
  if (m_headingFont) reb::compat::DeleteObject(m_headingFont);
  if (m_bodyFont) reb::compat::DeleteObject(m_bodyFont);
  if (m_smallFont) reb::compat::DeleteObject(m_smallFont);
  if (m_hintFont) reb::compat::DeleteObject(m_hintFont);
}

reb::native::LanGame* LanLobbyWindow::Game() const
{
  return reb::native::CurrentLanGame();
}

void LanLobbyWindow::Refresh()
{
  if (m_hwnd) reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
}

void LanLobbyWindow::RefreshCheckpointCache(const reb::native::LanGame* game, bool force)
{
  if (!force && m_checkpointsLoaded) return;
  std::filesystem::path selectedPath;
  if (m_selectedCheckpoint < m_checkpoints.size()) selectedPath = m_checkpoints[m_selectedCheckpoint].path;
  m_checkpoints = game ? game->Checkpoints() : std::vector<reb::native::CampaignCheckpointInfo>{};
  if (!selectedPath.empty()) {
    const auto found = std::find_if(m_checkpoints.begin(), m_checkpoints.end(), [&selectedPath](const auto& item) {
      return item.path == selectedPath;
    });
    m_selectedCheckpoint = found == m_checkpoints.end() ? static_cast<size_t>(-1)
                                                        : static_cast<size_t>(found - m_checkpoints.begin());
  }
  const size_t pages = std::max<size_t>(1, (m_checkpoints.size() + kCheckpointRows - 1) / kCheckpointRows);
  if (m_checkpointPage >= pages) m_checkpointPage = pages - 1;
  if (m_selectedCheckpoint >= m_checkpoints.size()) m_selectedCheckpoint = static_cast<size_t>(-1);
  m_checkpointsLoaded = true;
}

void LanLobbyWindow::RefreshHostAddress(const reb::native::LanGame* game)
{
  if (!game || !game->IsHost()) {
    m_hostAddress.clear();
    m_hostAddressPort = 0;
    return;
  }
  if (m_hostAddressPort == game->Port() && !m_hostAddress.empty()) return;
  const auto addresses = reb::native::lan::LocalAddresses();
  const std::string address = addresses.empty() ? "127.0.0.1" : addresses.front();
  m_hostAddress = address + ":" + std::to_string(game->Port());
  m_hostAddressPort = game->Port();
}

void LanLobbyWindow::ChangeCheckpointPage(int direction)
{
  const size_t pages = std::max<size_t>(1, (m_checkpoints.size() + kCheckpointRows - 1) / kCheckpointRows);
  if (direction < 0 && m_checkpointPage > 0) --m_checkpointPage;
  if (direction > 0 && m_checkpointPage + 1 < pages) ++m_checkpointPage;
  m_selectedCheckpoint = static_cast<size_t>(-1);
  Refresh();
}

void LanLobbyWindow::DrawText(HDC dc, const char* text, RECT area, COLORREF color, int font, UINT format)
{
  HFONT selected = font == 1 ? m_headingFont : (font == 2 ? m_smallFont : (font == 3 ? m_hintFont : m_bodyFont));
  HGDIOBJ previous = selected ? reb::compat::SelectObject(dc, selected) : nullptr;
  reb::compat::SetTextColor(dc, color);
  reb::compat::SetBkMode(dc, TRANSPARENT);
  reb::compat::DrawTextA(dc, text, -1, &area, format | DT_NOPREFIX);
  if (previous) reb::compat::SelectObject(dc, previous);
}

void LanLobbyWindow::DrawPanel(HDC dc, RECT area, COLORREF fill, COLORREF edge)
{
  HBRUSH brush = reb::compat::CreateSolidBrush(fill);
  HPEN pen = reb::compat::CreatePen(PS_SOLID, 1, edge);
  HGDIOBJ oldBrush = reb::compat::SelectObject(dc, brush);
  HGDIOBJ oldPen = reb::compat::SelectObject(dc, pen);
  reb::compat::Rectangle(dc, area.left, area.top, area.right, area.bottom);
  reb::compat::SelectObject(dc, oldBrush);
  reb::compat::SelectObject(dc, oldPen);
  reb::compat::DeleteObject(brush);
  reb::compat::DeleteObject(pen);
}

void LanLobbyWindow::DrawButton(HDC dc, RECT area, const char* label, bool selected, bool enabled, int font)
{
  DrawPanel(dc, area, selected ? RGB(19, 61, 69) : (enabled ? kPanelLight : RGB(14, 23, 33)),
            selected ? kAccent : kEdge);
  DrawText(dc, label, area, selected ? kAccent : (enabled ? kText : RGB(87, 106, 118)), font,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void LanLobbyWindow::DrawSettings(HDC dc, const reb::native::LanGame* game)
{
  const bool editable = m_tab == 0 && (!game || !game->IsGuest());
  auto settings = game ? game->Settings() : reb::native::LanSettings{};
  if (m_tab == 0 && (!game || !game->IsHost())) {
    settings.hostSide = m_hostSide;
    settings.galaxySize = m_galaxySize;
    settings.victoryMode = m_victoryMode;
  }
  const int visibleSide = m_tab == 1 ? (game && game->IsGuest() ? 3 - settings.hostSide : 0)
                                     : settings.hostSide;
  const bool settingsKnown = m_tab == 0 || (game && game->IsGuest());
  DrawPanel(dc, Rect(318, 116, 616, 260), kPanel, kEdge);
  DrawText(dc, "CAMPAIGN SETTINGS", Rect(334, 126, 600, 149), kAccent, 2);
  DrawText(dc, "YOUR FACTION", Rect(334, 158, 430, 180), kMuted, 3);
  DrawButton(dc, Rect(430, 153, 510, 182), "Alliance", visibleSide == 1, editable);
  DrawButton(dc, Rect(516, 153, 600, 182), "Empire", visibleSide == 2, editable);
  DrawText(dc, "GALAXY", Rect(334, 195, 430, 217), kMuted, 2);
  DrawButton(dc, Rect(430, 190, 483, 220), "Small", settingsKnown && settings.galaxySize == 1, editable);
  DrawButton(dc, Rect(489, 190, 542, 220), "Medium", settingsKnown && settings.galaxySize == 2, editable);
  DrawButton(dc, Rect(548, 190, 600, 220), "Large", settingsKnown && settings.galaxySize == 3, editable);
  DrawText(dc, "VICTORY", Rect(334, 231, 430, 252), kMuted, 2);
  DrawButton(dc, Rect(430, 226, 510, 255), "Standard", settingsKnown && settings.victoryMode == 1, editable);
  DrawButton(dc, Rect(516, 226, 600, 255), "HQ only", settingsKnown && settings.victoryMode == 2, editable);
}

void LanLobbyWindow::DrawCheckpoints(HDC dc)
{
  DrawPanel(dc, Rect(318, 270, 616, 376), kPanel, kEdge);
  DrawText(dc, "RESUME A CAMPAIGN", Rect(334, 278, 478, 299), kAccent, 2);
  DrawButton(dc, Rect(480, 276, 522, 298), "NEW", m_selectedCheckpoint == static_cast<size_t>(-1));
  const size_t pages = std::max<size_t>(1, (m_checkpoints.size() + kCheckpointRows - 1) / kCheckpointRows);
  DrawButton(dc, Rect(530, 276, 561, 298), "<", false, m_checkpointPage > 0);
  DrawButton(dc, Rect(567, 276, 598, 298), ">", false, m_checkpointPage + 1 < pages);
  if (m_checkpoints.empty()) {
    DrawText(dc, "No saved LAN campaigns yet.", Rect(334, 310, 600, 342), kMuted, 2);
    return;
  }
  const size_t first = m_checkpointPage * kCheckpointRows;
  const size_t count = std::min(kCheckpointRows, m_checkpoints.size() - first);
  for (size_t row = 0; row < count; ++row) {
    const size_t i = first + row;
    const auto& checkpoint = m_checkpoints[i];
    const int top = 303 + static_cast<int>(row) * 22;
    const bool selected = i == m_selectedCheckpoint;
    if (selected) DrawPanel(dc, Rect(330, top - 1, 606, top + 19), RGB(20, 48, 60), kAccent);
    std::string label = DateLabel(checkpoint.metadata.unixTimestamp);
    label += checkpoint.automatic ? "  -  Auto" : "  -  Manual";
    if (!checkpoint.metadata.displayName.empty()) label = checkpoint.metadata.displayName + "  -  " + label;
    if (label.size() > 46) label.resize(43), label += "...";
    DrawText(dc, label.c_str(), Rect(338, top, 596, top + 17), selected ? kText : kMuted, 2,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  }
}

void LanLobbyWindow::Paint(HDC dc)
{
  RECT client{0, 0, m_clientWidth, m_clientHeight};
  HBRUSH background = reb::compat::CreateSolidBrush(kBackground);
  reb::compat::FillRect(dc, &client, background);
  reb::compat::DeleteObject(background);
  HPEN gridPen = reb::compat::CreatePen(PS_SOLID, 1, RGB(16, 34, 49));
  HGDIOBJ previousPen = reb::compat::SelectObject(dc, gridPen);
  for (int y = 62; y < m_clientHeight; y += 32) {
    reb::compat::MoveToEx(dc, 0, y, nullptr);
    reb::compat::LineTo(dc, m_clientWidth, y);
  }
  reb::compat::SelectObject(dc, previousPen);
  reb::compat::DeleteObject(gridPen);

  DrawText(dc, "LAN MULTIPLAYER", Rect(24, 17, 430, 48), kText, 1);
  DrawText(dc, "Two players on the same network", Rect(26, 47, 490, 65), kMuted, 2);
  DrawButton(dc, Rect(516, 20, 616, 52), "BACK", false);
  DrawButton(dc, Rect(24, 73, 164, 108), "HOST A GAME", m_tab == 0);
  DrawButton(dc, Rect(170, 73, 310, 108), "JOIN A GAME", m_tab == 1);

  const auto* game = Game();
  RefreshCheckpointCache(game);
  RefreshHostAddress(game);
  if (m_tab == 0) {
    DrawPanel(dc, Rect(24, 116, 306, 376), kPanel, kEdge);
    DrawText(dc, "HOST A CAMPAIGN", Rect(40, 128, 290, 151), kAccent, 2);
    DrawText(dc, "SESSION NAME", Rect(40, 162, 280, 183), kMuted, 2);
    DrawPanel(dc, Rect(40, 185, 290, 221), m_activeField == 1 ? RGB(18, 43, 57) : RGB(9, 20, 31),
              m_activeField == 1 ? kAccent : kEdge);
    DrawText(dc, m_sessionName.c_str(), Rect(50, 190, 280, 216), kText, 0, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawButton(dc, Rect(40, 236, 290, 276), game && game->IsHost() ? "CANCEL HOSTING" : "HOST SESSION",
               game && game->IsHost(), true);
    if (game && game->IsHost()) {
      DrawText(dc, "SHARE THIS ADDRESS", Rect(42, 283, 287, 302), kMuted, 2);
      DrawText(dc, m_hostAddress.c_str(), Rect(42, 303, 287, 327), kAccent, 0,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
      DrawText(dc, "Create a session for a nearby player.", Rect(42, 285, 287, 325), kMuted, 2, DT_WORDBREAK);
    }
    if (game && game->Connected()) {
      DrawText(dc, game->PeerReady() ? "Player connected and ready." : "Player connected; waiting for ready.",
               Rect(42, 331, 287, 356), game->PeerReady() ? kAccent : kGold, 2, DT_WORDBREAK);
    }
    DrawSettings(dc, game);
    DrawCheckpoints(dc);
  } else {
    DrawPanel(dc, Rect(24, 116, 306, 376), kPanel, kEdge);
    DrawText(dc, "NEARBY GAMES", Rect(40, 128, 286, 151), kAccent, 2);
    DrawButton(dc, Rect(204, 124, 290, 153), "SCAN", false);
    const auto nearby = game ? game->NearbyGames() : std::vector<reb::native::lan::Advertisement>{};
    if (nearby.empty()) {
      DrawText(dc, "No games found. Make sure both computers are on the same network.", Rect(42, 165, 287, 222), kMuted,
               2, DT_WORDBREAK);
    } else {
      const size_t count = std::min<size_t>(5, nearby.size());
      for (size_t i = 0; i < count; ++i) {
        const RECT row = Rect(38, 160 + static_cast<int>(i) * 31, 292, 188 + static_cast<int>(i) * 31);
        DrawPanel(dc, row, i == m_selectedNearby ? RGB(20, 48, 60) : kPanelLight,
                  i == m_selectedNearby ? kAccent : kEdge);
        std::string label = nearby[i].name + "  ·  " + nearby[i].address;
        DrawText(dc, label.c_str(), Rect(row.left + 8, row.top + 3, row.right - 7, row.bottom - 3),
                 i == m_selectedNearby ? kText : kMuted, 2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
      }
    }
    DrawText(dc, "OR ENTER AN ADDRESS", Rect(40, 325, 285, 344), kMuted, 2);
    DrawPanel(dc, Rect(40, 346, 190, 373), m_activeField == 2 ? RGB(18, 43, 57) : RGB(9, 20, 31),
              m_activeField == 2 ? kAccent : kEdge);
    DrawText(dc, m_manualAddress.c_str(), Rect(48, 347, 185, 370), kText, 2,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawButton(dc, Rect(198, 346, 290, 373), game && game->IsGuest() ? "CANCEL" : "CONNECT", false);
    DrawSettings(dc, game);
    DrawPanel(dc, Rect(318, 270, 616, 376), kPanel, kEdge);
    DrawText(dc, game && game->IsGuest() ? "CONNECTED TO HOST" : "READY TO JOIN?", Rect(334, 280, 600, 302), kAccent, 2);
    DrawText(dc, game && game->IsGuest() ? "The host chooses settings. Select Ready when you are set."
                                         : "Select a nearby game or enter its IPv4 address and port.",
             Rect(334, 307, 600, 351), kMuted, 2, DT_WORDBREAK);
    if (game && game->Connected()) {
      DrawText(dc, game->PeerReady() ? "Host is ready." : "Waiting for host to be ready.",
               Rect(334, 350, 600, 372), game->PeerReady() ? kAccent : kGold, 2);
    }
  }

  DrawPanel(dc, Rect(24, 389, 616, 456), RGB(10, 22, 33), kEdge);
  std::string status = game ? game->StatusText() : "LAN service is unavailable.";
  if (status.empty()) status = "Same network required. Allow local network access in your firewall settings.";
  DrawText(dc, status.c_str(), Rect(40, 396, 382, 427), game && game->Connected() ? kAccent : kMuted, 2,
           DT_LEFT | DT_WORDBREAK);
  DrawText(dc, "F8 saves manually; autosaves every 5 min.", Rect(40, 428, 382, 440), kMuted, 3, DT_LEFT | DT_SINGLELINE);
  DrawText(dc, "Guest Wi-Fi may isolate devices.", Rect(40, 441, 382, 453), kMuted, 3, DT_LEFT | DT_SINGLELINE);
  const bool connected = game && game->Connected();
  const bool bothReady = connected && game->Ready() && game->PeerReady();
  DrawButton(dc, Rect(394, 399, 482, 446), game && game->Ready() ? "READY" : "NOT READY",
             game && game->Ready(), connected, 3);
  const bool canStart = game && game->IsHost() && bothReady;
  const bool resume = m_tab == 0 && m_selectedCheckpoint < m_checkpoints.size();
  DrawButton(dc, Rect(490, 399, 600, 446), resume ? "RESUME SELECTED" : "START CAMPAIGN", false,
             canStart, 3);
}

void LanLobbyWindow::GoBack()
{
  if (auto* game = Game()) game->Close();
  AppSetState(kMainMenuState);
}

void LanLobbyWindow::Click(int x, int y)
{
  if (Contains(Rect(516, 20, 616, 52), x, y)) { GoBack(); return; }
  auto* game = Game();
  if (Contains(Rect(24, 73, 164, 108), x, y) || Contains(Rect(170, 73, 310, 108), x, y)) {
    const int nextTab = Contains(Rect(24, 73, 164, 108), x, y) ? 0 : 1;
    if (nextTab != m_tab) {
      if (game && game->Active() && (nextTab == 0 ? game->IsGuest() : game->IsHost())) game->Close();
      m_tab = nextTab;
      m_activeField = 0;
      if (nextTab == 1 && game) game->Discover();
      RefreshCheckpointCache(game, true);
    }
    Refresh();
    return;
  }
  if (m_tab == 0) {
    if (Contains(Rect(40, 185, 290, 221), x, y)) {
      if (!game || !game->Active()) m_activeField = 1;
      reb::compat::SetFocus(m_hwnd); Refresh(); return;
    }
    if (Contains(Rect(40, 236, 290, 276), x, y) && game) {
      if (game->IsHost()) {
        game->Close();
        m_hostAddress.clear();
      } else {
        if (game->Active()) game->Close();
        auto selectedSettings = game->Settings();
        selectedSettings.hostSide = m_hostSide;
        selectedSettings.galaxySize = m_galaxySize;
        selectedSettings.victoryMode = m_victoryMode;
        if (game->Host(m_sessionName)) {
          game->SetSettings(selectedSettings);
          RefreshHostAddress(game);
        }
      }
      RefreshCheckpointCache(game, true);
      Refresh(); return;
    }
    auto settings = game ? game->Settings() : reb::native::LanSettings{};
    if (!game || !game->IsHost()) {
      settings.hostSide = m_hostSide;
      settings.galaxySize = m_galaxySize;
      settings.victoryMode = m_victoryMode;
    }
    bool changed = true;
    if (Contains(Rect(430, 153, 510, 182), x, y)) settings.hostSide = 1;
    else if (Contains(Rect(516, 153, 600, 182), x, y)) settings.hostSide = 2;
    else if (Contains(Rect(430, 190, 483, 220), x, y)) settings.galaxySize = 1;
    else if (Contains(Rect(489, 190, 542, 220), x, y)) settings.galaxySize = 2;
    else if (Contains(Rect(548, 190, 600, 220), x, y)) settings.galaxySize = 3;
    else if (Contains(Rect(430, 226, 510, 255), x, y)) settings.victoryMode = 1;
    else if (Contains(Rect(516, 226, 600, 255), x, y)) settings.victoryMode = 2;
    else changed = false;
    if (changed) {
      m_hostSide = settings.hostSide;
      m_galaxySize = settings.galaxySize;
      m_victoryMode = settings.victoryMode;
      if (game && game->IsHost()) game->SetSettings(settings);
      Refresh();
      return;
    }
    if (Contains(Rect(480, 276, 522, 298), x, y)) {
      m_selectedCheckpoint = static_cast<size_t>(-1);
      Refresh(); return;
    }
    if (Contains(Rect(530, 276, 561, 298), x, y)) { ChangeCheckpointPage(-1); return; }
    if (Contains(Rect(567, 276, 598, 298), x, y)) { ChangeCheckpointPage(1); return; }
    for (size_t row = 0; row < kCheckpointRows; ++row) {
      const size_t index = m_checkpointPage * kCheckpointRows + row;
      if (index < m_checkpoints.size()
          && Contains(Rect(330, 302 + static_cast<int>(row) * 22,
                           606, 322 + static_cast<int>(row) * 22), x, y)) {
        m_selectedCheckpoint = m_selectedCheckpoint == index ? static_cast<size_t>(-1) : index;
        Refresh(); return;
      }
    }
  } else {
    if (Contains(Rect(204, 124, 290, 153), x, y)) { if (game) game->Discover(); Refresh(); return; }
    const auto nearby = game ? game->NearbyGames() : std::vector<reb::native::lan::Advertisement>{};
    for (size_t i = 0; i < std::min<size_t>(5, nearby.size()); ++i) {
      if (Contains(Rect(38, 160 + static_cast<int>(i) * 31, 292, 188 + static_cast<int>(i) * 31), x, y)) {
        m_selectedNearby = i;
        m_manualAddress.clear();
        Refresh(); return;
      }
    }
    if (Contains(Rect(40, 346, 190, 373), x, y)) {
      if (!game || !game->IsGuest()) {
        m_activeField = 2;
        m_selectedNearby = static_cast<size_t>(-1);
      }
      reb::compat::SetFocus(m_hwnd); Refresh(); return;
    }
    if (Contains(Rect(198, 346, 290, 373), x, y) && game) {
      if (game->IsGuest()) {
        game->Close();
        game->Discover();
        Refresh(); return;
      }
      std::string address = m_manualAddress;
      uint16_t port = reb::native::lan::kDefaultPort;
      if (!address.empty()) {
        const auto colon = address.rfind(':');
        if (colon != std::string::npos) {
          const std::string portText = address.substr(colon + 1);
          uint32_t parsed = 0;
          const auto [end, error] = std::from_chars(portText.data(), portText.data() + portText.size(), parsed);
          if (error != std::errc{} || end != portText.data() + portText.size() || parsed == 0 || parsed > 65535) {
            Refresh(); return;
          }
          port = static_cast<uint16_t>(parsed);
          address.resize(colon);
        }
      } else if (auto games = game->NearbyGames(); m_selectedNearby < games.size()) {
        address = games[m_selectedNearby].address;
        port = games[m_selectedNearby].port;
      }
      if (!address.empty()) game->Join(address, port);
      Refresh(); return;
    }
  }

  if (Contains(Rect(394, 399, 482, 446), x, y) && game && game->Connected()) {
    game->SetReady(!game->Ready());
    Refresh(); return;
  }
  if (Contains(Rect(490, 399, 600, 446), x, y) && game && game->IsHost() && game->Connected()
      && game->Ready() && game->PeerReady()) {
    auto* application = dynamic_cast<RebellionApplication*>(m_parent);
    if (!application) return;
    RefreshCheckpointCache(game, true);
    if (m_tab == 0 && m_selectedCheckpoint < m_checkpoints.size()) {
      const auto path = m_checkpoints[m_selectedCheckpoint].path;
      game->Resume(*application, path);
    } else {
      game->StartNew(*application);
    }
    return;
  }
}

void LanLobbyWindow::HandleCharacter(WPARAM character)
{
  std::string* field = m_activeField == 1 ? &m_sessionName : (m_activeField == 2 ? &m_manualAddress : nullptr);
  if (!field) return;
  if (m_activeField == 1 && Game() && Game()->Active()) return;
  if (m_activeField == 2 && Game() && Game()->IsGuest()) return;
  if (m_activeField == 2 && m_tab == 1) m_selectedNearby = static_cast<size_t>(-1);
  if (character == '\b') {
    if (!field->empty()) field->pop_back();
  } else if (character >= 32 && character < 127 && field->size() < (m_activeField == 1 ? 48U : 64U)) {
    field->push_back(static_cast<char>(character));
  }
  Refresh();
}

LRESULT LanLobbyWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message) {
    case WM_CREATE:
      reb::compat::SetTimer(m_hwnd, kRefreshTimer, kRefreshMilliseconds, nullptr);
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = reb::compat::BeginPaint(m_hwnd, &paint);
      Paint(dc);
      reb::compat::EndPaint(m_hwnd, &paint);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_TIMER:
      if (wParam == kRefreshTimer) {
        RefreshCheckpointCache(Game());
        RefreshHostAddress(Game());
        Refresh(); return 0;
      }
      break;
    case WM_LBUTTONUP:
      Click(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
      return 0;
    case WM_CHAR:
      if (wParam == '\r') {
        auto* game = Game();
        if (m_tab == 0) {
          if (game && game->IsHost()) {
            if (game->Connected() && game->Ready() && game->PeerReady()) Click(540, 420);
          } else {
            Click(120, 255);
          }
        } else if (!game || !game->IsGuest()) {
          Click(245, 360);
        }
      } else {
        HandleCharacter(wParam);
      }
      return 0;
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) { GoBack(); return 0; }
      if (m_tab == 0 && wParam == VK_PRIOR) { ChangeCheckpointPage(-1); return 0; }
      if (m_tab == 0 && wParam == VK_NEXT) { ChangeCheckpointPage(1); return 0; }
      if (wParam == VK_TAB) {
        m_activeField = m_tab == 0 ? 1 : (m_activeField == 2 ? 0 : 2);
        Refresh();
        return 0;
      }
      break;
    case kMouseWheelMessage:
      if (m_tab == 0) {
        ChangeCheckpointPage(static_cast<short>(HIWORD(wParam)) > 0 ? -1 : 1);
        return 0;
      }
      break;
  }
  return CoolWindow::WindowProc(message, wParam, lParam);
}
