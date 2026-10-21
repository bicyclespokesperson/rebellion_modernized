#pragma once

#include <filesystem>

class RebellionApplication;

void RunNativeUiSmoke(RebellionApplication& app);
void RunNativeSwitchSidesSmoke(RebellionApplication& app);
// Presents the current screen and writes it to `path` (a review image of what the windows drew).
void SaveUiReviewFrame(const std::filesystem::path& path);
