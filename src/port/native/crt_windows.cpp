// Native Windows helpers layered over the host MSVC-compatible CRT.
#include <stdio.h>
#include <time.h>

#include "core/runtime_environment.h"
#include "port/minwin_impl/res/path.h"
#include "port/native/native_clock.h"

int64_t GameSeedSeconds()
{
  return native_clock::Deterministic() ? 946728000 : static_cast<int64_t>(time(nullptr));
}

FILE* OpenGameFile(const char* name, const char* mode)
{
  return fopen(mw::res::ResolvePath(name).c_str(), mode);
}
