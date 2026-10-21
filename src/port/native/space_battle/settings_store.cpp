#include "port/native/space_battle/settings_store.h"

#include <charconv>
#include <utility>

namespace reb::native::space_battle
{
namespace
{

std::string Encode(std::string_view value)
{
  if (!value.starts_with("hex:") && value.find_first_of("\r\n\t") == std::string_view::npos
      && (value.empty() || (value.front() != ' ' && value.back() != ' '))) {
    return std::string(value);
  }
  constexpr std::string_view kHex = "0123456789abcdef";
  std::string encoded = "hex:";
  for (unsigned char c : value) {
    encoded += kHex[c >> 4];
    encoded += kHex[c & 15];
  }
  return encoded;
}

std::string Decode(const std::string& value)
{
  if (!value.starts_with("hex:") || value.size() % 2 != 0) {
    return value;
  }
  std::string decoded;
  for (size_t offset = 4; offset < value.size(); offset += 2) {
    unsigned int byte = 0;
    const auto [end, error] = std::from_chars(value.data() + offset, value.data() + offset + 2, byte, 16);
    if (error != std::errc{} || end != value.data() + offset + 2) {
      return value;
    }
    decoded += static_cast<char>(byte);
  }
  return decoded;
}

} // namespace

ConfigSettingsStore::ConfigSettingsStore()
    : ConfigSettingsStore(mw::config::Global(), mw::config::GlobalPath())
{
}

ConfigSettingsStore::ConfigSettingsStore(mw::config::Config& config, std::string path)
    : m_config(config), m_path(std::move(path))
{
}

std::optional<std::string> ConfigSettingsStore::Get(std::string_view key) const
{
  // HUD settings are nonempty strings; an erased or missing setting uses its default.
  const std::string value = m_config.Get(std::string(key));
  return value.empty() ? std::nullopt : std::optional(Decode(value));
}

void ConfigSettingsStore::Set(std::string_view key, std::string_view value)
{
  m_config.Set(std::string(key), Encode(value));
  m_config.Save(m_path);
}

void ConfigSettingsStore::Erase(std::string_view key)
{
  m_config.Erase(std::string(key));
  m_config.Save(m_path);
}

} // namespace reb::native::space_battle
