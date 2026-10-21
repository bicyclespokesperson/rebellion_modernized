#pragma once

namespace minwin::present
{

constexpr int RenderScaleForOutput(int outputWidth, int outputHeight, int logicalWidth = 640, int logicalHeight = 480)
{
  if (outputWidth <= 0 || outputHeight <= 0 || logicalWidth <= 0 || logicalHeight <= 0) {
    return 1;
  }
  const int widthScale = (outputWidth + logicalWidth - 1) / logicalWidth;
  const int heightScale = (outputHeight + logicalHeight - 1) / logicalHeight;
  const int scale = widthScale < heightScale ? widthScale : heightScale;
  return scale < 1 ? 1 : (scale > 4 ? 4 : scale);
}

constexpr int GameplayWidthForOutput(int outputWidth, int outputHeight)
{
  if (outputWidth <= 0 || outputHeight <= 0) {
    return 640;
  }
  const long long width = 480LL * outputWidth / outputHeight;
  return width < 640 ? 640 : (width > 1280 ? 1280 : static_cast<int>(width));
}

} // namespace minwin::present
