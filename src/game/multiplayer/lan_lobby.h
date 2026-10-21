#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "platform/win32.h"
#include "port/native/campaign_checkpoints.h"
#include "ui/cool_window.h"

namespace reb::native
{
class LanGame;
struct CampaignCheckpointInfo;
}

class LanLobbyWindow : public CoolWindow
{
  public:
  LanLobbyWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent);
  ~LanLobbyWindow() override;

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

  private:
  void Paint(HDC dc);
  void Click(int x, int y);
  void HandleCharacter(WPARAM character);
  void Refresh();
  void DrawText(HDC dc, const char* text, RECT area, COLORREF color, int font = 0, UINT format = DT_LEFT);
  void DrawPanel(HDC dc, RECT area, COLORREF fill, COLORREF edge);
  void DrawButton(HDC dc, RECT area, const char* label, bool selected, bool enabled = true, int font = 2);
  void DrawSettings(HDC dc, const reb::native::LanGame* game);
  void DrawCheckpoints(HDC dc);
  void RefreshCheckpointCache(const reb::native::LanGame* game, bool force = false);
  void RefreshHostAddress(const reb::native::LanGame* game);
  void ChangeCheckpointPage(int direction);
  reb::native::LanGame* Game() const;
  void GoBack();

  HFONT m_headingFont = nullptr;
  HFONT m_bodyFont = nullptr;
  HFONT m_smallFont = nullptr;
  HFONT m_hintFont = nullptr;
  std::string m_sessionName = "Rebellion Campaign";
  std::string m_manualAddress;
  int m_hostSide = 1;
  int m_galaxySize = 1;
  int m_victoryMode = 1;
  size_t m_selectedNearby = static_cast<size_t>(-1);
  size_t m_selectedCheckpoint = static_cast<size_t>(-1);
  size_t m_checkpointPage = 0;
  std::vector<reb::native::CampaignCheckpointInfo> m_checkpoints;
  bool m_checkpointsLoaded = false;
  std::string m_hostAddress;
  uint16_t m_hostAddressPort = 0;
  int m_activeField = 0;
  int m_tab = 0;
};
