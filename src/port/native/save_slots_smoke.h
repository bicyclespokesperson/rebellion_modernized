#pragma once

#include <string>

class RebellionApplication;

namespace reb::native {

// Opens the Saved Games screen over the loaded game and checks its slot buttons and confirmation dialogs: overwriting
// a save, loading over the running game and deleting a save. Writes and deletes files in the install's SaveGame
// directory, so run it against a scratch copy of the installation.
bool RunSaveSlotsSmoke(RebellionApplication& app, std::string& error);

} // namespace reb::native
