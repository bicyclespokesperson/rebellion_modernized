#include "port/native/space_battle/sprite_pack.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <locale>
#include <numbers>
#include <sstream>
#include <utility>

namespace reb::battle_sprites
{
namespace
{
constexpr int kMaximumImageSide = 16384;
constexpr std::streamoff kMaximumFileBytes = 256LL << 20;
constexpr int kQoiMaximumRun = 62;
constexpr std::string_view kManifestMagic = "rebellion-tactical-sprites";
constexpr int kManifestVersion = 1;

uint32_t ReadBigEndian(std::span<const uint8_t> bytes, size_t offset)
{
  return (uint32_t{bytes[offset]} << 24) | (uint32_t{bytes[offset + 1]} << 16) | (uint32_t{bytes[offset + 2]} << 8)
         | uint32_t{bytes[offset + 3]};
}

std::optional<std::vector<uint8_t>> ReadFile(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return std::nullopt;
  }
  const std::streamoff size = input.tellg();
  if (size < 0 || size > kMaximumFileBytes) {
    return std::nullopt;
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  input.seekg(0);
  if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
    return std::nullopt;
  }
  return bytes;
}

std::optional<SpriteView> ParseView(const std::string& text)
{
  if (text == "top") {
    return SpriteView::Top;
  }
  if (text == "angled") {
    return SpriteView::Angled;
  }
  return std::nullopt;
}

std::optional<SpriteSize> ParseSize(const std::string& text)
{
  if (text == "large") {
    return SpriteSize::Large;
  }
  if (text == "small") {
    return SpriteSize::Small;
  }
  return std::nullopt;
}

bool IsPlainQoiName(const std::string& name)
{
  return name.size() > 4 && name.ends_with(".qoi") && name.front() != '.'
         && name.find_first_of("/\\:") == std::string::npos;
}

struct ManifestError {
  std::string message;
};

class ManifestParser
{
  public:
  explicit ManifestParser(std::vector<SpriteClass>& classes, std::vector<AtlasFile>& atlases, float& pitch)
      : m_classes(classes), m_atlases(atlases), m_pitch(pitch)
  {
  }

  void Line(const std::string& line)
  {
    std::istringstream fields(line);
    fields.imbue(std::locale::classic());
    std::string kind;
    if (!(fields >> kind) || kind.starts_with('#')) {
      return;
    }
    if (kind == "setting") {
      Setting(fields);
    } else if (kind == "atlas") {
      Atlas(fields);
    } else if (kind == "class") {
      Class(fields);
    } else if (kind == "view") {
      View(fields);
    } else if (kind == "frame") {
      Frame(fields);
    }
  }

  void Finish() const
  {
    for (const SpriteClass& spriteClass : m_classes) {
      for (const auto& views : spriteClass.sets) {
        for (const SpriteSet& set : views) {
          for (const SpriteFrame& frame : set.frames) {
            if (frame.width <= 0) {
              Fail("class " + std::to_string(spriteClass.typeIndex) + " is missing frames");
            }
          }
        }
      }
    }
  }

  private:
  [[noreturn]] static void Fail(const std::string& message)
  {
    throw ManifestError{message};
  }

  template <typename T>
  static T Read(std::istringstream& fields, const char* what)
  {
    T value{};
    if (!(fields >> value)) {
      Fail(std::string("bad or missing ") + what);
    }
    return value;
  }

  void Setting(std::istringstream& fields)
  {
    const auto key = Read<std::string>(fields, "setting key");
    if (key == "pitch_degrees") {
      m_pitch = Read<float>(fields, "pitch");
      if (!(m_pitch > 0.0f && m_pitch <= 90.0f)) {
        Fail("pitch out of range");
      }
    }
  }

  void Atlas(std::istringstream& fields)
  {
    const auto id = Read<int>(fields, "atlas id");
    AtlasFile atlas{Read<std::string>(fields, "atlas file"), Read<int>(fields, "atlas width"),
                    Read<int>(fields, "atlas height")};
    if (id != static_cast<int>(m_atlases.size())) {
      Fail("atlas ids must be consecutive from 0");
    }
    if (atlas.width <= 0 || atlas.height <= 0 || atlas.width > kMaximumImageSide || atlas.height > kMaximumImageSide
        || !IsPlainQoiName(atlas.file)) {
      Fail("bad atlas " + atlas.file);
    }
    m_atlases.push_back(std::move(atlas));
  }

  void Class(std::istringstream& fields)
  {
    SpriteClass spriteClass;
    spriteClass.typeIndex = Read<int>(fields, "class type");
    spriteClass.slug = Read<std::string>(fields, "class slug");
    spriteClass.origin = Read<std::string>(fields, "class origin");
    spriteClass.lengthMetres = Read<float>(fields, "class length");
    std::getline(fields >> std::ws, spriteClass.name);
    if (FindClass(spriteClass.typeIndex)) {
      Fail("duplicate class " + std::to_string(spriteClass.typeIndex));
    }
    m_classes.push_back(std::move(spriteClass));
  }

  void View(std::istringstream& fields)
  {
    SpriteClass& spriteClass = ClassFor(Read<int>(fields, "view type"));
    const auto view = ReadView(spriteClass, fields);
    SpriteSet& set = spriteClass.sets[static_cast<size_t>(view.first)][static_cast<size_t>(view.second)];
    const auto headings = Read<int>(fields, "heading count");
    set.metresPerPixel = Read<float>(fields, "metres per pixel");
    const bool rotatedTop = view.first == SpriteView::Top && headings != 1;
    if (headings <= 0 || headings > 1024 || rotatedTop || !(set.metresPerPixel > 0.0f) || !set.frames.empty()) {
      Fail("bad view for class " + std::to_string(spriteClass.typeIndex));
    }
    set.frames.resize(static_cast<size_t>(headings));
  }

  void Frame(std::istringstream& fields)
  {
    SpriteClass& spriteClass = ClassFor(Read<int>(fields, "frame type"));
    const auto view = ReadView(spriteClass, fields);
    SpriteSet& set = spriteClass.sets[static_cast<size_t>(view.first)][static_cast<size_t>(view.second)];
    const auto heading = Read<int>(fields, "frame heading");
    SpriteFrame frame;
    frame.atlas = Read<int>(fields, "frame atlas");
    frame.x = Read<int>(fields, "frame x");
    frame.y = Read<int>(fields, "frame y");
    frame.width = Read<int>(fields, "frame width");
    frame.height = Read<int>(fields, "frame height");
    frame.pivotX = Read<float>(fields, "pivot x");
    frame.pivotY = Read<float>(fields, "pivot y");
    if (heading < 0 || heading >= static_cast<int>(set.frames.size()) || set.frames[heading].width != 0) {
      Fail("bad frame heading for class " + std::to_string(spriteClass.typeIndex));
    }
    if (frame.atlas < 0 || frame.atlas >= static_cast<int>(m_atlases.size()) || frame.x < 0 || frame.y < 0
        || frame.width <= 0 || frame.height <= 0 || frame.width > m_atlases[frame.atlas].width - frame.x
        || frame.height > m_atlases[frame.atlas].height - frame.y) {
      Fail("frame outside its atlas for class " + std::to_string(spriteClass.typeIndex));
    }
    set.frames[heading] = frame;
  }

  SpriteClass* FindClass(int typeIndex)
  {
    const auto found = std::ranges::find(m_classes, typeIndex, &SpriteClass::typeIndex);
    return found == m_classes.end() ? nullptr : &*found;
  }

  SpriteClass& ClassFor(int typeIndex)
  {
    SpriteClass* spriteClass = FindClass(typeIndex);
    if (!spriteClass) {
      Fail("undeclared class " + std::to_string(typeIndex));
    }
    return *spriteClass;
  }

  static std::pair<SpriteView, SpriteSize> ReadView(const SpriteClass& spriteClass, std::istringstream& fields)
  {
    const auto view = ParseView(Read<std::string>(fields, "view"));
    const auto size = ParseSize(Read<std::string>(fields, "size"));
    if (!view || !size) {
      Fail("bad view or size for class " + std::to_string(spriteClass.typeIndex));
    }
    return {*view, *size};
  }

  std::vector<SpriteClass>& m_classes;
  std::vector<AtlasFile>& m_atlases;
  float& m_pitch;
};
} // namespace

bool DecodeQoi(std::span<const uint8_t> bytes, RgbaImage& image)
{
  image = {};
  constexpr size_t kHeader = 14;
  constexpr size_t kEndMarker = 8;
  if (bytes.size() < kHeader + kEndMarker || !std::ranges::equal(bytes.first(4), std::string_view("qoif"))) {
    return false;
  }
  const uint32_t width = ReadBigEndian(bytes, 4);
  const uint32_t height = ReadBigEndian(bytes, 8);
  if (width == 0 || height == 0 || width > kMaximumImageSide || height > kMaximumImageSide || bytes[12] < 3
      || bytes[12] > 4) {
    return false;
  }
  const size_t end = bytes.size() - kEndMarker;
  if (size_t{width} * height > (end - kHeader) * kQoiMaximumRun) {
    return false;
  }
  std::vector<uint8_t> pixels(size_t{width} * height * 4);
  std::array<std::array<uint8_t, 4>, 64> index{};
  std::array<uint8_t, 4> pixel = {0, 0, 0, 255};
  size_t position = kHeader;
  int run = 0;
  for (size_t offset = 0; offset < pixels.size(); offset += 4) {
    if (run > 0) {
      --run;
    } else {
      if (position >= end) {
        return false;
      }
      const uint8_t op = bytes[position++];
      if (op == 0xFE || op == 0xFF) {
        const size_t channels = op == 0xFE ? 3 : 4;
        if (position + channels > end) {
          return false;
        }
        std::copy_n(bytes.begin() + static_cast<ptrdiff_t>(position), channels, pixel.begin());
        position += channels;
      } else if ((op & 0xC0) == 0x00) {
        pixel = index[op];
      } else if ((op & 0xC0) == 0x40) {
        pixel[0] = static_cast<uint8_t>(pixel[0] + ((op >> 4) & 3) - 2);
        pixel[1] = static_cast<uint8_t>(pixel[1] + ((op >> 2) & 3) - 2);
        pixel[2] = static_cast<uint8_t>(pixel[2] + (op & 3) - 2);
      } else if ((op & 0xC0) == 0x80) {
        if (position >= end) {
          return false;
        }
        const uint8_t second = bytes[position++];
        const int green = (op & 0x3F) - 32;
        pixel[0] = static_cast<uint8_t>(pixel[0] + green - 8 + (second >> 4));
        pixel[1] = static_cast<uint8_t>(pixel[1] + green);
        pixel[2] = static_cast<uint8_t>(pixel[2] + green - 8 + (second & 0x0F));
      } else {
        run = op & 0x3F;
      }
      index[(pixel[0] * 3 + pixel[1] * 5 + pixel[2] * 7 + pixel[3] * 11) % 64] = pixel;
    }
    std::copy(pixel.begin(), pixel.end(), pixels.begin() + static_cast<ptrdiff_t>(offset));
  }
  image.width = static_cast<int>(width);
  image.height = static_cast<int>(height);
  image.pixels = std::move(pixels);
  return true;
}

int NearestHeadingIndex(float heading, int count)
{
  if (!std::isfinite(heading)) {
    return 0;
  }
  const double steps = std::round(static_cast<double>(heading) * count / (2.0 * std::numbers::pi));
  const auto index = static_cast<long long>(std::fmod(steps, static_cast<double>(count)));
  return static_cast<int>((index + count) % count);
}

std::optional<SpritePack> SpritePack::Load(const std::filesystem::path& directory, std::string* error)
{
  const auto fail = [error](std::string message) -> std::optional<SpritePack> {
    if (error) {
      *error = std::move(message);
    }
    return std::nullopt;
  };
  std::ifstream manifest(directory / "manifest.txt");
  if (!manifest) {
    return fail("no manifest.txt in " + directory.string());
  }
  std::string line;
  std::getline(manifest, line);
  std::istringstream header(line);
  std::string magic;
  int version = 0;
  if (!(header >> magic >> version) || magic != kManifestMagic || version != kManifestVersion) {
    return fail("not a version " + std::to_string(kManifestVersion) + " tactical sprite manifest");
  }
  SpritePack pack;
  pack.m_directory = directory;
  ManifestParser parser(pack.m_classes, pack.m_atlasFiles, pack.m_pitchDegrees);
  int lineNumber = 1;
  try {
    while (std::getline(manifest, line)) {
      ++lineNumber;
      parser.Line(line);
    }
    parser.Finish();
  } catch (const ManifestError& problem) {
    return fail("manifest.txt line " + std::to_string(lineNumber) + ": " + problem.message);
  }
  std::ranges::sort(pack.m_classes, {}, &SpriteClass::typeIndex);
  pack.m_atlases.resize(pack.m_atlasFiles.size());
  pack.m_atlasFailed.resize(pack.m_atlasFiles.size());
  return pack;
}

const SpriteClass* SpritePack::Find(int typeIndex) const
{
  const auto found = std::ranges::lower_bound(m_classes, typeIndex, {}, &SpriteClass::typeIndex);
  return found != m_classes.end() && found->typeIndex == typeIndex ? &*found : nullptr;
}

const SpriteFrame* SpritePack::Frame(int typeIndex, SpriteView view, SpriteSize size, float heading) const
{
  const SpriteClass* spriteClass = Find(typeIndex);
  if (!spriteClass) {
    return nullptr;
  }
  const SpriteSet& set = spriteClass->Set(view, size);
  if (set.frames.empty()) {
    return nullptr;
  }
  return &set.frames[static_cast<size_t>(NearestHeadingIndex(heading, static_cast<int>(set.frames.size())))];
}

std::vector<int> SpritePack::AtlasesFor(int typeIndex, SpriteSize size) const
{
  std::vector<int> atlases;
  if (const SpriteClass* spriteClass = Find(typeIndex)) {
    for (const SpriteView view : {SpriteView::Top, SpriteView::Angled}) {
      for (const SpriteFrame& frame : spriteClass->Set(view, size).frames) {
        atlases.push_back(frame.atlas);
      }
    }
  }
  std::ranges::sort(atlases);
  atlases.erase(std::ranges::unique(atlases).begin(), atlases.end());
  return atlases;
}

const RgbaImage* SpritePack::Atlas(int atlas)
{
  if (atlas < 0 || atlas >= static_cast<int>(m_atlasFiles.size())) {
    return nullptr;
  }
  auto& cached = m_atlases[static_cast<size_t>(atlas)];
  if (!cached && !m_atlasFailed[static_cast<size_t>(atlas)]) {
    const AtlasFile& file = m_atlasFiles[static_cast<size_t>(atlas)];
    const auto bytes = ReadFile(m_directory / file.file);
    RgbaImage image;
    if (bytes && DecodeQoi(*bytes, image) && image.width == file.width && image.height == file.height) {
      cached = std::move(image);
    } else {
      m_atlasFailed[static_cast<size_t>(atlas)] = true;
    }
  }
  return cached ? &*cached : nullptr;
}

void SpritePack::ReleaseAtlases()
{
  std::ranges::fill(m_atlases, std::nullopt);
  std::ranges::fill(m_atlasFailed, false);
}

} // namespace reb::battle_sprites
