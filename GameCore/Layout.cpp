// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Layout.h"

namespace Outpost
{

namespace
{
/// The anchor player one gets, and every other anchor is a quarter turn of it. On the negative x axis
/// looking along positive x, so its heading is zero and the arithmetic below adds rather than special
/// cases.
constexpr Neuron::Vec2 BASE_ANCHOR{.x = -ANCHOR_RADIUS, .y = 0};

/// A quarter turn counter-clockwise: `(x, y) -> (-y, x)`. **A SWAP AND A NEGATION, WHICH IS EXACT** --
/// `TechnicalDesign.md` section 3 makes the whole symmetry argument rest on that.
[[nodiscard]] constexpr Neuron::Vec2 QuarterTurn(const Neuron::Vec2& _point) noexcept
{
  return Neuron::Vec2{.x = -_point.y, .y = _point.x};
}

/// How many quarter turns from the base anchor this player's is, or `ANCHOR_COUNT` for a player who has
/// no anchor at all.
[[nodiscard]] constexpr std::size_t QuarterTurnsFor(std::size_t _playerCount, PlayerId _player) noexcept
{
  if ((_player == NO_PLAYER) || (_playerCount == 0) || (static_cast<std::size_t>(_player) > _playerCount))
  {
    return ANCHOR_COUNT;
  }
  if (_playerCount > ANCHOR_COUNT)
  {
    return ANCHOR_COUNT;
  }

  // 4 / count: two quarter turns apart at two players, one at four. Integer division, and the comment in
  // the header says what three does with it.
  const std::size_t step = ANCHOR_COUNT / _playerCount;
  return ((static_cast<std::size_t>(_player) - 1) * step) % ANCHOR_COUNT;
}
} // namespace

Neuron::Vec2 StartAnchor(std::size_t _playerCount, PlayerId _player) noexcept
{
  const std::size_t turns = QuarterTurnsFor(_playerCount, _player);
  if (turns >= ANCHOR_COUNT)
  {
    // THE ORIGIN, WHICH NO ANCHOR IS, so a caller that placed something on a player who has no start
    // gets a position it can recognize rather than one that looks plausible.
    return Neuron::Vec2{};
  }

  Neuron::Vec2 anchor = BASE_ANCHOR;
  for (std::size_t turn = 0; turn < turns; ++turn)
  {
    anchor = QuarterTurn(anchor);
  }
  return anchor;
}

Neuron::Angle StartHeading(std::size_t _playerCount, PlayerId _player) noexcept
{
  const std::size_t turns = QuarterTurnsFor(_playerCount, _player);
  if (turns >= ANCHOR_COUNT)
  {
    return 0;
  }

  // The same quarter turns, on the angle. `Neuron::Angle` wraps at a full turn by construction, so four
  // quarter turns is zero and there is nothing to reduce.
  return static_cast<Neuron::Angle>(static_cast<Neuron::Angle>(turns) * Neuron::ANGLE_QUARTER_TURN);
}

std::vector<Placement> GenerateLayout(std::uint64_t _seed, std::size_t _playerCount)
{
  // THE SEED IS IGNORED AND THE PARAMETER IS NOT DEAD. `GameDesign.md` section 3 runs M0 and M1 on one
  // fixed layout on purpose -- it keeps "is the map wrong or is the game wrong" out of the two milestones
  // that can least afford the question -- and M2 is this function reading it rather than a new one.
  static_cast<void>(_seed);

  const std::size_t players = (_playerCount > ANCHOR_COUNT) ? ANCHOR_COUNT : _playerCount;

  std::vector<Placement> placed;
  placed.reserve(players);
  for (std::size_t index = 0; index < players; ++index)
  {
    const PlayerId player = static_cast<PlayerId>(index + 1);
    placed.push_back(Placement{
      .design = DesignId::Station, .owner = player, .position = StartAnchor(players, player), .heading = StartHeading(players, player)});
  }
  return placed;
}

} // namespace Outpost
