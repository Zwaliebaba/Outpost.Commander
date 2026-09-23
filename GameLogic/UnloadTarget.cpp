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

Neuron::Fixed UnloadReach(DesignId _miner, DesignId _acceptor) noexcept
{
  const std::int32_t minerUnits = Hull(Design(_miner).hull).sizeUnits;
  const std::int32_t acceptorUnits = Hull(Design(_acceptor).hull).sizeUnits;
  return static_cast<Neuron::Fixed>((((minerUnits + acceptorUnits) / 2) + UNLOAD_SLACK_UNITS) * Neuron::FIXED_ONE);
}

} // namespace Outpost
