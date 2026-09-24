#include "pch.h"

#include "UnloadTarget.h"

namespace Outpost
{

namespace
{
/// Past every corner of the square from every point in it: the diagonal is under 2.9 half extents, so three
/// reaches everything and cannot overflow once squared in 64 bits.
constexpr Neuron::Fixed WHOLE_MAP_RADIUS = 3 * PLAY_AREA_HALF_EXTENT;
} // namespace

EntityId FindUnloadTarget(const World& _world, const UniformGrid& _grid, PlayerId _player, const Neuron::Vec2& _from,
                          std::vector<EntityId>& _scratch)
{
  return _grid.Nearest(
    _world, _from, WHOLE_MAP_RADIUS,
    [_player](const Entity& _entity) noexcept { return (_entity.owner == _player) && Derive(_entity.design).acceptsOre; }, _scratch);
}

std::int32_t ThreatRadiusUnits() noexcept
{
  return HOME_FIELD_OUTER_RADIUS_UNITS + static_cast<std::int32_t>(Component(ComponentId::MassDriver).rangeUnits);
}

bool FarSideUnloadPoint(const World& _world, const UniformGrid& _grid, const Entity& _acceptor, DesignId _miner,
                        std::vector<EntityId>& _scratch, Neuron::Vec2& _outPoint)
{
  const PlayerId owner = _acceptor.owner;
  const EntityId threat = _grid.Nearest(
    _world, _acceptor.position, static_cast<Neuron::Fixed>(ThreatRadiusUnits() * Neuron::FIXED_ONE),
    [owner](const Entity& _entity) noexcept { return (_entity.owner != NO_PLAYER) && (_entity.owner != owner); }, _scratch);
  const Entity* hostile = _world.Find(threat);
  if (hostile == nullptr)
  {
    return false;
  }

  // AWAY FROM IT: the bearing from the hostile to the acceptor, carried on past the acceptor's center by the reach.
  const Neuron::Vec2 away = _acceptor.position - hostile->position;
  const Neuron::Angle bearing = Neuron::BearingOf(away.x, away.y);
  const std::int64_t reach = UnloadReach(_miner, _acceptor.design);
  _outPoint = Neuron::Vec2{.x = static_cast<Neuron::Fixed>(static_cast<std::int64_t>(_acceptor.position.x) +
                                                           ((Neuron::Cosine(bearing) * reach) / Neuron::SINE_ONE)),
                           .y = static_cast<Neuron::Fixed>(static_cast<std::int64_t>(_acceptor.position.y) +
                                                           ((Neuron::Sine(bearing) * reach) / Neuron::SINE_ONE))};
  return true;
}

Neuron::Fixed UnloadReach(DesignId _miner, DesignId _acceptor) noexcept
{
  const std::int32_t minerUnits = Hull(Design(_miner).hull).sizeUnits;
  const std::int32_t acceptorUnits = Hull(Design(_acceptor).hull).sizeUnits;
  return static_cast<Neuron::Fixed>((((minerUnits + acceptorUnits) / 2) + UNLOAD_SLACK_UNITS) * Neuron::FIXED_ONE);
}

} // namespace Outpost
