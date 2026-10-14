// Where the text module finds the game's font ("Arial"). Search order, first hit wins:
//   1. the config file's font_path (the user's own arial.ttf); its bold face is looked up beside it (arialbd.ttf, or
//      <name>-Bold.ttf for a "-Regular" name), case-insensitively
//   2. a system Arial (arial.ttf + arialbd.ttf):
//        Windows  %WINDIR%\Fonts
//        macOS    /System/Library/Fonts/Supplemental, /Library/Fonts, ~/Library/Fonts
//        Linux    ~/.fonts, ~/.local/share/fonts, /usr/share/fonts, /usr/local/share/fonts (searched recursively, so
//                 .../truetype/msttcorefonts/Arial.ttf is found)
//   3. Liberation Sans (metric-compatible with Arial; LiberationSans-Regular.ttf + -Bold.ttf): the same system directories, then
//      <executable dir>/fonts and the repository's third_party/liberation-fonts-ttf-2.1.5 (tools/fetch_deps.sh)
// The environment variable MW_FONT_DIR adds one more directory (searched recursively) in front of the system ones.
// When no bold file exists next to the regular one the bold face is synthesised by emboldening the regular outlines.
#pragma once
#include <string>
#include <vector>

namespace mwtext
{

struct FontFiles
{
  std::string regular; // empty: no font found at all
  std::string bold; // empty: synthesise
  std::string source; // "config", "system Arial" or "Liberation Sans" (for the log)
};

// Extra directories searched (after the config path, before the system directories). Used by tests and by the port's startup
// code to add its own data directory.
void SetExtraFontDirectories(const std::vector<std::string>& dirs);

FontFiles FindArial();

// Recursive, case-insensitive search for a file name in a directory tree (depth-limited); empty when not found.
std::string FindFileNoCase(const std::string& directory, const std::string& fileName, int maxDepth = 5);

} // namespace mwtext
