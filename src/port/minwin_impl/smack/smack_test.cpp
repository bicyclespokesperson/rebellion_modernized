#include "smack_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../dsound/mixer.h"
#include "../gdi_api.h"
#include "../tests/mw_test.h"
#include "tests/smack_ops.h"
#include "tests/smack_table.h"

namespace
{

std::string InstallDir()
{
  const char* d = getenv("MW_INSTALL_DIR");
  return d ? d : "";
}

std::string Movie(const char* number)
{
  return InstallDir() + "/MDATA/MDATA." + number;
}

bool HaveGame()
{
  if (InstallDir().empty()) {
    printf("  (skipped: MW_INSTALL_DIR not set)\n");
    return false;
  }
  return true;
}

double g_now = 0;
double FakeClock()
{
  return g_now;
}

HDC GetTestDc(HWND)
{
  RECT all = {0, 0, 640, 480};
  minwin_gdi::WindowDcDesc d = {nullptr, 0, 0, &all, 1};
  return minwin_gdi::CreateWindowDc(d);
}
int ReleaseTestDc(HWND, HDC dc)
{
  minwin_gdi::DestroyWindowDc(dc);
  return 1;
}

std::vector<std::string> ReadLines(const char* path)
{
  std::vector<std::string> lines;
  FILE* f = fopen(path, "rb");
  if (!f) {
    return lines;
  }
  std::string line;
  int c;
  while ((c = fgetc(f)) != EOF) {
    if (c == '\n') {
      lines.push_back(line);
      line.clear();
    } else {
      line.push_back(static_cast<char>(c));
    }
  }
  fclose(f);
  return lines;
}

// A scenario line without the fields that depend on the host: the opaque value BufferNewPalette returned.
std::string Normalise(std::string line)
{
  size_t a = line.find(" newpalette=");
  if (a != std::string::npos) {
    size_t b = line.find(' ', a + 1);
    line.erase(a, b - a);
  }
  return line;
}

} // namespace

TEST(smack_struct_and_palette_timing)
{
  if (!HaveGame()) {
    return;
  }
  Smack* smk = SmackOpen(Movie("000").c_str(), 0xfe000, 0xffffffffu);
  CHECK(smk != nullptr);
  if (!smk) {
    return;
  }
  CHECK_EQ(smk->m_version, 0x324b4d53);
  CHECK_EQ(smk->m_width, 640);
  CHECK_EQ(smk->m_height, 480);
  CHECK_EQ(smk->m_frameCount, 259);
  CHECK_EQ(smk->m_frameNumber, 0);
  CHECK_EQ(smk->m_newPalette, 1); // the first frame brings the palette, available before the frame is decoded
  CHECK(smk->m_palette[3] != 0 || smk->m_palette[4] != 0 || smk->m_palette[5] != 0 || smk->m_palette[300] != 0);
  std::vector<uint8_t> buffer(640 * 480);
  SmackToBuffer(smk, 0, 0, 640, 480, buffer.data(), 1);
  SmackDoFrame(smk);
  CHECK_EQ(SmackToBufferRect(smk, 1), 1);
  CHECK_EQ(smk->m_lastRectX, 0);
  CHECK_EQ(smk->m_lastRectWidth, 640);
  CHECK_EQ(SmackToBufferRect(smk, 1), 0);
  SmackNextFrame(smk);
  CHECK_EQ(smk->m_frameNumber, 1);
  CHECK_EQ(smk->m_newPalette, 0);
  SmackGoto(smk, 1); // frame numbers passed to SmackGoto are 1-based
  CHECK_EQ(smk->m_frameNumber, 0);
  SmackGoto(smk, 4);
  CHECK_EQ(smk->m_frameNumber, 3);
  CHECK_EQ(smk->m_newPalette, 0);
  SmackClose(smk);
  CHECK(SmackOpen("/no/such/movie.smk", 0, 0) == nullptr);
}

TEST(smack_wait_follows_the_frame_rate)
{
  if (!HaveGame()) {
    return;
  }
  mwsmk::SetClock(FakeClock);
  g_now = 1000;
  Smack* smk = SmackOpen(Movie("000").c_str(), 0, 0xffffffffu);
  CHECK(smk != nullptr);
  if (!smk) {
    return;
  }
  CHECK_EQ(SmackWait(smk), 0); // the first frame is due at once
  std::vector<uint8_t> buffer(640 * 480);
  SmackToBuffer(smk, 0, 0, 640, 480, buffer.data(), 1);
  SmackDoFrame(smk);
  SmackNextFrame(smk);
  CHECK(SmackWait(smk) != 0);
  g_now += 60;
  CHECK(SmackWait(smk) != 0);
  g_now += 10; // 15 frames per second: 66.66 ms
  CHECK_EQ(SmackWait(smk), 0);
  SmackClose(smk);
  mwsmk::SetClock(nullptr);
}

TEST(decoding_matches_the_real_smackw32_dll_golden)
{
  if (!HaveGame()) {
    return;
  }
  SmackApiTable api;
  CHECK(LoadSmackApi(&api));
  std::vector<std::string> golden = ReadLines(SMACK_GOLDEN_PATH);
  CHECK(golden.size() > 100);
  std::string path = "mw_smack_golden_run.txt";
  FILE* out = fopen(path.c_str(), "wb");
  for (const char* number : {"000", "101", "202"}) {
    fprintf(out, "== %s\n", Movie(number).c_str());
    CHECK(RunSmackScenario(api, Movie(number).c_str(), 30, -1, nullptr, out));
  }
  fclose(out);
  std::vector<std::string> ours = ReadLines(path.c_str());
  std::vector<std::string> expected;
  for (const std::string& l : golden) {
    expected.push_back(Normalise(l));
  }
  std::vector<std::string> actual;
  for (const std::string& l : ours) {
    if (l.compare(0, 3, "== ") != 0) {
      actual.push_back(Normalise(l));
    }
  }
  CHECK_EQ(actual.size(), expected.size());
  int mismatches = 0;
  for (size_t i = 0; i < expected.size() && i < actual.size(); ++i) {
    if (expected[i] != actual[i]) {
      if (mismatches++ < 3) {
        printf("  line %zu\n    real: %s\n    ours: %s\n", i, expected[i].c_str(), actual[i].c_str());
      }
    }
  }
  CHECK_EQ(mismatches, 0);
}

TEST(audio_is_queued_on_the_mixer_and_volume_pan_apply)
{
  if (!HaveGame()) {
    return;
  }
  mwds::Mixer& mixer = mwds::Mixer::Global();
  mixer.SetOutputRate(11025);
  Smack* smk = SmackOpen(Movie("000").c_str(), 0xfe000, 0xffffffffu);
  CHECK(smk != nullptr);
  if (!smk) {
    return;
  }
  std::vector<uint8_t> buffer(640 * 480);
  SmackToBuffer(smk, 0, 0, 640, 480, buffer.data(), 1);
  SmackDoFrame(smk);
  std::vector<int16_t> out(2 * 4000);
  mixer.Render(out.data(), 4000);
  long loud = 0;
  for (int16_t v : out) {
    loud += v < 0 ? -v : v;
  }
  CHECK(loud > 0);
  SmackVolumePan(smk, 0xfe000, 0, 0x8000); // silent
  SmackNextFrame(smk);
  SmackDoFrame(smk);
  mixer.Render(out.data(), 200);
  long quiet = 0;
  for (int i = 0; i < 400; ++i) {
    quiet += out[i] < 0 ? -out[i] : out[i];
  }
  CHECK_EQ(quiet, 0);
  SmackClose(smk);
}

TEST(buffer_blit_and_palette_reach_the_screen)
{
  mwsmk::SetDeviceContextProvider(GetTestDc, ReleaseTestDc);
  memset(minwin_gdi::Screen().bits, 0, 640 * 480);
  SmackBuf* buf = SmackBufferOpen(reinterpret_cast<HWND>(static_cast<uintptr_t>(8)), 3, 32, 16, 32, 16);
  CHECK(buf != nullptr);
  if (!buf) {
    return;
  }
  CHECK_EQ(buf->m_reversed, 1);
  CHECK_EQ(buf->m_surfaceType, 1);
  CHECK_EQ(buf->m_width, 32);
  CHECK_EQ(buf->m_height, 16);
  uint8_t* px = static_cast<uint8_t*>(buf->m_buffer);
  // bottom-up: memory row 0 is the bottom row of the picture
  px[0 * 32 + 3] = 7;  // bottom row, x=3
  px[15 * 32 + 5] = 9; // top row, x=5
  uint32_t rgb[256], gen;
  minwin_gdi::CurrentPalette(rgb, &gen);
  uint8_t pal[768];
  for (int i = 0; i < 256; ++i) {
    pal[i * 3] = static_cast<uint8_t>(rgb[i] >> 16);
    pal[i * 3 + 1] = static_cast<uint8_t>(rgb[i] >> 8);
    pal[i * 3 + 2] = static_cast<uint8_t>(rgb[i]);
  }
  SmackBufferNewPalette(buf, pal, 0);
  RECT all = {0, 0, 640, 480};
  minwin_gdi::WindowDcDesc d = {nullptr, 0, 0, &all, 1};
  HDC dc = minwin_gdi::CreateWindowDc(d);
  SmackBufferBlit(buf, dc, 100, 50, 0, 0, 32, 16);
  const minwin_gdi::ScreenSurface s = minwin_gdi::Screen();
  CHECK_EQ(s.bits[(50 + 15) * s.pitch + 100 + 3], 7);
  CHECK_EQ(s.bits[(50 + 0) * s.pitch + 100 + 5], 9);
  CHECK_EQ(s.bits[(50 + 8) * s.pitch + 100 + 8], 0);
  // a sub-rectangle lands at the same place in the window
  memset(minwin_gdi::Screen().bits, 0, 640 * 480);
  SmackBufferBlit(buf, dc, 100, 50, 4, 0, 8, 16);
  CHECK_EQ(s.bits[(50 + 0) * s.pitch + 100 + 5], 9);
  CHECK_EQ(s.bits[(50 + 15) * s.pitch + 100 + 3], 0); // x=3 is outside the sub-rectangle
  minwin_gdi::DestroyWindowDc(dc);
  CHECK_EQ(SmackBufferSetPalette(buf), 0);
  SmackBufferClose(buf);
  mwsmk::SetDeviceContextProvider(nullptr, nullptr);
}

TEST(zoomed_buffers_replicate_pixels)
{
  SmackBuf* buf = SmackBufferOpen(nullptr, 3, 8, 4, 16, 8);
  CHECK(buf != nullptr);
  if (!buf) {
    return;
  }
  static_cast<uint8_t*>(buf->m_buffer)[3 * 8 + 2] = 5; // top row, x=2
  uint8_t pal[768] = {};
  uint32_t rgb[256], gen;
  minwin_gdi::CurrentPalette(rgb, &gen);
  for (int i = 0; i < 256; ++i) {
    pal[i * 3] = static_cast<uint8_t>(rgb[i] >> 16);
    pal[i * 3 + 1] = static_cast<uint8_t>(rgb[i] >> 8);
    pal[i * 3 + 2] = static_cast<uint8_t>(rgb[i]);
  }
  SmackBufferNewPalette(buf, pal, 0);
  memset(minwin_gdi::Screen().bits, 0, 640 * 480);
  RECT all = {0, 0, 640, 480};
  minwin_gdi::WindowDcDesc d = {nullptr, 0, 0, &all, 1};
  HDC dc = minwin_gdi::CreateWindowDc(d);
  SmackBufferBlit(buf, dc, 0, 0, 0, 0, 8, 4);
  const minwin_gdi::ScreenSurface s = minwin_gdi::Screen();
  for (int y = 0; y < 2; ++y) {
    for (int x = 4; x < 6; ++x) {
      CHECK_EQ(s.bits[y * s.pitch + x], 5);
    }
  }
  CHECK_EQ(s.bits[0 * s.pitch + 6], 0);
  CHECK_EQ(s.bits[2 * s.pitch + 4], 0);
  minwin_gdi::DestroyWindowDc(dc);
  SmackBufferClose(buf);
}
