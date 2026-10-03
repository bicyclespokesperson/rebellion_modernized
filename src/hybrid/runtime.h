// Hybrid runtime: lives inside the proxy SMACKW32.DLL. It deliberately links no C runtime of its own;
// reconstructed code uses the original executable's statically linked CRT (resolved at link time).
#pragma once
#include <cstdint>

namespace reb {
void log(const char* fmt, ...);
// Arms REB_CAPTURE breakpoints; must run before install_hooks so it sees pristine original bytes.
void install_captures();
bool is_capture_target(uint32_t address);
int install_hooks();
}  // namespace reb
