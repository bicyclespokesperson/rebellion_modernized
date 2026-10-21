#include "port/native/campaign_checkpoints.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace reb::native
{
namespace
{
constexpr std::array<uint8_t, 4> kMagic{'R', 'C', 'P', '1'};
constexpr uint32_t kEnvelopeVersion = 1;
constexpr uint32_t kMaximumText = 4096;
constexpr uint64_t kMaximumSnapshot = 256ULL * 1024 * 1024;

struct TemporaryPathCleanup
{
  std::filesystem::path path;
  ~TemporaryPathCleanup()
  {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
};

bool WriteTemporary(const std::filesystem::path& directory, std::string_view stem,
                    std::span<const uint8_t> bytes, std::filesystem::path& temporary,
                    std::string& error)
{
#ifdef _WIN32
  (void)stem;
  wchar_t name[MAX_PATH + 1]{};
  if (!GetTempFileNameW(directory.c_str(), L"rcp", 0, name)) {
    error = "could not create checkpoint temporary file";
    return false;
  }
  temporary = name;
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    error = "could not open checkpoint temporary file";
    return false;
  }
  size_t offset = 0;
  while (offset < bytes.size()) {
    const DWORD length = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 1U << 20));
    DWORD written = 0;
    if (!WriteFile(file, bytes.data() + offset, length, &written, nullptr) || written == 0) {
      error = "could not write checkpoint";
      break;
    }
    offset += written;
  }
  if (error.empty() && !FlushFileBuffers(file)) error = "could not flush checkpoint";
  if (!CloseHandle(file) && error.empty()) error = "could not close checkpoint";
#else
  std::string pattern = (directory / (std::string(stem) + "-XXXXXX.tmp")).string();
  std::vector<char> name(pattern.begin(), pattern.end());
  name.push_back('\0');
  const int file = mkstemps(name.data(), 4);
  if (file < 0) {
    error = "could not create checkpoint temporary file: " + std::error_code(errno, std::generic_category()).message();
    return false;
  }
  temporary = name.data();
  size_t offset = 0;
  while (offset < bytes.size()) {
    const ssize_t written = write(file, bytes.data() + offset, bytes.size() - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) {
      error = "could not write checkpoint";
      break;
    }
    offset += static_cast<size_t>(written);
  }
  if (error.empty() && fsync(file) != 0) error = "could not flush checkpoint";
  if (close(file) != 0 && error.empty()) error = "could not close checkpoint";
#endif
  return error.empty();
}

bool CommitTemporary(const std::filesystem::path& temporary, const std::filesystem::path& target,
                     const std::filesystem::path& directory, bool& collision, std::string& error)
{
#ifdef _WIN32
  if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) {
    collision = GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_FILE_EXISTS;
    if (!collision) error = "could not commit checkpoint";
    return false;
  }
#else
  if (link(temporary.c_str(), target.c_str()) != 0) {
    collision = errno == EEXIST;
    if (!collision) error = "could not commit checkpoint: " + std::error_code(errno, std::generic_category()).message();
    return false;
  }
  std::error_code removeError;
  std::filesystem::remove(temporary, removeError);
  const int directoryFile = open(directory.c_str(), O_RDONLY | O_DIRECTORY);
  if (directoryFile < 0) {
    error = "could not open checkpoint directory for flush";
    return false;
  }
  if (fsync(directoryFile) != 0) error = "could not flush checkpoint directory";
  if (close(directoryFile) != 0 && error.empty()) error = "could not close checkpoint directory";
  if (!error.empty()) return false;
#endif
  return true;
}

uint32_t Checksum(std::span<const uint8_t> bytes) noexcept
{
  uint32_t crc = 0xffffffffU;
  for (uint8_t byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

void Put32(std::vector<uint8_t>& out, uint32_t value)
{
  for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<uint8_t>(value >> shift));
}
void Put64(std::vector<uint8_t>& out, uint64_t value)
{
  for (unsigned shift = 0; shift < 64; shift += 8) out.push_back(static_cast<uint8_t>(value >> shift));
}
bool Get32(std::span<const uint8_t> data, size_t& offset, uint32_t& value) noexcept
{
  if (offset > data.size() || data.size() - offset < 4) return false;
  value = 0;
  for (unsigned shift = 0; shift < 32; shift += 8) value |= uint32_t(data[offset++]) << shift;
  return true;
}
bool Get64(std::span<const uint8_t> data, size_t& offset, uint64_t& value) noexcept
{
  if (offset > data.size() || data.size() - offset < 8) return false;
  value = 0;
  for (unsigned shift = 0; shift < 64; shift += 8) value |= uint64_t(data[offset++]) << shift;
  return true;
}
void PutText(std::vector<uint8_t>& out, const std::string& value)
{
  Put32(out, static_cast<uint32_t>(value.size()));
  out.insert(out.end(), value.begin(), value.end());
}
bool GetText(std::span<const uint8_t> data, size_t& offset, std::string& value)
{
  uint32_t length = 0;
  if (!Get32(data, offset, length) || length > kMaximumText || offset > data.size() || data.size() - offset < length)
    return false;
  value.assign(reinterpret_cast<const char*>(data.data() + offset), length);
  offset += length;
  return true;
}

bool Encode(const CampaignCheckpointMetadata& metadata, std::span<const uint8_t> gameState, bool automatic,
            std::vector<uint8_t>& output, std::string& error)
{
  if (metadata.campaignId.empty() || metadata.sessionId.empty() || metadata.hostSide < 1 || metadata.hostSide > 2
      || metadata.formatVersion == 0 || metadata.protocolVersion == 0) {
    error = "invalid checkpoint metadata";
    return false;
  }
  if (metadata.campaignId.size() > kMaximumText || metadata.sessionId.size() > kMaximumText
      || metadata.displayName.size() > kMaximumText || gameState.size() > kMaximumSnapshot) {
    error = "checkpoint exceeds size limit";
    return false;
  }
  output.insert(output.end(), kMagic.begin(), kMagic.end());
  Put32(output, kEnvelopeVersion);
  output.push_back(automatic ? 1 : 0);
  output.push_back(metadata.hostSide);
  Put32(output, metadata.formatVersion);
  Put32(output, metadata.protocolVersion);
  Put32(output, metadata.gameTick);
  Put64(output, static_cast<uint64_t>(metadata.unixTimestamp));
  PutText(output, metadata.campaignId);
  PutText(output, metadata.sessionId);
  PutText(output, metadata.displayName);
  Put64(output, gameState.size());
  output.insert(output.end(), gameState.begin(), gameState.end());
  Put32(output, Checksum(output));
  return true;
}

bool Decode(std::span<const uint8_t> bytes, CampaignCheckpoint& checkpoint, std::string& error)
{
  if (bytes.size() < 4 + 4 + 2 + 12 + 8 + 12 + 8 + 4 || bytes.size() > kMaximumSnapshot + 16384) {
    error = "invalid checkpoint size";
    return false;
  }
  if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) { error = "invalid checkpoint signature"; return false; }
  const uint32_t expectedCrc = Checksum(bytes.first(bytes.size() - 4));
  size_t crcOffset = bytes.size() - 4;
  uint32_t actualCrc = 0;
  if (!Get32(bytes, crcOffset, actualCrc) || actualCrc != expectedCrc) { error = "checkpoint checksum mismatch"; return false; }
  size_t offset = 4;
  uint32_t envelope = 0;
  if (!Get32(bytes, offset, envelope) || envelope != kEnvelopeVersion) { error = "unsupported checkpoint envelope"; return false; }
  const uint8_t automatic = bytes[offset++];
  auto& metadata = checkpoint.metadata;
  metadata.hostSide = bytes[offset++];
  uint64_t timestamp = 0;
  if (automatic > 1 || !Get32(bytes, offset, metadata.formatVersion) || !Get32(bytes, offset, metadata.protocolVersion)
      || !Get32(bytes, offset, metadata.gameTick) || !Get64(bytes, offset, timestamp)
      || !GetText(bytes, offset, metadata.campaignId) || !GetText(bytes, offset, metadata.sessionId)
      || !GetText(bytes, offset, metadata.displayName)) { error = "malformed checkpoint metadata"; return false; }
  metadata.unixTimestamp = static_cast<int64_t>(timestamp);
  uint64_t payloadSize = 0;
  if (!Get64(bytes, offset, payloadSize) || payloadSize > kMaximumSnapshot || offset > bytes.size() - 4
      || payloadSize != bytes.size() - 4 - offset || metadata.campaignId.empty() || metadata.sessionId.empty()
      || metadata.hostSide < 1 || metadata.hostSide > 2 || !metadata.formatVersion || !metadata.protocolVersion) {
    error = "invalid checkpoint payload or metadata";
    return false;
  }
  checkpoint.automatic = automatic != 0;
  checkpoint.gameState.assign(bytes.begin() + offset, bytes.end() - 4);
  return true;
}

} // namespace

CampaignCheckpoints::CampaignCheckpoints(std::filesystem::path directory) : directory_(std::move(directory)) {}

CampaignCheckpointResult CampaignCheckpoints::Save(const CampaignCheckpointMetadata& metadata,
                                                    std::span<const uint8_t> gameState, bool automatic,
                                                    size_t retainAutomatic) const noexcept
{
  CampaignCheckpointResult result;
  try {
    std::vector<uint8_t> bytes;
    if (!Encode(metadata, gameState, automatic, bytes, result.error)) return result;
    std::filesystem::create_directories(directory_);
    const auto stamp = metadata.unixTimestamp;
    const std::string stem = std::string(automatic ? "auto-" : "manual-") + std::to_string(stamp);
    for (int attempt = 0; attempt < 100; ++attempt) {
      std::filesystem::path temporary;
      if (!WriteTemporary(directory_, stem, bytes, temporary, result.error)) return result;
      TemporaryPathCleanup cleanup{temporary};
#ifdef _WIN32
      const auto target = directory_ / (stem + "-" + temporary.stem().string() + ".rcp");
#else
      const auto target = directory_ / (temporary.stem().string() + ".rcp");
#endif
      bool collision = false;
      if (!CommitTemporary(temporary, target, directory_, collision, result.error)) {
        if (collision) continue;
        return result;
      }
      result.path = target;
      break;
    }
    if (result.path.empty()) { result.error = "could not allocate checkpoint filename"; return result; }
    if (automatic) {
      auto checkpoints = List();
      size_t kept = 1;
      std::error_code ec;
      for (const auto& item : checkpoints) {
        if (!item.automatic || item.metadata.campaignId != metadata.campaignId) continue;
        if (item.path == result.path) continue;
        if (kept >= std::max<size_t>(1, retainAutomatic)) std::filesystem::remove(item.path, ec);
        else ++kept;
      }
    }
    return result;
  } catch (const std::exception& exception) {
    result.error = exception.what();
  } catch (...) {
    result.error = "unknown checkpoint storage error";
  }
  return result;
}

std::optional<CampaignCheckpoint> CampaignCheckpoints::Load(const std::filesystem::path& path, std::string* error) const noexcept
{
  auto fail = [&](const char* message) -> std::optional<CampaignCheckpoint> { if (error) *error = message; return std::nullopt; };
  try {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaximumSnapshot + 16384) return fail("checkpoint file unavailable or too large");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input || !input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return fail("could not read checkpoint");
    CampaignCheckpoint checkpoint;
    std::string decodeError;
    if (!Decode(bytes, checkpoint, decodeError)) { if (error) *error = std::move(decodeError); return std::nullopt; }
    checkpoint.path = path;
    if (error) error->clear();
    return checkpoint;
  } catch (...) { return fail("checkpoint read failed"); }
}

std::vector<CampaignCheckpointInfo> CampaignCheckpoints::List() const noexcept
{
  std::vector<CampaignCheckpointInfo> result;
  try {
    std::error_code ec;
    if (!std::filesystem::exists(directory_, ec)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(directory_, ec)) {
      if (ec || !entry.is_regular_file(ec) || entry.path().extension() != ".rcp") continue;
      auto checkpoint = Load(entry.path());
      if (checkpoint) result.push_back({std::move(checkpoint->metadata), checkpoint->automatic, entry.path()});
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
      if (left.metadata.unixTimestamp != right.metadata.unixTimestamp) return left.metadata.unixTimestamp > right.metadata.unixTimestamp;
      return left.path.filename().string() > right.path.filename().string();
    });
  } catch (...) { result.clear(); }
  return result;
}

CampaignCheckpointCadence::CampaignCheckpointCadence(Duration interval) noexcept : interval_(interval) {}
void CampaignCheckpointCadence::Reset(uint32_t currentTick) noexcept { activeTime_ = Duration::zero(); savedTick_ = currentTick; }
bool CampaignCheckpointCadence::Update(Duration elapsed, bool connected, bool paused, uint32_t currentTick) noexcept
{
  if (connected && !paused && elapsed > Duration::zero()) activeTime_ += elapsed;
  return connected && !paused && currentTick != savedTick_ && activeTime_ >= interval_;
}
void CampaignCheckpointCadence::Saved(uint32_t tick) noexcept { savedTick_ = tick; activeTime_ = Duration::zero(); }

} // namespace reb::native
