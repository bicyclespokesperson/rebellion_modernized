#include "port/native/lan_transport.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <span>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{

using reb::native::lan::Status;
using reb::native::lan::Transport;

void Require(bool condition, const char* message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    std::exit(EXIT_FAILURE);
  }
}

bool Until(const std::function<bool()>& condition)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    if (condition()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return condition();
}

void Connect(Transport& host, Transport& guest)
{
  Require(guest.Join("127.0.0.1", host.LocalPort()), "join failed");
  Require(Until([&] {
    host.Poll();
    guest.Poll();
    return host.GetStatus() == Status::Connected && guest.GetStatus() == Status::Connected;
  }), "connection timed out");
}

void CheckTransfer(Transport& sender, Transport& receiver, uint32_t kind,
                   const std::vector<uint8_t>& bytes)
{
  Require(sender.Send(kind, bytes), "send queue rejected message");
  std::optional<reb::native::lan::Message> received;
  Require(Until([&] {
    sender.Poll();
    receiver.Poll();
    received = receiver.TakeMessage();
    return received.has_value();
  }), "message timed out");
  Require(received->kind == kind, "message kind changed");
  Require(received->bytes == bytes, "message bytes changed");
}

void SendMalformedFrame(uint16_t port, uint32_t protocol, uint32_t length)
{
#ifdef _WIN32
  SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  Require(socketHandle != INVALID_SOCKET, "raw socket failed");
#else
  int socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  Require(socketHandle >= 0, "raw socket failed");
#endif
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  Require(connect(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
          "raw connection failed");
  std::array<uint8_t, 16> frame{'R', 'B', 'L', 'N',
                                static_cast<uint8_t>(protocol >> 24),
                                static_cast<uint8_t>(protocol >> 16),
                                static_cast<uint8_t>(protocol >> 8),
                                static_cast<uint8_t>(protocol),
                                0, 0, 0, 1,
                                static_cast<uint8_t>(length >> 24),
                                static_cast<uint8_t>(length >> 16),
                                static_cast<uint8_t>(length >> 8),
                                static_cast<uint8_t>(length)};
  Require(send(socketHandle, reinterpret_cast<const char*>(frame.data()),
               static_cast<int>(frame.size()), 0) == static_cast<int>(frame.size()),
          "raw frame send failed");
  // Keep the socket open until the host has read the invalid header.
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
#ifdef _WIN32
  closesocket(socketHandle);
#else
  close(socketHandle);
#endif
}

} // namespace

int main()
{
  Transport host;
  Require(host.Host("Loopback campaign", 0), "host failed");
  Require(host.IsHost() && host.LocalPort() != 0 && host.GetStatus() == Status::Hosting,
          "host state is wrong");
  const auto localAddresses = reb::native::lan::LocalAddresses();
  Require(!localAddresses.empty(), "no local address was found");
  for (const auto& address : localAddresses) {
    in_addr parsed{};
    Require(inet_pton(AF_INET, address.c_str(), &parsed) == 1, "invalid local IPv4 address");
  }

  Transport browser;
  Require(browser.Discover(host.LocalPort()), "discovery setup failed");
  Require(Until([&] {
    browser.Poll();
    host.Poll();
    browser.Poll();
    const auto found = browser.Peers();
    return found.size() == 1 && found[0].address == "127.0.0.1";
  }), "host advertisement timed out");
  const auto peers = browser.Peers();
  Require(peers.size() == 1 && peers[0].name == "Loopback campaign" &&
          peers[0].address == "127.0.0.1" && peers[0].port == host.LocalPort() &&
          peers[0].protocol == reb::native::lan::kProtocol, "discovery result is wrong");

  Transport guest;
  Connect(host, guest);
  CheckTransfer(guest, host, 7, {});
  CheckTransfer(host, guest, 9, {1, 2, 3, 4});
  const std::array<uint8_t, 3> gracefulBytes{1, 2, 3};
  Require(host.Send(10, gracefulBytes), "graceful send was rejected");
  Require(host.PendingSendBytes() == gracefulBytes.size() + 16, "send queue size is wrong");
  Require(host.Drain(std::chrono::milliseconds(250)), "graceful send did not drain");
  Require(host.PendingSendBytes() == 0, "send queue did not drain");
  Require(Until([&] {
    guest.Poll();
    const auto message = guest.TakeMessage();
    return message && message->kind == 10 && message->bytes == std::vector<uint8_t>{1, 2, 3};
  }), "drained frame did not arrive");
  std::vector<uint8_t> large(2 * 1024 * 1024);
  for (size_t index = 0; index < large.size(); ++index) large[index] = static_cast<uint8_t>(index * 37);
  CheckTransfer(guest, host, 11, large);

  Transport extra;
  Require(extra.Join("127.0.0.1", host.LocalPort()), "second guest connect failed");
  Require(Until([&] {
    extra.Poll();
    host.Poll();
    return extra.GetStatus() == Status::Disconnected || extra.GetStatus() == Status::Error;
  }), "second guest was not rejected");
  Require(host.GetStatus() == Status::Connected, "second guest displaced first guest");

  guest.Close();
  Require(Until([&] { host.Poll(); return host.GetStatus() == Status::Disconnected; }),
          "host did not notice disconnection");
  Connect(host, guest);
  CheckTransfer(host, guest, 13, {5, 6});
  Require(host.Send(14, gracefulBytes) && host.Drain(std::chrono::milliseconds(250)),
          "final frame did not drain");
  host.DisconnectPeer();
  Require(host.GetStatus() == Status::Disconnected && host.IsHost(),
          "peer disconnect closed the host listener");
  Require(Until([&] { guest.Poll(); return guest.GetStatus() == Status::Disconnected; }),
          "guest did not notice host peer disconnect");
  const auto finalMessage = guest.TakeMessage();
  Require(finalMessage && finalMessage->kind == 14 && finalMessage->bytes ==
          std::vector<uint8_t>{1, 2, 3}, "complete frame was lost on EOF");
  Connect(host, guest);
  CheckTransfer(guest, host, 15, {7});
  guest.Close();
  Require(Until([&] { host.Poll(); return host.GetStatus() == Status::Disconnected; }),
          "host did not return to listening state");

  SendMalformedFrame(host.LocalPort(), reb::native::lan::kProtocol + 1, 0);
  Require(Until([&] { host.Poll(); return host.GetStatus() == Status::Error; }),
          "wrong protocol was accepted");
  Require(!host.Error().empty(), "protocol error was not reported");
  SendMalformedFrame(host.LocalPort(), reb::native::lan::kProtocol,
                     reb::native::lan::kMaxMessageSize + 1);
  Require(Until([&] {
    host.Poll();
    return host.GetStatus() == Status::Error;
  }), "oversized frame was accepted");
  Connect(host, guest);
  Require(host.Error().empty(), "reconnect did not clear error");
  host.Close();
  Require(host.GetStatus() == Status::Idle && host.LocalPort() == 0, "close did not reset host");
}
