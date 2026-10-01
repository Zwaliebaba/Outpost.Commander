#pragma once

namespace Outpost
{
// The client's view of the world between snapshots (task 2.5, ADR-013). It keeps the last few snapshots and shows the
// world as it was one tick before the newest, interpolated between the two that bracket that moment, so that ships move
// smoothly at any frame rate from a 20 Hz tick. It never draws server state and never extrapolates: when snapshots stop
// coming, the view holds on the newest (ADR-002 decision 5).
class SnapshotInterpolator
{
public:
  // How far behind the newest snapshot the view runs, in ticks, so that the next snapshot has usually arrived by the time
  // the view reaches it (ADR-013).
  static constexpr double DELAY_TICKS = 1.0;
  // How much of the gap between the view's clock and where it should be is closed each frame, and past what gap the
  // clock jumps rather than catching up.
  static constexpr double CLOCK_CORRECTION = 0.1;
  static constexpr double CLOCK_SNAP_TICKS = 2.0;
  // Snapshots kept, the newest last: enough for a batch of five from one Advance (ADR-009) and the one before it.
  static constexpr size_t HISTORY = 8;

  explicit SnapshotInterpolator(std::uint32_t _ticksPerSecond);

  // Adds a snapshot as it arrives. One no newer than the newest held is ignored.
  void Receive(Snapshot _snapshot);

  // Runs the view's clock on by a frame's wall time, pulled gently toward DELAY_TICKS behind the newest snapshot.
  void Advance(float _elapsedSeconds) noexcept;

  // Nothing has arrived yet.
  [[nodiscard]] bool IsEmpty() const noexcept
  {
    return m_history.empty();
  }

  // The tick the view shows, with a fraction between two snapshots.
  [[nodiscard]] double ViewTick() const noexcept
  {
    return m_viewTick;
  }

  // The newest snapshot; only while not empty.
  [[nodiscard]] const Snapshot& Newest() const noexcept
  {
    return m_history.back();
  }

  // The entities at the view's tick: every entity of the newer of the two bracketing snapshots, placed and turned between
  // where it was in the older and where it is in the newer. An entity new in the newer snapshot is where it is.
  [[nodiscard]] std::vector<EntityView> Entities() const;

private:
  double m_ticksPerSecond = 0.0;
  std::deque<Snapshot> m_history;
  double m_viewTick = 0.0;
  // Wall time since the newest snapshot arrived, in seconds.
  double m_sinceNewestSeconds = 0.0;
  bool m_clockStarted = false;
};

// _from turned toward _to by _fraction the short way round, in radians counterclockwise from +x.
[[nodiscard]] float InterpolateHeading(float _from, float _to, float _fraction) noexcept;
} // namespace Outpost