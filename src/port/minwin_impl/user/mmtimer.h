#pragma once
#include "port/minwin/winmm.h"

namespace minwin
{
DWORD RealTimeMs();
UINT SetMultimediaTimer(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags);
}
