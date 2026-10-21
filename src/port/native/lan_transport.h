#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace reb::native::lan
{

constexpr uint16_t kDefaultPort = 47624;
constexpr uint32_t kProtocol = 1;
constexpr uint32_t kMaxMessageSize = 64 * 1024 * 1024;

std::vector<std::string> LocalAddresses();

struct Advertisement
{
  std::string name;
  std::string address;
  uint16_t port = 0;
  uint32_t protocol = 0;
};

struct Message
{
  uint32_t kind = 0;
  std::vector<uint8_t> bytes;
};

enum class Status
{
  Idle,
  Hosting,
  Connecting,
  Connected,
  Disconnected,
  Error,
};

class Transport
{
public:
  Transport();
  ~Transport();
  Transport(Transport&&) noexcept;
  Transport& operator=(Transport&&) noexcept;
  Transport(const Transport&) = delete;
  Transport& operator=(const Transport&) = delete;

  bool Host(std::string name, uint16_t port = kDefaultPort);
  bool Join(const std::string& address, uint16_t port = kDefaultPort);
  bool Discover(uint16_t port = kDefaultPort);
  void Poll();
  bool Send(uint32_t kind, std::span<const uint8_t> bytes);
  size_t PendingSendBytes() const;
  bool Drain(std::chrono::milliseconds maxWait);
  std::optional<Message> TakeMessage();
  Status GetStatus() const;
  const std::string& Error() const;
  std::vector<Advertisement> Peers() const;
  void DisconnectPeer();
  void Close();
  bool IsHost() const;
  uint16_t LocalPort() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace reb::native::lan
