#include <cstdlib>
#include <span>
#include <vector>

#include "gfx/rle8_decoder.h"

namespace
{
void Require(bool condition)
{
  if (!condition)
    std::abort();
}

void VerifyCommandsAndOrientations()
{
  // Encoded run [7,7], EOL, absolute [1,2,3], delta (+1,+0), run [9], EOB.
  const std::vector<uint8_t> stream{2, 7, 0, 0, 0, 3, 1, 2, 3, 0, 0, 2, 1, 0, 1, 9, 0, 1};
  std::vector<uint8_t> pixels;
  Require(DecodeRle8(stream, 5, 2, false, pixels));
  Require((pixels == std::vector<uint8_t>{1, 2, 3, 0, 9, 7, 7, 0, 0, 0}));
  Require(DecodeRle8(stream, 5, 2, true, pixels));
  Require((pixels == std::vector<uint8_t>{7, 7, 0, 0, 0, 1, 2, 3, 0, 9}));
}

void VerifySkippedPixelsAndMalformedStreams()
{
  std::vector<uint8_t> pixels;
  const std::vector<uint8_t> stream{0, 2, 2, 1, 1, 5, 0, 1};
  Require(DecodeRle8(stream, 4, 2, false, pixels));
  Require((pixels == std::vector<uint8_t>{0, 0, 5, 0, 0, 0, 0, 0}));
  const std::vector<uint8_t> lastRowDelta{0, 2, 1, 1, 1, 6, 0, 1};
  Require(DecodeRle8(lastRowDelta, 4, 2, false, pixels));
  Require((pixels == std::vector<uint8_t>{0, 6, 0, 0, 0, 0, 0, 0}));

  for (const std::vector<uint8_t>& invalid : {
         std::vector<uint8_t>{1}, // truncated pair
         std::vector<uint8_t>{0, 2, 1}, // truncated delta
         std::vector<uint8_t>{0, 3, 1, 2}, // truncated odd absolute run/pad
         std::vector<uint8_t>{5, 1, 0, 1}, // run crosses row
         std::vector<uint8_t>{0, 2, 1, 2, 0, 1}, // delta outside image
         std::vector<uint8_t>{0, 0}, // missing bitmap end
       })
  {
    Require(!DecodeRle8(invalid, 4, 2, false, pixels));
    Require(pixels.empty());
  }
}
} // namespace

int main()
{
  VerifyCommandsAndOrientations();
  VerifySkippedPixelsAndMalformedStreams();
}
