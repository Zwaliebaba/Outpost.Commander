#pragma once

namespace Outpost
{
// The alerts (ADR-059 decision 1): what the player should know of a front it is not looking at, read from the events
// the server raises in its snapshots (ADR-080), which are what its scheduled orders fire on too. One of its Relays
// suppressed, or hit by an enemy shot; one of its Mining Rigs destroyed; enemy warships seen entering a sector it holds;
// one of its ships going back to be repaired, and a sector the pirates guarded cleared of them; and one of its scheduled
// orders fired (ADR-080 decision 5); and a command of its own the server refused, in words (ADR-094). An alert of one kind
// in one sector is not repeated within REPEAT_SECONDS, but for an order fired, each of which is told, and a refusal, which
// takes the place of an older one of the same words. The HUD lists the alerts of the last SHOWN_SECONDS, newest first, and
// marks on the minimap each that has a place, which a refusal has not; a key moves the camera to the newest that has one.
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
    EnemyEntered,
    ShipRetreating,
    PiratesCleared,
    OrderFired,
    Refused
  };

  struct Alert
  {
    Kind kind = Kind::RelaySuppressed;
    std::string text;
    // Where it happened, if it happened anywhere.
    std::optional<PlanePosition> position;
    std::int32_t sector = 0;
    std::uint64_t tick = 0;
  };

  // Starts over, for a new match.
  void Reset() noexcept;

  // Takes the player's next snapshot; every one is needed, in order, since an event is in the snapshot of its tick only.
  // _ticksPerSecond is the server's rate.
  void Observe(const Snapshot& _snapshot, std::uint32_t _ticksPerSecond);

  // The alerts of the last SHOWN_SECONDS before _tick, newest first, at most SHOWN_ALERTS.
  [[nodiscard]] std::vector<Alert> Shown(std::uint64_t _tick, std::uint32_t _ticksPerSecond) const;

  // The newest alert that has a place, if any, for the key that moves the camera to it.
  [[nodiscard]] const Alert* Newest() const noexcept
  {
    const auto placed = std::ranges::find_last_if(m_alerts, [](const Alert& _alert) { return _alert.position.has_value(); });
    return placed.empty() ? nullptr : &placed.front();
  }

private:
  void Raise(Kind _kind, std::string _text, std::optional<PlanePosition> _position, std::int32_t _sector, std::uint64_t _tick,
             std::uint32_t _ticksPerSecond);

  // Every alert raised, oldest first, trimmed to the last few.
  std::vector<Alert> m_alerts;
};

// Why the server refused a command, in the player's words: "Not enough Ore" (ADR-094).
[[nodiscard]] std::string_view DescribeRefusal(CommandResult _refusal) noexcept;

// A scheduled order's action in words, _where naming where it goes: "attack-move to North" (ADR-080).
[[nodiscard]] std::string DescribeScheduledAction(ScheduledActionKind _action, std::string_view _where);

// A scheduled order that fired, as its event tells it, _where naming where it went: "Order fired: attack-move to North", or
// held back by its condition, or refused (ADR-080).
[[nodiscard]] std::string DescribeFiredOrder(const EventView& _event, std::string_view _where);
} // namespace Outpost
