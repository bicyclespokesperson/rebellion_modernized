// The host initializes the C runtime. Program startup, assets and platform services have explicit native lifetimes.
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>

#include <string>

#include "app/rebellion_application.h"
#include "platform/win32.h"
#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/dsound/mixer.h"
#include "port/native/first_run.h"
#include "port/native/game_context.h"
#include "port/native/space_battle/demo.h"
#include "port/native/startup_assets.h"
#include "port/native/thread_sync.h"

namespace minwin_wiring
{
void Install();
}

namespace
{

std::string CommandLineFrom(int argc, char** argv)
{
  std::string line;
  for (int i = 1; i < argc; ++i) {
    if (i > 1) {
      line += ' ';
    }
    line += argv[i];
  }
  return line;
}

} // namespace

int main(int argc, char** argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 4096);
  try {
    // The battle screen demo needs neither the installation nor the strategic game.
    if (const std::optional<int> demo = reb::native::space_battle::RunDemoIfRequested()) {
      return *demo;
    }
    if (!reb::native::CheckInstallation()) {
      return 1;
    }
    reb::native::StartupAssets startupAssets;
    startupAssets.LoadForInstallation(mw::config::Global().InstallDir());
    minwin_wiring::Install();
    // Buffer position notifications signal the audio threads' event groups (port/native/thread_sync.h).
    mwds::SetEventSignaler(reb::EventSet::SignalIfLive);
    reb::native::GameContext context;
    context.Start();
    std::string commandLine = CommandLineFrom(argc, argv);
    auto instance = reinterpret_cast<HINSTANCE>(reb::compat::GetModuleHandleA(nullptr));
    int result = RunApplication(context, instance, nullptr, commandLine.data(), 10 /* SW_SHOWDEFAULT */);
    context.Shutdown();
    return result;
  } catch (const std::exception& error) {
    reb::native::ReportStartupError(
        "Star Wars: Rebellion — Startup Failed",
        std::string("Native startup failed:\n    ") + error.what()
            + "\n\nMake sure install_dir points to an unmodified English copy of Star Wars: Rebellion "
              "(containing REBEXE.EXE from Steam, GOG, or CD).");
  } catch (...) {
    reb::native::ReportStartupError("Star Wars: Rebellion — Startup Failed",
                                    "Native startup failed with an unknown exception.");
  }
  return 1;
}
