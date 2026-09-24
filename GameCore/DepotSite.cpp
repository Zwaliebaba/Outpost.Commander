// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "DepotSite.h"

#include "Catalog.h"

namespace Outpost
{

namespace
{
[[nodiscard]] std::int64_t DistanceSquared(const Neuron::Vec2& _a, const Neuron::Vec2& _b) noexcept
{
  const std::int64_t dx = static_cast<std::int64_t>(_a.x) - _b.x;
  const std::int64_t dy = static_cast<std::int64_t>(_a.y) - _b.y;
  return (dx * dx) + (dy * dy);
}

[[nodiscard]] std::int64_t Squared(std::int64_t _units) noexcept
{
  const std::int64_t fixed = _units * Neuron::FIXED_ONE;
  return fixed * fixed;
}
} // namespace

bool IsDepot(DesignId _design) noexcept
{
  return (static_cast<std::size_t>(_design) < Designs().size()) && (Design(_design).hull == HullId::DepotFrame);
}

DepotSiteFault CheckDepotSite(std::span<const Neuron::Vec2> _stations, std::span<const Placement> _field,
                              std::span<const Neuron::Vec2> _ownDepots, const Neuron::Vec2& _site, DesignId _design) noexcept
{
  if (!IsDepot(_design))
  {
    return DepotSiteFault::NotADepot;
  }
  if (_ownDepots.size() >= MAXIMUM_DEPOTS_PER_PLAYER)
  {
    return DepotSiteFault::AtCapacity;
  }
  for (const Neuron::Vec2& station : _stations)
  {
    if (DistanceSquared(station, _site) < Squared(DEPOT_MIN_STATION_DISTANCE_UNITS))
    {
      return DepotSiteFault::NearStation;
    }
  }

  bool nearRock = false;
  for (const Placement& rock : _field)
  {
    if (DistanceSquared(rock.position, _site) <= Squared(DEPOT_MAX_ROCK_DISTANCE_UNITS))
    {
      nearRock = true;
      break;
    }
  }
  if (!nearRock)
  {
    return DepotSiteFault::FarFromRock;
  }

  const std::int64_t size = Hull(HullId::DepotFrame).sizeUnits;
  for (const Neuron::Vec2& depot : _ownDepots)
  {
    if (DistanceSquared(depot, _site) < Squared(size))
    {
      return DepotSiteFault::OnDepot;
    }
  }
  return DepotSiteFault::None;
}

} // namespace Outpost
