#pragma once

#if defined(REB_TEST_REAL_WIN32)
#include <windows.h>

namespace reb::compat
{
using ::BitBlt;
using ::CreateCompatibleDC;
using ::CreateDIBSection;
using ::CreateFontIndirectA;
using ::CreatePen;
using ::CreateSolidBrush;
using ::DeleteDC;
using ::DeleteObject;
using ::DrawTextA;
using ::FillRect;
using ::GetStockObject;
using ::GetTextExtentPoint32A;
using ::GetTextMetricsA;
using ::LineTo;
using ::MoveToEx;
using ::PatBlt;
using ::Polyline;
using ::Rectangle;
using ::SelectObject;
using ::SetBkColor;
using ::SetBkMode;
using ::SetDIBitsToDevice;
using ::SetROP2;
using ::SetTextColor;
using ::TextOutA;
}
#else
#include "port/minwin/gdi32.h"
#include "port/minwin/user32.h"
#endif
