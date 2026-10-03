// Hybrid runtime: lives inside the proxy SMACKW32.DLL. It deliberately links no C runtime of its own;
// reconstructed code uses the original executable's statically linked CRT (resolved at link time).
#pragma once

namespace reb {
void log(const char* fmt, ...);
int install_hooks();
}  // namespace reb
