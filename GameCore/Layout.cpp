// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Layout.h"

#include <array>

namespace Outpost
{

namespace
{
/// The anchor player one gets, and every other anchor is a quarter turn of it. On the negative x axis
/// looking along positive x, so its heading is zero and the arithmetic below adds rather than special
/// cases.
constexpr Neuron::Vec2 BASE_ANCHOR{.x = -ANCHOR_RADIUS, .y = 0};

/// How many quarter turns from the base anchor this player's is, or `ANCHOR_COUNT` for a player who has
/// no anchor at all.
[[nodiscard]] constexpr std::size_t QuarterTurnsFor(std::size_t _playerCount, PlayerId _player) noexcept
{
  if ((_player == NO_PLAYER) || (_playerCount == 0) || (static_cast<std::size_t>(_player) > _playerCount))
  {
    return ANCHOR_COUNT;
  }

  // 4 / count: two quarter turns apart at two players, one at four. Integer division, and the comment in
  // the header says what three does with it.
  const std::size_t step = ANCHOR_COUNT / _playerCount;
  return ((static_cast<std::size_t>(_player) - 1) * step) % ANCHOR_COUNT;
}

/// True when this count takes ADR-023's stress layout rather than the quarter-turn anchors.
[[nodiscard]] constexpr bool IsStressCount(std::size_t _playerCount) noexcept
{
  return _playerCount > ANCHOR_COUNT;
}

/// Where player _player of a stress count sits around the circle, as a binary angle from the base anchor.
/// Exact integer division of a full turn, so every player's angle is the same number on every machine.
[[nodiscard]] constexpr Neuron::Angle StressAngle(std::size_t _playerCount, PlayerId _player) noexcept
{
  return static_cast<Neuron::Angle>(((static_cast<std::uint32_t>(_player) - 1) * 65536u) / static_cast<std::uint32_t>(_playerCount));
}

/// True for a player a count seats at all.
[[nodiscard]] constexpr bool IsSeated(std::size_t _playerCount, PlayerId _player) noexcept
{
  return (_player != NO_PLAYER) && (static_cast<std::size_t>(_player) <= _playerCount);
}
} // namespace

Neuron::Vec2 StartAnchor(std::size_t _playerCount, PlayerId _player) noexcept
{
  if (IsStressCount(_playerCount))
  {
    if (!IsSeated(_playerCount, _player))
    {
      return Neuron::Vec2{};
    }

    // The base anchor, (-R, 0), turned by the player's angle: (-R cos, -R sin). Q1.15 over 32,768 in 64
    // bits, so a radius of six thousand units cannot overflow on the way.
    const Neuron::Angle angle = StressAngle(_playerCount, _player);
    const std::int64_t radius = static_cast<std::int64_t>(ANCHOR_RADIUS);
    return Neuron::Vec2{.x = static_cast<Neuron::Fixed>(-(radius * Neuron::Cosine(angle)) / 32768),
                        .y = static_cast<Neuron::Fixed>(-(radius * Neuron::Sine(angle)) / 32768)};
  }

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
  // FACING THE CENTER, as the quarter turns do: the base anchor faces heading zero, and turning the anchor
  // by an angle turns its heading by the same angle.
  if (IsStressCount(_playerCount))
  {
    return IsSeated(_playerCount, _player) ? StressAngle(_playerCount, _player) : Neuron::Angle{0};
  }

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

  // Up to every `PlayerId` there is. Above four is ADR-023's stress layout; whether a count that large is
  // allowed at all is the host's decision, made before it gets here.
  const std::size_t players = (_playerCount > MAX_PLAYERS) ? MAX_PLAYERS : _playerCount;

  std::vector<Placement> placed;
  placed.reserve(players * 4);
  for (std::size_t index = 0; index < players; ++index)
  {
    const PlayerId player = static_cast<PlayerId>(index + 1);
    placed.push_back(Placement{
      .design = DesignId::Station, .owner = player, .position = StartAnchor(players, player), .heading = StartHeading(players, player)});
  }

  // **THE STARTING SHIPS** (Q84): toward the center along the station's heading, through the pinned sine table, so
  // both sides compute them to the unit (R16). Across that line a quarter turn on, Miner, Fighter, Miner.
  constexpr std::array<DesignId, 3> STARTING{DesignId::Miner, DesignId::Fighter, DesignId::Miner};
  for (std::size_t index = 0; index < players; ++index)
  {
    const PlayerId player = static_cast<PlayerId>(index + 1);
    const Neuron::Vec2 anchor = StartAnchor(players, player);
    const Neuron::Angle heading = StartHeading(players, player);
    const std::int64_t forwardX = Neuron::Cosine(heading);
    const std::int64_t forwardY = Neuron::Sine(heading);
    for (std::size_t ship = 0; ship < STARTING.size(); ++ship)
    {
      const std::int64_t across = (static_cast<std::int64_t>(ship) - 1) * STARTING_SHIPS_SPACING_UNITS;
      const std::int64_t along = STARTING_SHIPS_DISTANCE_UNITS;
      // Along the heading, and across it a quarter turn anticlockwise: (-sin, cos).
      const std::int64_t offsetX = ((forwardX * along) - (forwardY * across)) * Neuron::FIXED_ONE / Neuron::SINE_ONE;
      const std::int64_t offsetY = ((forwardY * along) + (forwardX * across)) * Neuron::FIXED_ONE / Neuron::SINE_ONE;
      placed.push_back(Placement{
        .kind = PlacedKind::Ship,
        .design = STARTING[ship],
        .owner = player,
        .position = Neuron::Vec2{.x = static_cast<Neuron::Fixed>(anchor.x + offsetX), .y = static_cast<Neuron::Fixed>(anchor.y + offsetY)},
        .heading = heading});
    }
  }
  return placed;
}

} // namespace Outpost
