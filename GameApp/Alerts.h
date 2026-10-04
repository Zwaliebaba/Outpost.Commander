#pragma once

namespace Outpost
{
// Phase 2 design §9's alerts (ADR-059): what the player should know of a front it is not looking at, made from its own
// snapshots as they arrive, as any client could. One of its Relays suppressed, or hit by an enemy shot; one of its Mining
// Rigs destroyed; enemy warships seen in a sector it holds that held none it saw the snapshot before. An alert of one
// kind in one sector is not repeated within REPEAT_SECONDS. The HUD lists the alerts of the last SHOWN_SECONDS, newest
// first, and marks them on the minimap; a key moves the camera to the newest.
class Alerts
{
public:
  static constexpr std::uint32_t SHOWN_SECONDS = 8;
  static constexpr std::uint32_t REPEAT_SECONDS = 20;
  // The alerts listed at most at once.
  static constexpr std::size_t SHOWN_ALERTS = 4;

  enum class Kind : std::uint8_t
  {
    RelaySuppressed,
    RelayAttacked,
    RigLost,
    EnemyEntered
  };

  struct Alert
  {
    Kind kind = Kind::RelaySuppressed;
    std::string text;
    PlanePosition position;
    std::int32_t sector = 0;
    std::uint64_t tick = 0;
  };

  // Starts over, for a new match.
  void Reset() noexcept;

  // Takes the player's next snapshot; every one is needed, in order, since a shot and a destruction are in the snapshot of
  // their tick only. _ticksPerSecond is the server's rate.
  void Observe(const Snapshot& _snapshot, std::uint32_t _ticksPerSecond);

  // The alerts of the last SHOWN_SECONDS before _tick, newest first, at most SHOWN_ALERTS.
  [[nodiscard]] std::vector<Alert> Shown(std::uint64_t _tick, std::uint32_t _ticksPerSecond) const;

  // The newest alert, if any, for the key that moves the camera to it.
  [[nodiscard]] const Alert* Newest() const noexcept
  {
    return m_alerts.empty() ? nullptr : &m_alerts.back();
  }

private:
  void Raise(Kind _kind, std::string _text, PlanePosition _position, std::int32_t _sector, std::uint64_t _tick,
             std::uint32_t _ticksPerSecond);

  // Every alert raised, oldest first, trimmed to the last few.
  std::vector<Alert> m_alerts;
  // What the last snapshot showed: the sectors the player held that were suppressed, and the sectors it held that had an
  // enemy warship it saw in them.
  std::vector<std::int32_t> m_suppressed;
  std::vector<std::int32_t> m_entered;
};
} // namespace Outpost
