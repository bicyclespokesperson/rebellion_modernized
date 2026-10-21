#include "port/native/lan_lobby_smoke.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <thread>

#include "app/rebellion_application.h"
#include "port/minwin_impl/user/present.h"
#include "port/native/game_context.h"
#include "port/native/lan_game.h"

bool LanLobbySmokeRequested()
{
  return std::getenv("REB_LAN_LOBBY_SMOKE") != nullptr;
}

int RunLanLobbySmoke(RebellionApplication& app)
{
  const std::filesystem::path directory = std::getenv("REB_LAN_LOBBY_SMOKE");
  std::filesystem::create_directories(directory);
  if (!app.Create(0)) {
    throw std::runtime_error("Could not create the LAN lobby smoke window");
  }
  app.Show(5);
  app.SetState(kStateLobbyHost);
  auto pump = [&] {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
    while (std::chrono::steady_clock::now() < deadline) {
      MSG message{};
      int quit = 0;
      app.PumpMessages(&message, &quit);
      app.m_context.GetLanGame().Poll(app);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  };
  pump();
  if (!minwin::present::CaptureScreenshot((directory / "host.bmp").string().c_str())) {
    throw std::runtime_error("Could not capture the Host lobby");
  }
  reb::compat::SendMessageA(app.m_activeWindow->m_hwnd, WM_LBUTTONUP, 0, 220 | (90 << 16));
  pump();
  if (!minwin::present::CaptureScreenshot((directory / "join.bmp").string().c_str())) {
    throw std::runtime_error("Could not capture the Join lobby");
  }
  app.m_context.GetLanGame().Close();
  app.DestroyAllWindows();
  app.DestroyNativeWindow();
  return 0;
}
