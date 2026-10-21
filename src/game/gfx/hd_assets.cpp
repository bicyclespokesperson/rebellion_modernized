#include "gfx/hd_assets.h"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gfx/dib_bitmap.h"
#include "gfx/hd_asset_format.h"
#include "port/minwin_impl/config/config.h"

void LoadHighResolutionAsset(DibBitmap& bitmap, std::span<const uint8_t> originalDib)
{
  if (!bitmap.IsLoaded() || bitmap.BitCount() != 8) {
    return;
  }
  if (const char* disabled = std::getenv("REB_NO_HD"); disabled && *disabled && *disabled != '0') {
    return;
  }
  const char* defaultDirectory = std::getenv("REB_HD_ASSET_DIR");
  const auto configured = mw::config::Global().Get("hd_asset_dir", defaultDirectory ? defaultDirectory : "");
  if (configured.empty()) {
    return;
  }
  try {
    auto directory = std::filesystem::u8path(configured);
    if (directory.is_relative()) {
      directory = std::filesystem::u8path(mw::config::GlobalPath()).parent_path() / directory;
    }
    const uint64_t fingerprint = reb::hd::SourceFingerprint(originalDib);
    char filename[32];
    std::snprintf(filename, sizeof(filename), "%016llx.rhd", static_cast<unsigned long long>(fingerprint));
    const auto path = directory / filename;
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
      return;
    }
    const auto length = input.tellg();
    if (length < 32 || static_cast<uint64_t>(length) > reb::hd::kMaximumPixelBytes + 32) {
      return;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
      return;
    }
    reb::hd::IndexedAsset asset;
    const int height = bitmap.Height() < 0 ? -bitmap.Height() : bitmap.Height();
    if (!reb::hd::DecodeAsset(bytes, fingerprint, bitmap.Width(), height, bitmap.m_colorCount, asset)) {
      return;
    }
    if (!bitmap.SetHighResolutionPixels(asset.scale, asset.pixels, asset.keyedByDetailPixel)) {
      return;
    }
    if (std::getenv("REB_HD_TRACE")) {
      std::fprintf(stderr, "HD artwork: %s (%dx%d, %dx)\n", filename, bitmap.Width(), height, asset.scale);
    }
  } catch (const std::exception&) {
    // Optional artwork must never prevent the original game from loading.
  }
}
