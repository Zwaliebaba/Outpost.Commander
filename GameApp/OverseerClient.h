#pragma once

namespace Outpost
{
// The overseer's end of its channel (ADR-101, ADR-103): it connects to the server its Overseer.json names, pinning the
// certificate the file names, says hello with the token, and hands what the server says to its model. It tells the server
// the views it holds and the sight it watches, and joins again every REJOIN_INTERVAL while it has no connection. A
// connection is made on a thread of its own, so that the frame loop goes on drawing meanwhile. One thread at a time: the
// frame loop's.
class OverseerClient : Neuron::NonCopyable
{
public:
  // How long after a connection went, or failed to be made, the next is tried; and how long each is given to be made, as the
  // game gives a lost universe's joins (ADR-078 decision 10).
  static constexpr std::chrono::seconds REJOIN_INTERVAL{2};
  // How often the connection's round trip is read.
  static constexpr std::chrono::seconds STATISTICS_INTERVAL{1};

  // Makes no connection yet: the first Update does.
  explicit OverseerClient(OverseerTicket _ticket);
  // Closes the connection; waits for one being made, which gives up within REJOIN_INTERVAL.
  ~OverseerClient();

  // Once a frame, at _now: takes what arrived, tells the server the views it now holds, and joins again when it is due.
  void Update(std::chrono::steady_clock::time_point _now);

  // The sight the views show from here on: full sight with no player, or that seat's. It is told the server now, and again
  // as each new connection is welcomed.
  void Watch(PlayerId _seat);

  [[nodiscard]] const OverseerModel& Model() const noexcept
  {
    return m_model;
  }

  [[nodiscard]] const OverseerTicket& Ticket() const noexcept
  {
    return m_ticket;
  }

  [[nodiscard]] PlayerId Sight() const noexcept
  {
    return m_sight;
  }

  // Whether a connection is open now, and whether one is being made.
  [[nodiscard]] bool Connected() const noexcept
  {
    return m_channel != nullptr;
  }

  [[nodiscard]] bool Connecting() const noexcept
  {
    return m_connecting.valid();
  }

  // The connection's smoothed round trip, as last read; none without a connection.
  [[nodiscard]] std::optional<std::chrono::microseconds> RoundTrip() const noexcept
  {
    return m_roundTrip;
  }

private:
  // The connection has gone or broken: the model says why, and the next join is due after REJOIN_INTERVAL.
  void Drop(std::string _why, std::chrono::steady_clock::time_point _now);

  OverseerTicket m_ticket;
  OverseerModel m_model;
  std::unique_ptr<Neuron::QuicChannel> m_channel;
  std::future<std::unique_ptr<Neuron::QuicChannel>> m_connecting;
  std::chrono::steady_clock::time_point m_nextJoin{};
  std::chrono::steady_clock::time_point m_nextStatistics{};
  std::optional<std::chrono::microseconds> m_roundTrip;
  PlayerId m_sight;
};
} // namespace Outpost
