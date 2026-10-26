#pragma once

namespace GameLogicTests
{
// One way of an impaired link: how long a datagram takes, how much sooner or later than that, at random, the share of
// datagrams lost, and the most it carries a second. A datagram never overtakes one sent before it, so one that is late holds
// back the ones behind it.
struct LinkWay
{
  std::chrono::microseconds delay{};
  std::chrono::microseconds jitter{};
  std::uint32_t lossPartsPerMillion = 0;
  // None for no limit. At a limit, a datagram waits for the ones before it to go, and one that would wait longer than
  // ImpairedLink::QUEUE_LIMIT is dropped, as a full queue drops it.
  std::uint64_t bitsPerSecond = 0;
};

// A link both ways.
struct LinkProfile
{
  std::string_view name;
  LinkWay toServer;
  LinkWay toClient;
};

// The links the network is measured over, each way half the round trip, the jitter and the loss (owner, 2026-10-26): near,
// a 20 ms round trip, ±5 ms and 0.1%; Wi-Fi, 40 ms, ±15 ms and 1%; and a phone's hotspot, 80 ms, ±30 ms and 2%, which carries
// 4 Mbit/s to the client. A clear link carries everything at once.
inline constexpr LinkProfile CLEAR_LINK{.name = "clear"};
inline constexpr LinkWay NEAR_WAY{
  .delay = std::chrono::milliseconds{10}, .jitter = std::chrono::milliseconds{5}, .lossPartsPerMillion = 1'000};
inline constexpr LinkWay WI_FI_WAY{
  .delay = std::chrono::milliseconds{20}, .jitter = std::chrono::milliseconds{15}, .lossPartsPerMillion = 10'000};
inline constexpr LinkWay HOTSPOT_WAY{
  .delay = std::chrono::milliseconds{40}, .jitter = std::chrono::milliseconds{30}, .lossPartsPerMillion = 20'000};
inline constexpr LinkProfile NEAR_LINK{.name = "near", .toServer = NEAR_WAY, .toClient = NEAR_WAY};
inline constexpr LinkProfile WI_FI_LINK{.name = "Wi-Fi", .toServer = WI_FI_WAY, .toClient = WI_FI_WAY};
inline constexpr LinkProfile HOTSPOT_LINK{.name = "hotspot",
                                          .toServer = HOTSPOT_WAY,
                                          .toClient = {.delay = HOTSPOT_WAY.delay,
                                                       .jitter = HOTSPOT_WAY.jitter,
                                                       .lossPartsPerMillion = HOTSPOT_WAY.lossPartsPerMillion,
                                                       .bitsPerSecond = 4'000'000}};
inline constexpr std::array<LinkProfile, 3> IMPAIRED_LINKS{NEAR_LINK, WI_FI_LINK, HOTSPOT_LINK};

// A UDP relay on the loopback between one client and a server's listener, which impairs each way's datagrams as its profile
// says, its draws from a seed. It is the test project's own helper (ADR-089 decision 3), written with Winsock (R14), and
// runs on a thread of its own. The client is wherever the last datagram on its side came from, so a client that connects
// again through it is carried too.
class ImpairedLink : Neuron::NonCopyable
{
public:
  // How long a datagram may wait behind the ones before it at a way's limit before it is dropped instead.
  static constexpr std::chrono::milliseconds QUEUE_LIMIT{200};

  // What a way did with the datagrams it was given: carried them, lost them by its profile, dropped them from a full queue
  // or a full socket, or dropped them while the link was cut.
  struct Counts
  {
    std::uint64_t carried = 0;
    std::uint64_t lost = 0;
    std::uint64_t dropped = 0;
    std::uint64_t cut = 0;
  };

  // Relays to a server listening on 127.0.0.1 at _serverPort. Throws Neuron::Exception when it cannot open its sockets.
  ImpairedLink(std::uint16_t _serverPort, const LinkProfile& _profile, std::uint64_t _seed);
  ~ImpairedLink();

  // The loopback port a client sends to in place of the server's.
  [[nodiscard]] std::uint16_t Port() const noexcept;

  // _seat, reached through the link.
  [[nodiscard]] Outpost::ServerAddress Through(Outpost::ServerAddress _seat) const;

  // While cut, the link carries nothing either way, as a network that has gone.
  void Cut(bool _cut) noexcept;

  [[nodiscard]] Counts ToServer() const noexcept;
  [[nodiscard]] Counts ToClient() const noexcept;

  struct State;

private:
  // Reads both sockets, and sends each datagram when it is due, until asked to stop.
  void Run(const std::stop_token& _stop);

  std::unique_ptr<State> m_state;
  // Last, so that it is joined before the state it uses goes.
  std::jthread m_thread;
};
} // namespace GameLogicTests
