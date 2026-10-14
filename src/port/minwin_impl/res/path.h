// Path translation for the game's Windows-style file names: backslashes, drive letters, case-insensitive names.
// Every file-opening entry point of the port (_lopen, FindFirstFileA, DeleteFileA, LoadLibraryA, the CRT replacement) goes
// through ResolvePath().
#pragma once
#include <string>

namespace mw::res
{

// "C:\Games\Rebellion\GData\FOO.DAT" -> a native path. On POSIX: '\' becomes '/', a leading drive letter is dropped, and each
// component that does not exist as written is matched case-insensitively against its directory. Components that do not exist
// are kept as written (so the result can still be used to create a file). On Windows the name is returned unchanged.
std::string ResolvePath(const std::string& windowsPath);

bool FileExists(const std::string& nativePath);

// Joins two path pieces with a single separator.
std::string JoinPath(const std::string& dir, const std::string& name);

} // namespace mw::res
