#include "port/native/space_battle/sprite_pack.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

using namespace reb::battle_sprites;

namespace
{
void Require(bool condition, const char* what)
{
  if (!condition) {
    std::fprintf(stderr, "sprite pack test failed: %s\n", what);
    std::exit(1);
  }
}

// A straightforward QOI encoder (the specification's reference algorithm), independent of the decoder under test.
std::vector<uint8_t> EncodeQoi(const RgbaImage& image)
{
  std::vector<uint8_t> out = {'q', 'o', 'i', 'f'};
  for (const uint32_t value : {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)}) {
    for (int shift = 24; shift >= 0; shift -= 8) {
      out.push_back(static_cast<uint8_t>(value >> shift));
    }
  }
  out.push_back(4);
  out.push_back(0);
  std::array<std::array<uint8_t, 4>, 64> index{};
  std::array<uint8_t, 4> previous = {0, 0, 0, 255};
  int run = 0;
  const size_t count = image.pixels.size() / 4;
  for (size_t i = 0; i < count; ++i) {
    const std::array<uint8_t, 4> pixel = {image.pixels[i * 4], image.pixels[i * 4 + 1], image.pixels[i * 4 + 2],
                                          image.pixels[i * 4 + 3]};
    if (pixel == previous) {
      ++run;
      if (run == 62 || i + 1 == count) {
        out.push_back(static_cast<uint8_t>(0xC0 | (run - 1)));
        run = 0;
      }
      continue;
    }
    if (run > 0) {
      out.push_back(static_cast<uint8_t>(0xC0 | (run - 1)));
      run = 0;
    }
    const int slot = (pixel[0] * 3 + pixel[1] * 5 + pixel[2] * 7 + pixel[3] * 11) % 64;
    if (index[slot] == pixel) {
      out.push_back(static_cast<uint8_t>(slot));
    } else {
      index[slot] = pixel;
      const auto delta = [&](int channel) { return static_cast<int8_t>(pixel[channel] - previous[channel]); };
      const int dr = delta(0), dg = delta(1), db = delta(2);
      if (pixel[3] != previous[3]) {
        out.insert(out.end(), {0xFF, pixel[0], pixel[1], pixel[2], pixel[3]});
      } else if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
        out.push_back(static_cast<uint8_t>(0x40 | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2)));
      } else if (dg >= -32 && dg <= 31 && dr - dg >= -8 && dr - dg <= 7 && db - dg >= -8 && db - dg <= 7) {
        out.push_back(static_cast<uint8_t>(0x80 | (dg + 32)));
        out.push_back(static_cast<uint8_t>((dr - dg + 8) << 4 | (db - dg + 8)));
      } else {
        out.insert(out.end(), {0xFE, pixel[0], pixel[1], pixel[2]});
      }
    }
    previous = pixel;
  }
  out.insert(out.end(), {0, 0, 0, 0, 0, 0, 0, 1});
  return out;
}

RgbaImage PatternImage(int width, int height)
{
  RgbaImage image{width, height, std::vector<uint8_t>(static_cast<size_t>(width) * height * 4)};
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint8_t* pixel = &image.pixels[(static_cast<size_t>(y) * width + x) * 4];
      const bool transparent = x < 3 || (x + y) % 11 == 0;
      pixel[0] = static_cast<uint8_t>(transparent ? 0 : x * 7 + y);
      pixel[1] = static_cast<uint8_t>(transparent ? 0 : 128 + x - y);
      pixel[2] = static_cast<uint8_t>(transparent ? 0 : (x * y) % 251);
      pixel[3] = static_cast<uint8_t>(transparent ? 0 : (y % 3 == 0 ? 255 : 90 + x));
    }
  }
  return image;
}

void TestQoiRoundTrip()
{
  const RgbaImage original = PatternImage(37, 23);
  RgbaImage decoded;
  Require(DecodeQoi(EncodeQoi(original), decoded), "decode the reference encoding");
  Require(decoded.width == 37 && decoded.height == 23 && decoded.pixels == original.pixels, "round trip pixels");
  auto truncated = EncodeQoi(original);
  truncated.resize(truncated.size() / 2);
  Require(!DecodeQoi(truncated, decoded) && decoded.pixels.empty(), "truncated input fails");
  auto badMagic = EncodeQoi(original);
  badMagic[0] = 'x';
  Require(!DecodeQoi(badMagic, decoded), "bad magic fails");
  auto oversized = EncodeQoi(original);
  oversized[6] = 0x40; // 16384 pixels wide: more than its few hundred bytes can encode
  oversized[7] = 0x00;
  Require(!DecodeQoi(oversized, decoded), "dimensions the data cannot cover fail");
}

void TestPythonEncoderFixture()
{
  // tools/build_tactical_sprites.py qoi_encode() output for a 6x4 image using every QOI op.
  const std::vector<uint8_t> encoded = {
      0x71, 0x6f, 0x69, 0x66, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x00, 0xc1,
      0xff, 0x0a, 0x14, 0x1e, 0xff, 0x76, 0xb4, 0x83, 0xfe, 0xc8, 0x0a, 0x5a, 0xff, 0xc8, 0x0a, 0x5a,
      0x80, 0x09, 0xc0, 0x5f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa2, 0x76,
      0x07, 0x10, 0xff, 0x28, 0x32, 0x3c, 0x46, 0xc2, 0xff, 0xc8, 0x0a, 0x5a, 0x80, 0xff, 0x01, 0x02,
      0x03, 0x04, 0xc0, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
  const std::vector<uint8_t> expected = {
      0,   0,  0,   0,   0,   0,  0,  0,   0,  0,  0,  0,   10, 20, 30, 255, 11, 19, 30, 255, 31,  39, 45, 255,
      200, 10, 90,  255, 200, 10, 90, 128, 10, 20, 30, 255, 10, 20, 30, 255, 9,  21, 31, 255, 0,   0,  0,  0,
      255, 255, 255, 255, 0, 1, 255, 255, 11, 19, 30, 255, 31, 39, 45, 255, 40, 50, 60, 70,  40,  50, 60, 70,
      40,  50, 60,  70,  40, 50, 60, 70,  200, 10, 90, 128, 1,  2,  3,  4,   1,  2,  3,  4,   0,   0,  0,  0};
  RgbaImage decoded;
  Require(DecodeQoi(encoded, decoded), "decode the Python fixture");
  Require(decoded.width == 6 && decoded.height == 4 && decoded.pixels == expected, "Python fixture pixels");
}

void TestHeadingLookup()
{
  constexpr float kPi = std::numbers::pi_v<float>;
  Require(NearestHeadingIndex(0.0f, 64) == 0, "heading 0");
  Require(NearestHeadingIndex(kPi / 2, 64) == 16, "heading pi/2");
  Require(NearestHeadingIndex(-kPi / 2, 64) == 48, "heading -pi/2");
  Require(NearestHeadingIndex(2 * kPi - 0.01f, 64) == 0, "heading wraps");
  Require(NearestHeadingIndex(7 * kPi, 16) == 8, "heading beyond one turn");
  Require(NearestHeadingIndex(1.0f, 1) == 0, "single frame");
  Require(NearestHeadingIndex(std::nanf(""), 64) == 0, "non-finite heading");
}

void WriteFile(const std::filesystem::path& path, const std::string& text)
{
  std::ofstream(path, std::ios::binary) << text;
}

void WriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
  std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),
                                              static_cast<std::streamsize>(bytes.size()));
}

std::string Manifest(int frameX)
{
  std::string text = "rebellion-tactical-sprites 1\n"
                     "# a synthetic pack\n"
                     "setting pitch_degrees 45\n"
                     "source TACTICAL.DLL 0123\n"
                     "atlas 0 20_isd_large_0.qoi 37 23\n"
                     "atlas 1 20_isd_small_0.qoi 8 8\n"
                     "class 20 imperial_star_destroyer original 1602.4 Imperial Star Destroyer\n"
                     "view 20 top large 1 8\n"
                     "frame 20 top large 0 0 3 0 10 6 5 3\n"
                     "view 20 angled large 4 8\n";
  for (int heading = 0; heading < 4; ++heading) {
    text += "frame 20 angled large " + std::to_string(heading) + " 0 " + std::to_string(frameX + heading * 8)
            + " 7 8 9 4.5 4.25\n";
  }
  text += "view 20 top small 1 32\n"
          "frame 20 top small 0 1 0 0 3 2 1.5 1\n"
          "view 20 angled small 1 32\n"
          "frame 20 angled small 0 1 4 4 4 4 2 2\n";
  return text;
}

void TestPackLoad()
{
  const auto directory = std::filesystem::temp_directory_path()
                         / ("reb_sprite_pack_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  std::string error;
  Require(!SpritePack::Load(directory, &error) && !error.empty(), "missing manifest is reported");

  const RgbaImage large = PatternImage(37, 23);
  WriteFile(directory / "20_isd_large_0.qoi", EncodeQoi(large));
  WriteFile(directory / "manifest.txt", Manifest(3));
  auto pack = SpritePack::Load(directory, &error);
  Require(pack.has_value(), error.c_str());
  Require(pack->PitchDegrees() == 45.0f && pack->Classes().size() == 1, "settings and classes");
  const SpriteClass* isd = pack->Find(20);
  Require(isd && isd->name == "Imperial Star Destroyer" && isd->origin == "original" && isd->lengthMetres > 1602.0f,
          "class record");
  Require(!pack->Find(21) && !pack->Frame(21, SpriteView::Top, SpriteSize::Large, 0.0f), "absent class");
  Require(isd->Set(SpriteView::Angled, SpriteSize::Large).metresPerPixel == 8.0f, "metres per pixel");
  const SpriteFrame* north = pack->Frame(20, SpriteView::Angled, SpriteSize::Large, std::numbers::pi_v<float> / 2);
  Require(north && north->x == 11 && north->y == 7 && north->width == 8 && north->pivotX == 4.5f, "heading frame");
  const SpriteFrame* top = pack->Frame(20, SpriteView::Top, SpriteSize::Large, 2.0f);
  Require(top && top->x == 3 && top->width == 10, "top frame ignores heading");
  Require((pack->AtlasesFor(20, SpriteSize::Large) == std::vector<int>{0}), "large atlases");
  Require((pack->AtlasesFor(20, SpriteSize::Small) == std::vector<int>{1}), "small atlases");
  const RgbaImage* atlas = pack->Atlas(0);
  Require(atlas && atlas->width == 37 && atlas->pixels == large.pixels, "atlas pixels");
  Require(pack->Atlas(0) == atlas, "atlas cached");
  Require(!pack->Atlas(1), "missing atlas file");
  WriteFile(directory / "20_isd_small_0.qoi", EncodeQoi(PatternImage(9, 8)));
  Require(!pack->Atlas(1), "a failed atlas stays failed until released");
  pack->ReleaseAtlases();
  Require(!pack->Atlas(1), "atlas with the wrong size");
  WriteFile(directory / "20_isd_small_0.qoi", EncodeQoi(PatternImage(8, 8)));
  pack->ReleaseAtlases();
  Require(pack->Atlas(0) && pack->Atlas(1), "atlases reload");

  WriteFile(directory / "manifest.txt", Manifest(20));
  Require(!SpritePack::Load(directory, &error) && error.find("outside") != std::string::npos, "frame bounds");
  auto missingFrame = Manifest(3);
  missingFrame.erase(missingFrame.find("frame 20 angled large 2"), missingFrame.find("frame 20 angled large 3")
                                                                      - missingFrame.find("frame 20 angled large 2"));
  WriteFile(directory / "manifest.txt", missingFrame);
  Require(!SpritePack::Load(directory, &error) && error.find("missing frames") != std::string::npos, "missing frame");
  const auto rejects = [&](std::string from, const std::string& to, const char* what) {
    auto text = Manifest(3);
    text.replace(text.find(from), from.size(), to);
    WriteFile(directory / "manifest.txt", text);
    Require(!SpritePack::Load(directory, &error), what);
  };
  rejects("frame 20 top large 0 0 3", "frame 20 top large 0 0 2147483647", "frame offset overflow");
  rejects("view 20 top large 1 8", "view 20 top large 2 8", "a top view has one frame");
  rejects("pitch_degrees 45", "pitch_degrees 0", "pitch range");
  rejects("20_isd_small_0.qoi", "../20_isd_small_0.qoi", "atlas outside the pack");
  WriteFile(directory / "manifest.txt", std::string("rebellion-tactical-sprites 2\n"));
  Require(!SpritePack::Load(directory, &error), "unknown version");
  std::filesystem::remove_all(directory);
}
} // namespace

int main()
{
  TestQoiRoundTrip();
  TestPythonEncoderFixture();
  TestHeadingLookup();
  TestPackLoad();
  return 0;
}
