#pragma once

namespace Outpost
{
// How far a ship leans into the turn it is making now (ADR-029): in proportion to its sideways acceleration, its speed
// times how fast it turns, up to its limit, positive into a counterclockwise turn. A ship flying straight, or turning on
// the spot, does not lean.
[[nodiscard]] float TargetBankRadians(const EntityMotion& _motion, const BankLimits& _limits) noexcept;

// Each ship's bank as it is drawn (ADR-029). A ship's target bank changes with each snapshot, twenty times a second, so
// the drawn bank follows it through a critically damped spring rather than jumping. The spring is solved exactly over a
// frame, so the bank does not depend on the frame rate.
class ShipBanking
{
public:
  // How fast the spring settles: a step is about nine tenths followed after one settle time.
  static constexpr float SETTLE_RATE = 4.0f;

  // A ship's bank to follow this frame, and how quickly.
  struct Target
  {
    EntityId id;
    float bankRadians = 0.0f;
    float settleSeconds = 0.0f;
  };

  // Moves each ship's bank on by a frame toward its target. A ship with no target is forgotten, so one that leaves the
  // view and comes back starts level.
  void Update(std::span<const Target> _targets, float _elapsedSeconds);

  // The ship's bank as it is drawn now; level for a ship it does not know.
  [[nodiscard]] float BankRadians(EntityId _ship) const noexcept;

  void Clear() noexcept
  {
    m_ships.clear();
  }

private:
  struct Ship
  {
    EntityId id;
    float bankRadians = 0.0f;
    float radiansPerSecond = 0.0f;
  };

  // In identifier order.
  std::vector<Ship> m_ships;
};
} // namespace Outpost
