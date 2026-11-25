// First-run checks of the native executable: the configuration file and the game installation it points to. A missing
// or wrong setting would otherwise show up as a blank window or an exit without a word; on a desktop we offer a native
// folder picker that writes config.ini, and everywhere we print clear manual instructions to stderr.
#include "port/native/first_run.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/res/path.h"
#include "port/minwin_impl/text/font_files.h"

namespace reb::native
{
namespace
{

const char* const kRequiredDlls[]
    = {"ALBRIEF.DLL",  "ALSPRITE.DLL", "CDDERR.DLL",  "COMMON.DLL",   "EMBRIEF.DLL",  "EMSPRITE.DLL", "ENCYBMAP.DLL",
       "ENCYTEXT.DLL", "GOKRES.DLL",   "REBDLOG.DLL", "STRATEGY.DLL", "TEXTCOMM.DLL", "TEXTSTRA.DLL", "TEXTTACT.DLL"};

bool IsDirectory(const std::string& path)
{
  std::error_code error;
  return std::filesystem::is_directory(path, error) && !error;
}

bool IsRegularFile(const std::string& path)
{
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) && !error;
}

std::string In(const std::string& dir, const char* name)
{
  return mw::res::ResolvePath(mw::res::JoinPath(dir, name));
}

bool IsHeadlessEnv()
{
  if (const char* sim = std::getenv("REB_SIM"); sim && *sim) {
    return true;
  }
  if (const char* driver = std::getenv("SDL_VIDEODRIVER"); driver && *driver) {
    const std::string_view value(driver);
    if (value == "dummy" || value == "offscreen") {
      return true;
    }
  }
  return false;
}

class ScopedSdlVideo
{
  public:
  ~ScopedSdlVideo()
  {
    if (initialized_) {
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
  }

  bool IsInteractive()
  {
    if (checked_) {
      return interactive_;
    }
    checked_ = true;
    if (IsHeadlessEnv()) {
      return false;
    }
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
      if (!SDL_Init(SDL_INIT_VIDEO)) {
        return false;
      }
      initialized_ = true;
    }
    const char* current = SDL_GetCurrentVideoDriver();
    if (!current) {
      return false;
    }
    const std::string_view driver(current);
    interactive_ = (driver != "dummy" && driver != "offscreen");
    return interactive_;
  }

  private:
  bool checked_ = false;
  bool initialized_ = false;
  bool interactive_ = false;
};

std::string ExampleInstallDir()
{
#if defined(_WIN32)
  return R"(C:\Program Files (x86)\Steam\steamapps\common\Star Wars - Rebellion)";
#else
  return "/path/to/Star Wars - Rebellion";
#endif
}

std::string ManualConfigHelp(const std::string& configPath)
{
  return "Configuration file:\n    " + configPath + "\n\nTo set it manually, put this line in that file:\n    install_dir="
         + ExampleInstallDir()
         + "\n\nOptional keys: data_dir (where MDATA lives, if not inside install_dir), font_path (an arial.ttf), "
           "window_scale (1-8; default: largest that fits), fullscreen (0 or 1; Alt+Enter toggles while playing).\n"
           "The REBELLION_CONFIG environment variable selects a different config file.";
}

std::vector<std::string> MissingFiles(const std::string& installDir, const std::string& dataDir)
{
  std::vector<std::string> missing;
  if (!IsRegularFile(In(installDir, "REBEXE.EXE"))) {
    missing.push_back("REBEXE.EXE (game resources and startup text are read from it)");
  }
  for (const char* dll : kRequiredDlls) {
    if (!IsRegularFile(In(installDir, dll))) {
      missing.push_back(dll);
    }
  }
  if (!IsDirectory(In(installDir, "GData"))) {
    missing.push_back("GData/ (folder)");
  }
  if (!IsDirectory(In(installDir, "EData"))) {
    missing.push_back("EData/ (folder)");
  }
  if (!IsRegularFile(In(In(dataDir, "MDATA"), "MDATA.000"))) {
    missing.push_back("MDATA/MDATA.000 (looked in " + dataDir + ")");
  }
  return missing;
}

bool EnsureSaveDirectory(const std::string& installDir)
{
  const std::string saves = In(installDir, "SaveGame");
  if (IsDirectory(saves)) {
    return true;
  }
  std::error_code error;
  return std::filesystem::create_directory(saves, error) && !error;
}

void ReportError(ScopedSdlVideo& video, const std::string& title, const std::string& message)
{
  fprintf(stderr, "%s\n\n%s\n", title.c_str(), message.c_str());
  if (video.IsInteractive()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), message.c_str(), nullptr);
  }
}

bool AskToChooseFolder(ScopedSdlVideo& video, SDL_MessageBoxFlags flags, const std::string& title,
                       const std::string& message, const char* chooseLabel)
{
  fprintf(stderr, "%s\n\n%s\n", title.c_str(), message.c_str());
  if (!video.IsInteractive()) {
    return false;
  }
  const SDL_MessageBoxButtonData buttons[2] = {
      {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, chooseLabel},
  };
  SDL_MessageBoxData data{};
  data.flags = flags;
  data.window = nullptr;
  data.title = title.c_str();
  data.message = message.c_str();
  data.numbuttons = 2;
  data.buttons = buttons;
  int buttonId = 0;
  if (!SDL_ShowMessageBox(&data, &buttonId)) {
    return false;
  }
  return buttonId == 1;
}

struct FolderPickState {
  std::mutex mutex;
  bool done = false;
  bool error = false;
  std::string selected;
  std::string errorMessage;
};

void SDLCALL OnFolderPicked(void* userdata, const char* const* filelist, int /*filter*/)
{
  auto* state = static_cast<FolderPickState*>(userdata);
  std::lock_guard<std::mutex> lock(state->mutex);
  if (!filelist) {
    state->error = true;
    const char* err = SDL_GetError();
    state->errorMessage = (err && *err) ? err : "folder picker failed";
  } else if (filelist[0] && filelist[0][0] != '\0') {
    state->selected = filelist[0];
  }
  state->done = true;
}

struct FolderPickResult {
  bool chosen = false;
  bool error = false;
  std::string path;
  std::string errorMessage;
};

FolderPickResult PickGameFolder(const std::string& defaultDir)
{
  FolderPickState state;
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, "Select Star Wars: Rebellion Game Folder");
  SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, "Select Folder");
  SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
  std::string location = defaultDir;
  if (!location.empty()) {
#if defined(_WIN32)
    if (location.back() != '\\' && location.back() != '/') {
      location.push_back('\\');
    }
#else
    if (location.back() != '/') {
      location.push_back('/');
    }
#endif
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.c_str());
  }
  SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, OnFolderPicked, &state, props);
  SDL_DestroyProperties(props);

  for (;;) {
    {
      std::lock_guard<std::mutex> lock(state.mutex);
      if (state.done) {
        FolderPickResult result;
        result.error = state.error;
        result.errorMessage = state.errorMessage;
        result.chosen = !state.error && !state.selected.empty();
        result.path = state.selected;
        return result;
      }
    }
    SDL_PumpEvents();
    SDL_Delay(10);
  }
}

} // namespace

void ReportStartupError(const std::string& title, const std::string& message)
{
  ScopedSdlVideo video;
  ReportError(video, title, message);
}

bool CheckInstallation()
{
  mw::config::Config& config = mw::config::Global();
  const std::string effectivePath = mw::config::GlobalPath();
  bool configPresent = mw::res::FileExists(effectivePath);
  ScopedSdlVideo video;

  for (;;) {
    const std::string installDir = config.InstallDir();
    SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION;
    const char* chooseLabel = "Choose Game Folder...";
    std::string title;
    std::string message;

    if (!configPresent) {
      title = "Star Wars: Rebellion — First-Time Setup";
      message = "Star Wars: Rebellion requires your own copy of the original 1998 game files (Steam, GOG, or CD).\n\n"
                "Click \"Choose Game Folder...\" to select the folder containing REBEXE.EXE, GData, EData, MDATA, "
                "and the game .DLL files. Your selection will be saved automatically.\n\n"
                + ManualConfigHelp(effectivePath);
    } else if (installDir.empty()) {
      title = "Star Wars: Rebellion — Game Folder Not Set";
      message = "The configuration file does not set install_dir yet.\n\n"
                "Click \"Choose Game Folder...\" to select the folder containing REBEXE.EXE, GData, EData, MDATA, "
                "and the game .DLL files.\n\n"
                + ManualConfigHelp(effectivePath);
    } else if (!IsDirectory(installDir)) {
      flags = SDL_MESSAGEBOX_WARNING;
      title = "Star Wars: Rebellion — Game Folder Not Found";
      message = "The configured game folder does not exist:\n    " + installDir
                + "\n\nClick \"Choose Game Folder...\" to select your Star Wars: Rebellion folder.\n\n"
                + ManualConfigHelp(effectivePath);
    } else {
      const std::vector<std::string> missing = MissingFiles(installDir, config.DataDir());
      if (!missing.empty()) {
        flags = SDL_MESSAGEBOX_WARNING;
        chooseLabel = "Choose Another Folder...";
        title = "Star Wars: Rebellion — Incomplete Game Folder";
        std::string list;
        for (const std::string& name : missing) {
          list += "    " + name + "\n";
        }
        message = "The game folder:\n    " + installDir + "\nis missing required files:\n\n" + list
                  + "\nSelect the folder of a complete Star Wars: Rebellion installation (Steam, GOG, or CD).\n"
                    "Original file names are required (upper or lower case does not matter).\n\n"
                  + ManualConfigHelp(effectivePath);
      } else if (!EnsureSaveDirectory(installDir)) {
        ReportError(video, "Star Wars: Rebellion — Cannot Write Saves",
                    "The SaveGame folder does not exist in:\n    " + installDir
                        + "\nand could not be created. Check the folder's permissions.");
        return false;
      } else {
        if (mwtext::FindArial().regular.empty()) {
          fprintf(stderr, "warning: no Arial or Liberation Sans font found; set font_path in %s\n",
                  effectivePath.c_str());
        }
        return true;
      }
    }

    if (!AskToChooseFolder(video, flags, title, message, chooseLabel)) {
      return false;
    }

    const std::string startDir = IsDirectory(installDir) ? installDir : std::string();
    const FolderPickResult pick = PickGameFolder(startDir);
    if (pick.error) {
      ReportError(video, "Star Wars: Rebellion — Configure Manually",
                  "Could not open the system folder picker (" + pick.errorMessage
                      + ").\n\nPlease set the game folder manually.\n\n" + ManualConfigHelp(effectivePath));
      return false;
    }
    if (!pick.chosen) {
      return false;
    }

    config.Set("install_dir", pick.path);
    if (!config.Get("data_dir").empty()
        && !IsRegularFile(In(In(config.Get("data_dir"), "MDATA"), "MDATA.000"))
        && IsRegularFile(In(In(pick.path, "MDATA"), "MDATA.000"))) {
      config.Erase("data_dir");
    }
    configPresent = true;

    if (MissingFiles(pick.path, config.DataDir()).empty() && EnsureSaveDirectory(pick.path)) {
      if (!config.Save(effectivePath)) {
        const std::string warn
            = "The game folder is valid and the game will start now, but config.ini could not be saved to:\n    "
              + effectivePath + "\n\nTo remember this folder, add:\n    install_dir=" + pick.path;
        fprintf(stderr, "warning: %s\n", warn.c_str());
        if (video.IsInteractive()) {
          SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Star Wars: Rebellion — Could Not Save Config",
                                   warn.c_str(), nullptr);
        }
      }
    }
  }
}

} // namespace reb::native
