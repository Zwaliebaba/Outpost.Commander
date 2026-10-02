#pragma once

namespace Outpost
{
// What the simulation tells a measurement about where a tick's time goes (task 8.1): where each part of the tick begins
// and ends, nested as TickPart says. The simulation never reads a clock (ADR-009): an observer does, and nothing it learns
// reaches the simulation's state.
class TickObserver
{
public:
  virtual ~TickObserver() = default;

  virtual void Begin(TickPart _part) noexcept = 0;
  virtual void End(TickPart _part) noexcept = 0;
};

// One part of a tick, from where this is made to where it goes out of scope; nothing without an observer.
class ObservedPart
{
public:
  ObservedPart(TickObserver* _observer, TickPart _part) noexcept
    : m_observer(_observer),
      m_part(_part)
  {
    if (m_observer != nullptr)
      m_observer->Begin(m_part);
  }

  ObservedPart(const ObservedPart&) = delete;
  ObservedPart& operator=(const ObservedPart&) = delete;
  ObservedPart(ObservedPart&&) = delete;
  ObservedPart& operator=(ObservedPart&&) = delete;

  ~ObservedPart()
  {
    if (m_observer != nullptr)
      m_observer->End(m_part);
  }

private:
  TickObserver* m_observer;
  TickPart m_part;
};

// Times each part of a tick on std::chrono::steady_clock, for the server's measurement (task 8.1). A part that begins
// again before it ends, as a graph built inside another graph's part cannot, restarts its timing.
class TickProfiler final : public TickObserver
{
public:
  void Begin(TickPart _part) noexcept override;
  void End(TickPart _part) noexcept override;

  // How long each part took since the last call, and a fresh start.
  [[nodiscard]] std::array<std::chrono::nanoseconds, TICK_PART_COUNT> Take() noexcept;

private:
  std::array<std::chrono::steady_clock::time_point, TICK_PART_COUNT> m_started{};
  std::array<std::chrono::nanoseconds, TICK_PART_COUNT> m_parts{};
};
} // namespace Outpost
