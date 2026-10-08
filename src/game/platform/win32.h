// The only header through which reconstructed code reaches Win32 / DirectX / Smacker declarations.
// Hybrid build: the real Windows SDK. Port: a native subset implemented on SDL (docs/platform_layer.md).
#pragma once
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <vfw.h>
#include <ddraw.h>
#include <dsound.h>

#include "platform/dplay.h"

// rpcndr.h defines `small` as char, which breaks parameters of that name.
#undef small
