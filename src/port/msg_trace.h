// REB_MSG_TRACE=<file>: a log of every message a window procedure of the game receives (src/port/minwin_impl/user
// dispatches through this logger), in a stable form so that two runs' logs can be diffed.
//
// One line per window-procedure entry:  "<depth> <class> #<window serial> <MESSAGE> <wParam> <lParam>"
// Window handles are replaced by serials in order of first appearance (also when they occur as wParam/lParam), pointers
// by "ptr".
#pragma once
#include <stdint.h>

namespace msgtrace
{

bool Enabled();
void Log(const char* windowClass, uintptr_t window, unsigned message, uintptr_t wParam, intptr_t lParam, int depth);
void Note(const char* text);

} // namespace msgtrace
