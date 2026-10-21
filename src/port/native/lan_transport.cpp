#include "port/native/lan_transport.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <limits>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace reb::native::lan
{
namespace
{

#ifdef _WIN32
using Socket = SOCKET;
using SockLen = int;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
int LastSocketError() { return WSAGetLastError(); }
bool WouldBlock(int error) { return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS; }
void CloseSocket(Socket socket) { if (socket != kInvalidSocket) closesocket(socket); }
bool SetNonblocking(Socket socket)
{
  u_long enabled = 1;
  return ioctlsocket(socket, FIONBIO, &enabled) == 0;
}
struct SocketRuntime
{
  SocketRuntime() { WSADATA data{}; ready = WSAStartup(MAKEWORD(2, 2), &data) == 0; }
  ~SocketRuntime() { if (ready) WSACleanup(); }
  bool ready = false;
};
SocketRuntime& Runtime() { static SocketRuntime runtime; return runtime; }
#else
using Socket = int;
using SockLen = socklen_t;
constexpr Socket kInvalidSocket = -1;
int LastSocketError() { return errno; }
bool WouldBlock(int error) { return error == EWOULDBLOCK || error == EAGAIN || error == EINPROGRESS; }
void CloseSocket(Socket socket) { if (socket != kInvalidSocket) close(socket); }
bool SetNonblocking(Socket socket)
{
  int flags = fcntl(socket, F_GETFL, 0);
  return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}
struct SocketRuntime { bool ready = true; };
SocketRuntime& Runtime() { static SocketRuntime runtime; return runtime; }
#endif

constexpr uint32_t kFrameMagic = 0x52424c4e; // RBLN
constexpr uint32_t kDiscoveryMagic = 0x52424c44; // RBLD
constexpr size_t kFrameHeaderSize = 16;
constexpr size_t kMaxQueuedBytes = 128 * 1024 * 1024;
constexpr size_t kPollByteBudget = 1024 * 1024;
constexpr auto kDiscoveryInterval = std::chrono::seconds(1);
constexpr auto kPeerLifetime = std::chrono::seconds(5);

void Write32(uint8_t* target, uint32_t value)
{
  target[0] = static_cast<uint8_t>(value >> 24);
  target[1] = static_cast<uint8_t>(value >> 16);
  target[2] = static_cast<uint8_t>(value >> 8);
  target[3] = static_cast<uint8_t>(value);
}

uint32_t Read32(const uint8_t* source)
{
  return (uint32_t(source[0]) << 24) | (uint32_t(source[1]) << 16) |
         (uint32_t(source[2]) << 8) | uint32_t(source[3]);
}

void Write16(uint8_t* target, uint16_t value)
{
  target[0] = static_cast<uint8_t>(value >> 8);
  target[1] = static_cast<uint8_t>(value);
}

uint16_t Read16(const uint8_t* source)
{
  return static_cast<uint16_t>((uint16_t(source[0]) << 8) | source[1]);
}

bool PrepareSocket(Socket socket)
{
#ifdef SO_NOSIGPIPE
  int enabled = 1;
  setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&enabled), sizeof(enabled));
#endif
  return SetNonblocking(socket);
}

int SendFlags()
{
#ifdef MSG_NOSIGNAL
  return MSG_NOSIGNAL;
#else
  return 0;
#endif
}

std::string SocketErrorText(const char* operation)
{
  return std::string(operation) + " failed (socket error " + std::to_string(LastSocketError()) + ")";
}

std::string AddressText(const sockaddr_in& address)
{
  std::array<char, INET_ADDRSTRLEN> text{};
  if (!inet_ntop(AF_INET, &address.sin_addr, text.data(), static_cast<SockLen>(text.size()))) return {};
  return text.data();
}

sockaddr_in MakeAddress(uint32_t ipv4, uint16_t port)
{
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(ipv4);
  address.sin_port = htons(port);
  return address;
}

} // namespace

std::vector<std::string> LocalAddresses()
{
  std::vector<std::string> addresses;
  if (!Runtime().ready) return {"127.0.0.1"};
#ifdef _WIN32
  std::array<char, 256> hostname{};
  if (gethostname(hostname.data(), static_cast<int>(hostname.size())) == 0) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* resolved = nullptr;
    if (getaddrinfo(hostname.data(), nullptr, &hints, &resolved) == 0) {
      for (addrinfo* item = resolved; item; item = item->ai_next) {
        const auto* address = reinterpret_cast<const sockaddr_in*>(item->ai_addr);
        const uint32_t ipv4 = ntohl(address->sin_addr.s_addr);
        if (ipv4 != 0 && (ipv4 >> 24) != 127) addresses.push_back(AddressText(*address));
      }
      freeaddrinfo(resolved);
    }
  }
#else
  ifaddrs* interfaces = nullptr;
  if (getifaddrs(&interfaces) == 0) {
    for (const ifaddrs* item = interfaces; item; item = item->ifa_next) {
      if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET ||
          !(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK)) continue;
      const auto& address = *reinterpret_cast<const sockaddr_in*>(item->ifa_addr);
      if (address.sin_addr.s_addr != htonl(INADDR_ANY)) addresses.push_back(AddressText(address));
    }
    freeifaddrs(interfaces);
  }
#endif
  std::erase(addresses, std::string{});
  std::ranges::sort(addresses);
  addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
  if (addresses.empty()) addresses.push_back("127.0.0.1");
  return addresses;
}

struct Transport::Impl
{
  struct SeenPeer
  {
    Advertisement advertisement;
    uint64_t id;
    std::chrono::steady_clock::time_point lastSeen;
  };

  Socket listener = kInvalidSocket;
  Socket peer = kInvalidSocket;
  Socket discovery = kInvalidSocket;
  Status status = Status::Idle;
  bool host = false;
  bool discovering = false;
  std::string error;
  std::string name;
  uint64_t discoveryId = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
  uint16_t localPort = 0;
  uint16_t discoveryPort = 0;
  std::chrono::steady_clock::time_point lastDiscovery{};
  std::vector<SeenPeer> seenPeers;
  std::deque<Message> messages;
  size_t queuedMessageBytes = 0;
  std::deque<std::vector<uint8_t>> outbound;
  size_t queuedOutboundBytes = 0;
  size_t outboundOffset = 0;
  std::vector<uint8_t> inbound;

  void ResetPeer()
  {
    CloseSocket(peer);
    peer = kInvalidSocket;
    inbound.clear();
    outbound.clear();
    outboundOffset = 0;
    queuedOutboundBytes = 0;
  }

  void Disconnect(std::string reason = {})
  {
    ResetPeer();
    if (reason.empty()) {
      error.clear();
      status = Status::Disconnected;
    } else {
      error = std::move(reason);
      status = Status::Error;
    }
  }

  bool ReadFrames()
  {
    std::array<uint8_t, 65536> chunk{};
    size_t budget = kPollByteBudget;
    while (budget > 0) {
      const int count = recv(peer, reinterpret_cast<char*>(chunk.data()),
                             static_cast<int>(std::min(chunk.size(), budget)), 0);
      if (count == 0) {
        Disconnect();
        return false;
      }
      if (count < 0) {
        if (WouldBlock(LastSocketError())) return true;
        Disconnect(SocketErrorText("receive"));
        return false;
      }
      budget -= static_cast<size_t>(count);
      inbound.insert(inbound.end(), chunk.begin(), chunk.begin() + count);
      while (inbound.size() >= kFrameHeaderSize) {
        const uint32_t magic = Read32(inbound.data());
        const uint32_t protocol = Read32(inbound.data() + 4);
        const uint32_t kind = Read32(inbound.data() + 8);
        const uint32_t length = Read32(inbound.data() + 12);
        if (magic != kFrameMagic || protocol != kProtocol || length > kMaxMessageSize) {
          Disconnect("Invalid LAN frame or protocol version");
          return false;
        }
        if (inbound.size() < kFrameHeaderSize + size_t(length)) break;
        if (queuedMessageBytes + kFrameHeaderSize + length > kMaxQueuedBytes) {
          Disconnect("LAN receive queue is full");
          return false;
        }
        Message message;
        message.kind = kind;
        message.bytes.assign(inbound.begin() + kFrameHeaderSize,
                             inbound.begin() + kFrameHeaderSize + length);
        queuedMessageBytes += kFrameHeaderSize + length;
        messages.push_back(std::move(message));
        inbound.erase(inbound.begin(), inbound.begin() + kFrameHeaderSize + length);
      }
    }
    return true;
  }

  void WriteFrames()
  {
    size_t budget = kPollByteBudget;
    while (!outbound.empty() && budget > 0) {
      const auto& frame = outbound.front();
      const size_t length = std::min({frame.size() - outboundOffset, budget,
                                      size_t(std::numeric_limits<int>::max())});
      const int count = send(peer, reinterpret_cast<const char*>(frame.data() + outboundOffset),
                             static_cast<int>(length), SendFlags());
      if (count < 0) {
        if (WouldBlock(LastSocketError())) return;
        Disconnect(SocketErrorText("send"));
        return;
      }
      if (count == 0) return;
      budget -= static_cast<size_t>(count);
      outboundOffset += static_cast<size_t>(count);
      queuedOutboundBytes -= static_cast<size_t>(count);
      if (outboundOffset == frame.size()) {
        outbound.pop_front();
        outboundOffset = 0;
      }
    }
  }

  void AcceptGuest()
  {
    if (listener == kInvalidSocket) return;
    for (int attempt = 0; attempt < 16; ++attempt) {
      sockaddr_in address{};
      SockLen length = sizeof(address);
      Socket candidate = accept(listener, reinterpret_cast<sockaddr*>(&address), &length);
      if (candidate == kInvalidSocket) return;
      if (peer != kInvalidSocket || !PrepareSocket(candidate)) {
        CloseSocket(candidate);
        continue;
      }
      peer = candidate;
      messages.clear();
      queuedMessageBytes = 0;
      status = Status::Connected;
      error.clear();
    }
  }

  void CheckConnection()
  {
    if (status != Status::Connecting || peer == kInvalidSocket) return;
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(peer, &writable);
    timeval timeout{};
#ifdef _WIN32
    const int ready = select(0, nullptr, &writable, nullptr, &timeout);
#else
    const int ready = select(peer + 1, nullptr, &writable, nullptr, &timeout);
#endif
    if (ready == 0) return;
    if (ready < 0) {
      Disconnect(SocketErrorText("connect"));
      return;
    }
    int errorCode = 0;
    SockLen length = sizeof(errorCode);
    if (getsockopt(peer, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&errorCode), &length) != 0 || errorCode != 0) {
      Disconnect("LAN connection failed (socket error " + std::to_string(errorCode ? errorCode : LastSocketError()) + ")");
      return;
    }
    status = Status::Connected;
  }

  void AnnounceTo(const sockaddr_in& destination)
  {
    std::array<uint8_t, 220> packet{};
    const size_t nameLength = std::min(name.size(), size_t(200));
    Write32(packet.data(), kDiscoveryMagic);
    Write32(packet.data() + 4, kProtocol);
    Write16(packet.data() + 8, localPort);
    Write16(packet.data() + 10, static_cast<uint16_t>(nameLength));
    Write32(packet.data() + 12, static_cast<uint32_t>(discoveryId >> 32));
    Write32(packet.data() + 16, static_cast<uint32_t>(discoveryId));
    std::memcpy(packet.data() + 20, name.data(), nameLength);
    sendto(discovery, reinterpret_cast<const char*>(packet.data()), static_cast<int>(20 + nameLength),
           0, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));
  }

  void PollDiscovery()
  {
    if (discovery == kInvalidSocket) return;
    const auto now = std::chrono::steady_clock::now();
    if (discovering && (lastDiscovery == std::chrono::steady_clock::time_point{} ||
                        now - lastDiscovery >= kDiscoveryInterval)) {
      std::array<uint8_t, 8> query{};
      Write32(query.data(), kDiscoveryMagic);
      Write32(query.data() + 4, kProtocol);
      for (uint32_t target : {uint32_t(0xffffffff), uint32_t(0x7f000001)}) {
        const sockaddr_in address = MakeAddress(target, discoveryPort);
        sendto(discovery, reinterpret_cast<const char*>(query.data()), static_cast<int>(query.size()),
               0, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
      }
      lastDiscovery = now;
    }
    std::array<uint8_t, 512> packet{};
    for (int attempt = 0; attempt < 32; ++attempt) {
      sockaddr_in source{};
      SockLen sourceLength = sizeof(source);
      const int count = recvfrom(discovery, reinterpret_cast<char*>(packet.data()),
                                 static_cast<int>(packet.size()), 0,
                                 reinterpret_cast<sockaddr*>(&source), &sourceLength);
      if (count < 0) break;
      if (count < 8 || Read32(packet.data()) != kDiscoveryMagic || Read32(packet.data() + 4) != kProtocol) continue;
      if (host && count == 8) {
        AnnounceTo(source);
      } else if (discovering && count >= 20) {
        const uint16_t port = Read16(packet.data() + 8);
        const uint16_t nameLength = Read16(packet.data() + 10);
        if (port == 0 || nameLength == 0 || nameLength > 200 || count != 20 + nameLength) continue;
        const uint64_t id = (uint64_t(Read32(packet.data() + 12)) << 32) | Read32(packet.data() + 16);
        Advertisement advertisement{
            std::string(reinterpret_cast<const char*>(packet.data() + 20), nameLength),
            AddressText(source), port, kProtocol};
        const auto match = std::find_if(seenPeers.begin(), seenPeers.end(), [&](const SeenPeer& peer) {
          return peer.id == id;
        });
        if (match == seenPeers.end()) {
          if (seenPeers.size() >= 256) {
            const auto oldest = std::min_element(seenPeers.begin(), seenPeers.end(),
                [](const SeenPeer& a, const SeenPeer& b) { return a.lastSeen < b.lastSeen; });
            seenPeers.erase(oldest);
          }
          seenPeers.push_back({std::move(advertisement), id, now});
        }
        else {
          if (advertisement.address == "127.0.0.1" || match->advertisement.address != "127.0.0.1")
            match->advertisement = std::move(advertisement);
          match->lastSeen = now;
        }
      }
    }
    std::erase_if(seenPeers, [&](const SeenPeer& peer) { return now - peer.lastSeen > kPeerLifetime; });
  }
};

Transport::Transport() : impl_(std::make_unique<Impl>()) {}
Transport::~Transport() { Close(); }
Transport::Transport(Transport&&) noexcept = default;
Transport& Transport::operator=(Transport&& other) noexcept
{
  if (this != &other) {
    Close();
    impl_ = std::move(other.impl_);
  }
  return *this;
}

void Transport::Close()
{
  if (!impl_) return;
  impl_->ResetPeer();
  CloseSocket(impl_->listener);
  CloseSocket(impl_->discovery);
  impl_->listener = impl_->discovery = kInvalidSocket;
  impl_->status = Status::Idle;
  impl_->host = impl_->discovering = false;
  impl_->localPort = impl_->discoveryPort = 0;
  impl_->name.clear();
  impl_->error.clear();
  impl_->seenPeers.clear();
  impl_->messages.clear();
  impl_->queuedMessageBytes = 0;
}

bool Transport::Host(std::string name, uint16_t port)
{
  Close();
  if (!Runtime().ready || name.empty()) {
    impl_->error = "LAN sockets unavailable or host name is empty";
    impl_->status = Status::Error;
    return false;
  }
  const int attempts = port == 0 ? 16 : 1;
  for (int attempt = 0; attempt < attempts; ++attempt) {
    Socket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidSocket) {
      impl_->error = SocketErrorText("create listener");
      break;
    }
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    const sockaddr_in address = MakeAddress(INADDR_ANY, port);
    if (!PrepareSocket(listener) || bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 1) != 0) {
      impl_->error = SocketErrorText("listen");
      CloseSocket(listener);
      if (port != 0) break;
      continue;
    }
    sockaddr_in bound{};
    SockLen boundLength = sizeof(bound);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &boundLength) != 0) {
      impl_->error = SocketErrorText("local port");
      CloseSocket(listener);
      break;
    }
    const uint16_t selectedPort = ntohs(bound.sin_port);
    Socket discovery = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const sockaddr_in discoveryAddress = MakeAddress(INADDR_ANY, selectedPort);
    if (discovery == kInvalidSocket || !PrepareSocket(discovery) ||
        bind(discovery, reinterpret_cast<const sockaddr*>(&discoveryAddress), sizeof(discoveryAddress)) != 0) {
      impl_->error = SocketErrorText("discovery listener");
      CloseSocket(discovery);
      CloseSocket(listener);
      if (port != 0) break;
      continue;
    }
    impl_->listener = listener;
    impl_->discovery = discovery;
    impl_->localPort = selectedPort;
    impl_->host = true;
    impl_->name = std::move(name);
    impl_->status = Status::Hosting;
    impl_->error.clear();
    return true;
  }
  impl_->status = Status::Error;
  return false;
}

bool Transport::Join(const std::string& address, uint16_t port)
{
  Close();
  if (!Runtime().ready || address.empty() || port == 0) {
    impl_->error = "Invalid LAN address or port";
    impl_->status = Status::Error;
    return false;
  }
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  addrinfo* addresses = nullptr;
  if (getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &addresses) != 0) {
    impl_->error = "Could not resolve LAN address";
    impl_->status = Status::Error;
    return false;
  }
  for (addrinfo* item = addresses; item; item = item->ai_next) {
    Socket peer = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
    if (peer == kInvalidSocket) continue;
    if (!PrepareSocket(peer)) { CloseSocket(peer); continue; }
    const int result = connect(peer, item->ai_addr, static_cast<SockLen>(item->ai_addrlen));
    if (result == 0 || WouldBlock(LastSocketError())) {
      impl_->peer = peer;
      impl_->status = result == 0 ? Status::Connected : Status::Connecting;
      break;
    }
    CloseSocket(peer);
  }
  freeaddrinfo(addresses);
  if (impl_->peer == kInvalidSocket) {
    impl_->error = "Could not connect to LAN host";
    impl_->status = Status::Error;
    return false;
  }
  return true;
}

bool Transport::Discover(uint16_t port)
{
  if (!Runtime().ready || port == 0 || impl_->host) {
    impl_->error = "LAN discovery is unavailable";
    if (impl_->status == Status::Idle) impl_->status = Status::Error;
    return false;
  }
  CloseSocket(impl_->discovery);
  impl_->discovery = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (impl_->discovery == kInvalidSocket || !PrepareSocket(impl_->discovery)) {
    impl_->error = SocketErrorText("create discovery socket");
    if (impl_->status == Status::Idle) impl_->status = Status::Error;
    CloseSocket(impl_->discovery);
    impl_->discovery = kInvalidSocket;
    return false;
  }
  int broadcast = 1;
  setsockopt(impl_->discovery, SOL_SOCKET, SO_BROADCAST,
             reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));
  const sockaddr_in address = MakeAddress(INADDR_ANY, 0);
  if (bind(impl_->discovery, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
    impl_->error = SocketErrorText("bind discovery socket");
    if (impl_->status == Status::Idle) impl_->status = Status::Error;
    CloseSocket(impl_->discovery);
    impl_->discovery = kInvalidSocket;
    return false;
  }
  impl_->discoveryPort = port;
  impl_->discovering = true;
  impl_->lastDiscovery = {};
  impl_->seenPeers.clear();
  impl_->error.clear();
  if (impl_->status == Status::Error && impl_->peer == kInvalidSocket) impl_->status = Status::Idle;
  return true;
}

void Transport::Poll()
{
  impl_->PollDiscovery();
  impl_->AcceptGuest();
  impl_->CheckConnection();
  if (impl_->status == Status::Connected) {
    if (impl_->ReadFrames() && impl_->status == Status::Connected) impl_->WriteFrames();
  }
}

bool Transport::Drain(std::chrono::milliseconds maxWait)
{
  const auto deadline = std::chrono::steady_clock::now() + maxWait;
  for (;;) {
    if (impl_->status != Status::Connected) return false;
    Poll();
    if (impl_->queuedOutboundBytes == 0) return impl_->status == Status::Connected;
    if (std::chrono::steady_clock::now() >= deadline) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

bool Transport::Send(uint32_t kind, std::span<const uint8_t> bytes)
{
  if (impl_->status != Status::Connected || bytes.size() > kMaxMessageSize ||
      bytes.size() + kFrameHeaderSize > kMaxQueuedBytes - impl_->queuedOutboundBytes) return false;
  std::vector<uint8_t> frame(kFrameHeaderSize + bytes.size());
  Write32(frame.data(), kFrameMagic);
  Write32(frame.data() + 4, kProtocol);
  Write32(frame.data() + 8, kind);
  Write32(frame.data() + 12, static_cast<uint32_t>(bytes.size()));
  std::copy(bytes.begin(), bytes.end(), frame.begin() + kFrameHeaderSize);
  impl_->queuedOutboundBytes += frame.size();
  impl_->outbound.push_back(std::move(frame));
  return true;
}

size_t Transport::PendingSendBytes() const
{
  return impl_->queuedOutboundBytes;
}

std::optional<Message> Transport::TakeMessage()
{
  if (impl_->messages.empty()) return std::nullopt;
  Message message = std::move(impl_->messages.front());
  impl_->queuedMessageBytes -= kFrameHeaderSize + message.bytes.size();
  impl_->messages.pop_front();
  return message;
}

Status Transport::GetStatus() const { return impl_->status; }
const std::string& Transport::Error() const { return impl_->error; }
std::vector<Advertisement> Transport::Peers() const
{
  std::vector<Advertisement> peers;
  peers.reserve(impl_->seenPeers.size());
  for (const auto& peer : impl_->seenPeers) peers.push_back(peer.advertisement);
  return peers;
}
void Transport::DisconnectPeer()
{
  if (impl_->peer != kInvalidSocket) impl_->Disconnect();
}
bool Transport::IsHost() const { return impl_->host; }
uint16_t Transport::LocalPort() const { return impl_->localPort; }

} // namespace reb::native::lan
