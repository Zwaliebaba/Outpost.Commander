#pragma once

namespace Outpost
{
// How an entity moved between the two snapshots around the view's tick: how fast across the ground, and how fast its
// heading turned, counterclockwise from above positive. The server sends only positions and headings, so this is the
// client's reading of them, for presentation such as banking (ADR-029).
struct EntityMotion
{
  EntityId id;
  float speedMetersPerSecond = 0.0f;
  float turnRadiansPerSecond = 0.0f;
};

// The client's view of the simulation between snapshots (ADR-013 decision 3). It keeps the last few snapshots and shows the
// simulation as it was a delay behind the server's clock, interpolated between the two that bracket that moment, so that
// ships move smoothly at any frame rate from a 20 Hz tick. The delay follows how late the snapshots arrive. It never draws
// server state and never extrapolates: when snapshots stop coming, the view holds on the newest (ADR-002 decision 5).
class SnapshotInterpolator
{
public:
  // How far behind the server's clock the view runs, in ticks: a tick, plus how late snapshots have arrived over the
  // window at the percentile, so that the next snapshot has usually arrived by the time the view reaches it; no more than
  // the most (ADR-013 decision 3).
  static constexpr double LEAST_DELAY_TICKS = 1.0;
  static constexpr double MOST_DELAY_TICKS = 4.0;
  static constexpr double LATENESS_WINDOW_SECONDS = 10.0;
  static constexpr std::size_t LATENESS_PERCENT = 95;
  // How much of a snapshot's lateness the server's clock follows, which an earlier snapshot sets back at once.
  static constexpr double CLOCK_FOLLOW = 0.2;
  // How much of the gap between the view and where it should be is closed each frame, and how much faster than the ticks
  // the view runs at most while it catches up, so that a burst after a stall plays on rather than jumps.
  static constexpr double CLOCK_CORRECTION = 0.1;
  static constexpr double CATCH_UP_PACE = 3.0;
  // Snapshots kept, the newest last: at least the fewest, and more while the view still needs them, up to the most.
  static constexpr std::size_t HISTORY = 8;
  static constexpr std::size_t MOST_HISTORY = 64;

  explicit SnapshotInterpolator(std::uint32_t _ticksPerSecond);

  // Adds a snapshot as it arrives, and takes how late it is. One no newer than the newest held is ignored.
  void Receive(Snapshot _snapshot);

  // Runs the view's clock on by a frame's wall time, pulled gently toward the delay behind the server's clock.
  void Advance(float _elapsedSeconds) noexcept;

  // How far behind the server's clock the view runs, in ticks.
  [[nodiscard]] double DelayTicks() const noexcept
  {
    return m_delayTicks;
  }

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
  // The same into _into, whose storage is kept, so that a caller asking every frame does not allocate every frame.
  void Entities(std::vector<EntityView>& _into) const;

  // How each entity of both bracketing snapshots moved from the older to the newer, in identifier order; the same for
  // every view tick between them. None while the view is at either end of what is kept, as it is when snapshots stop
  // coming: the view does not extrapolate, so nothing is moving.
  [[nodiscard]] std::vector<EntityMotion> Motions() const;

private:
  // The first snapshot past the view's tick, or the end when there is none.
  [[nodiscard]] std::deque<Snapshot>::const_iterator NewerThanView() const;

  double m_ticksPerSecond = 0.0;
  std::deque<Snapshot> m_history;
  double m_viewTick = 0.0;
  bool m_clockStarted = false;
  // The frames' wall time since the view was made, in seconds.
  double m_seconds = 0.0;
  // The server's clock, as the wall time less the server's tick in seconds, once a snapshot has arrived.
  std::optional<double> m_offsetSeconds;
  // When each snapshot of the window arrived, and how late it was, in seconds; and the scratch the percentile is taken in.
  std::deque<std::pair<double, double>> m_lateness;
  std::vector<double> m_latenessSorted;
  double m_delayTicks = LEAST_DELAY_TICKS;
};

// _from turned toward _to by _fraction the short way round, in radians counterclockwise from +x.
[[nodiscard]] float InterpolateHeading(float _from, float _to, float _fraction) noexcept;
} // namespace Outpost