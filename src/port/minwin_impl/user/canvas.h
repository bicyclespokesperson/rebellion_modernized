#pragma once

#include <string>

namespace minwin
{

// Switches between the classic 640x480 canvas and a gameplay canvas fitted to
// the current drawable aspect. The active dimensions are in minwin_gdi::Screen().
void SetGameplayCanvasEnabled(bool enabled);
bool GameplayCanvasEnabled();

bool SetClipboardText(const std::string& text);
std::string GetClipboardText();

} // namespace minwin
