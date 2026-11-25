// Start-up validation of the configuration and the game installation (first_run.cpp).
#pragma once

#include <string>

namespace reb::native
{

// True when config.ini names a complete installation (offering an interactive folder picker when running on a desktop
// and config.ini is missing or incomplete). Otherwise it has shown a message (SDL message box when interactive, and
// stderr) that says what to set and where, and the caller exits.
bool CheckInstallation();

// Reports a fatal startup error to stderr and (when running interactively) an SDL error dialog.
void ReportStartupError(const std::string& title, const std::string& message);

} // namespace reb::native
