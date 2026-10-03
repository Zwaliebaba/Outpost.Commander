#include "pch.h"
#include "ShipBanking.h"

#include <algorithm>
#include <cmath>

namespace
{
// A settle time shorter than this would make the spring snap within a frame, which is the jump it is there to avoid.
constexpr float MINIMUM_SETTLE_SECONDS = 0.01f;
} // namespace

float Outpost::TargetBankRadians(const EntityMotion& _motion, const BankLimits& _limits) noexcept
{
  if (_limits.maxBankRadians <= 0.0f || _limits.fullBankMetersPerSecondSquared <= 0.0f)
    return 0.0f;
  const float sideways = _motion.speedMetersPerSecond * _motion.turnRadiansPerSecond;
  return std::clamp(sideways / _limits.fullBankMetersPerSecondSquared, -1.0f, 1.0f) * _limits.maxBankRadians;
}

void Outpost::ShipBanking::Update(std::span<const Target> _targets, float _elapsedSeconds)
{
  const float seconds = std::max(_elapsedSeconds, 0.0f);
  std::vector<Ship> ships;
  ships.reserve(_targets.size());
  for (const Target& target : _targets)
  {
    Ship ship{.id = target.id};
    if (const auto known = std::ranges::lower_bound(m_ships, target.id, {}, &Ship::id); known != m_ships.end() && known->id == target.id)
      ship = *known;

    // A critically damped spring toward the target, solved over the frame: x(t) = target + (c1 + c2 t) e^(-w t).
    const float rate = SETTLE_RATE / std::max(target.settleSeconds, MINIMUM_SETTLE_SECONDS);
    const float offset = ship.bankRadians - target.bankRadians;
    const float drift = ship.radiansPerSecond + (rate * offset);
    const float decay = std::exp(-rate * seconds);
    const float remaining = offset + (drift * seconds);
    ship.bankRadians = target.bankRadians + (remaining * decay);
    ship.radiansPerSecond = (drift - (rate * remaining)) * decay;
    ships.push_back(ship);
  }
  if (!std::ranges::is_sorted(ships, {}, &Ship::id))
    std::ranges::sort(ships, {}, &Ship::id);
  m_ships = std::move(ships);
}

float Outpost::ShipBanking::BankRadians(EntityId _ship) const noexcept
{
  const auto found = std::ranges::lower_bound(m_ships, _ship, {}, &Ship::id);
  return found != m_ships.end() && found->id == _ship ? found->bankRadians : 0.0f;
}
