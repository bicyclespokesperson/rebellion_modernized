// Start-up validation of the configuration and the game installation (first_run.cpp).
#pragma once

namespace reb::native {

// True when config.ini names a complete installation. Otherwise it has shown a message (SDL message box, and stderr) that says what to
// set and where, and the caller exits.
bool CheckInstallation();

}  // namespace reb::native
