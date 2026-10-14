#include <cstdlib>

#include "port/minwin_impl/user/presentation_math.h"

namespace
{

void Require(bool condition)
{
  if (!condition)
    std::abort();
}

} // namespace

int main()
{
  using minwin::present::RenderScaleForOutput;
  using minwin::present::GameplayWidthForOutput;
  Require(RenderScaleForOutput(640, 480) == 1);
  Require(RenderScaleForOutput(1280, 960) == 2);
  Require(RenderScaleForOutput(1365, 768) == 2);
  Require(RenderScaleForOutput(1600, 1200) == 3);
  Require(RenderScaleForOutput(7680, 4320) == 4);
  Require(RenderScaleForOutput(0, 480) == 1);
  Require(GameplayWidthForOutput(640, 480) == 640);
  Require(GameplayWidthForOutput(1920, 1080) == 853);
  Require(GameplayWidthForOutput(5120, 1440) == 1280);
  Require(GameplayWidthForOutput(0, 1080) == 640);
}
