// First-run checks of the native executable: the configuration file and the game installation it points to. A missing or wrong
// setting would otherwise show up as a blank window or an exit without a word; here it is a message box (and stderr) that says
// what to put where.
#include "port/native/first_run.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/res/path.h"
#include "port/minwin_impl/text/font_files.h"

namespace reb::native
{
namespace
{

const char* const kRequiredDlls[] = {"ALBRIEF.DLL",  "ALSPRITE.DLL", "CDDERR.DLL",   "COMMON.DLL",  "EMBRIEF.DLL",
                                     "EMSPRITE.DLL", "ENCYBMAP.DLL", "ENCYTEXT.DLL", "GOKRES.DLL",  "REBDLOG.DLL",
                                     "STRATEGY.DLL", "TEXTCOMM.DLL", "TEXTSTRA.DLL", "TEXTTACT.DLL"};

bool IsDirectory(const std::string& path)
{
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool IsRegularFile(const std::string& path)
{
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string In(const std::string& dir, const char* name)
{
  return mw::res::ResolvePath(mw::res::JoinPath(dir, name));
}

std::string ConfigHelp(const std::string& configPath)
{
  return "The configuration file is:\n    " + configPath +
         "\n\nIt is a plain text file with one key=value per line. The one required line names the folder of your own "
         "copy of Star Wars: "
         "Rebellion (the folder that contains REBEXE.EXE, GData, MDATA and EData):\n\n    install_dir=/path/to/Star "
         "Wars - Rebellion\n\n"
         "Optional lines: data_dir (where MDATA lives, if it is not in install_dir), font_path (an arial.ttf), "
         "window_scale (1-8; "
         "default: the largest that fits the screen), fullscreen (0 or 1; Alt+Enter toggles while playing).\n"
         "The REBELLION_CONFIG environment variable selects a different file.";
}

std::vector<std::string> MissingFiles(const std::string& installDir, const std::string& dataDir)
{
  std::vector<std::string> missing;
  if (!IsRegularFile(In(installDir, "REBEXE.EXE")))
    missing.push_back("REBEXE.EXE (the game's resources are read from it)");
  for (const char* dll : kRequiredDlls)
    if (!IsRegularFile(In(installDir, dll)))
      missing.push_back(dll);
  if (!IsDirectory(In(installDir, "GData")))
    missing.push_back("GData\\ (folder)");
  if (!IsDirectory(In(installDir, "EData")))
    missing.push_back("EData\\ (folder)");
  if (!IsRegularFile(In(In(dataDir, "MDATA"), "MDATA.000")))
    missing.push_back("MDATA\\MDATA.000 (looked in " + dataDir + ")");
  return missing;
}

bool EnsureSaveDirectory(const std::string& installDir)
{
  const std::string saves = In(installDir, "SaveGame");
  return IsDirectory(saves) || mkdir(saves.c_str(), 0777) == 0;
}

void Report(const std::string& title, const std::string& message)
{
  fprintf(stderr, "%s\n\n%s\n", title.c_str(), message.c_str());
  if (SDL_Init(SDL_INIT_VIDEO))
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), message.c_str(), nullptr);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
}

} // namespace

bool CheckInstallation()
{
  const std::string configPath = mw::config::Config::DefaultPath();
  const std::string title = "Star Wars: Rebellion";
  mw::config::Config& config = mw::config::Global();
  const std::string effectivePath = mw::config::GlobalPath();
  if (!mw::res::FileExists(effectivePath))
  {
    Report(title + ": no configuration file", "No configuration file was found.\n\n" + ConfigHelp(configPath));
    return false;
  }
  const std::string installDir = config.InstallDir();
  if (installDir.empty())
  {
    Report(title + ": install_dir is not set",
           "The configuration file does not set install_dir.\n\n" + ConfigHelp(effectivePath));
    return false;
  }
  if (!IsDirectory(installDir))
  {
    Report(title + ": install_dir is not a folder", "install_dir points to a folder that does not exist:\n    " +
                                                      installDir + "\n\n" + ConfigHelp(effectivePath));
    return false;
  }
  const std::vector<std::string> missing = MissingFiles(installDir, config.DataDir());
  if (!missing.empty())
  {
    std::string list;
    for (const std::string& name : missing)
      list += "    " + name + "\n";
    Report(title + ": incomplete installation",
           "The game folder\n    " + installDir + "\nis missing:\n\n" + list +
             "\nIt must be a complete copy of the game as installed from the CD or GOG/Steam, "
             "with the original file names (upper or lower case does not matter).\n\ninstall_dir (and data_dir for "
             "MDATA) are set in:\n    " +
             effectivePath);
    return false;
  }
  if (!EnsureSaveDirectory(installDir))
  {
    Report(title + ": cannot write saves", "The SaveGame folder does not exist in\n    " + installDir +
                                             "\nand could not be created. Check the folder's permissions.");
    return false;
  }
  if (mwtext::FindArial().regular.empty())
    fprintf(stderr, "warning: no Arial or Liberation Sans font found; set font_path in %s\n", effectivePath.c_str());
  return true;
}

} // namespace reb::native
