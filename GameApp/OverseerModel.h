#pragma once

namespace Outpost
{
// One Health, and when the overseer had it on its own clock.
struct HeardHealth
{
  std::chrono::steady_clock::time_point heard;
  HealthMessage health;
};

// What the overseer knows of its universe, from what the server said on its channel (ADR-101, ADR-102): the welcome; the
// newest Health and the last hour of them; the universe log's records, and those its lanes are drawn from over the last day;
// the newest view, whole; and how the server ended, once it has said. It holds no connection, so that a test hands it messages as bytes, and what it knew stays as it was
// while no connection brings more. One thread at a time.
class OverseerModel : Neuron::NonCopyable
{
public:
  // How many Health it keeps, an hour of one a second, and how many of the log's records.
  static constexpr std::size_t HEALTH_KEPT = 3'600;
  static constexpr std::size_t RECORDS_KEPT = 10'000;

  // One message of the overseer's own stream, heard at _now. A welcome starts a connection's knowledge: the views held go,
  // since the server sends a new connection whole ones first. Throws Neuron::Exception for bytes that are not one of
  // the overseer's messages, or a message only an overseer sends.
  void Take(std::span<const std::byte> _message, std::chrono::steady_clock::time_point _now);

  // One view, the game's state message on its stream of its own (ADR-101 decision 6), put together with the views held
  // (ADR-098) and with the welcome's sectors and fields. Returns the tick the overseer now holds, which it tells the server;
  // none for a view that came before its connection's welcome, which crosses on the other stream and carries what it is put
  // together with. Throws
  // Neuron::Exception for a view that is no state, or a change of a view not held.
  [[nodiscard]] std::optional<std::uint64_t> TakeView(std::span<const std::byte> _view);

  // The connection has gone, saying _why, at _now: what the model knew stays, as of then, and no view is taken until the next
  // connection's welcome.
  void Lost(std::string _why, std::chrono::steady_clock::time_point _now);

  [[nodiscard]] const std::optional<OverseerWelcomeMessage>& Welcome() const noexcept
  {
    return m_welcome;
  }

  // The newest Health, if one has come since the first welcome.
  [[nodiscard]] const HeardHealth* LatestHealth() const noexcept
  {
    return m_health.empty() ? nullptr : &m_health.back();
  }

  // The last HEALTH_KEPT of them, oldest first.
  [[nodiscard]] const std::deque<HeardHealth>& Healths() const noexcept
  {
    return m_health;
  }

  // The last RECORDS_KEPT of the log's records, oldest first.
  [[nodiscard]] const std::deque<std::string>& Records() const noexcept
  {
    return m_records;
  }

  // The records the lanes of who played are drawn from, the welcome's and those written since, as KeepLaneRecords keeps them
  // for the day of universe time that ends at the newest tick the overseer was told of, oldest first.
  [[nodiscard]] const std::deque<std::string>& LaneRecords() const noexcept
  {
    return m_lanes;
  }

  // The newest tick the overseer was told of, by a Health or a record.
  [[nodiscard]] std::uint64_t NewestTick() const noexcept
  {
    return m_newestTick;
  }

  // The newest view, whole: full sight, or the seat whose sight the overseer watches.
  [[nodiscard]] const std::optional<Snapshot>& View() const noexcept
  {
    return m_view;
  }

  [[nodiscard]] const std::optional<ClosingMessage>& Closing() const noexcept
  {
    return m_closing;
  }

  // Why the last connection went and when, while no newer one has been welcomed.
  [[nodiscard]] const std::optional<std::string>& LostWhy() const noexcept
  {
    return m_lostWhy;
  }

  [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> LostAt() const noexcept
  {
    return m_lostAt;
  }

private:
  // Lets go of the lanes' records from before the day that ends at the newest tick.
  void KeepTheDay();

  std::optional<OverseerWelcomeMessage> m_welcome;
  // Whether the connection now open has been welcomed, so that its views are taken.
  bool m_welcomed = false;
  // The welcome's sectors and fields, which a view crosses without (TakeCatalog).
  CatalogMessage m_catalog;
  std::deque<HeardHealth> m_health;
  std::deque<std::string> m_records;
  std::deque<std::string> m_lanes;
  std::uint64_t m_newestTick = 0;
  HeldStates m_views;
  std::optional<Snapshot> m_view;
  std::optional<ClosingMessage> m_closing;
  std::optional<std::string> m_lostWhy;
  std::optional<std::chrono::steady_clock::time_point> m_lostAt;
};
} // namespace Outpost
