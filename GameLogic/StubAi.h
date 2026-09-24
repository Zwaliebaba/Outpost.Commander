#pragma once

#include "BuildSystem.h"
#include "CommandIntake.h"
#include "World.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// **ONCE A SECOND** (`OpenQuestions.md` Q48): a player taps at about that rate, and an AI that reacted every tick
/// would win raids on reflexes no human has.
inline constexpr std::uint32_t AI_DECISION_INTERVAL_TICKS = 20;

/// Miners the stub keeps before it builds anything else: one for each of the six nearest home rocks (Q62).
inline constexpr std::size_t AI_MINERS = 6;

/// Fighters the stub gathers before it goes for the nearest enemy station.
inline constexpr std::size_t AI_STRIKE_FIGHTERS = 8;

/// **HOW CLOSE AN ENEMY SHIP MUST COME TO THE STATION FOR THE STUB TO DEFEND**: the home field's outer edge, so a
/// raid on its miners is answered and a fleet passing across the map is not.
inline constexpr std::int32_t AI_DEFEND_RADIUS_UNITS = 2000;

/// **WHAT AN AI SEES** (Q48): exactly what a client of its seat is sent -- every live entity as a wire record, and
/// its own player block -- plus the field, which a client derives from the seed. Nothing else is in it, so the
/// no-cheating rule of `GameDesign.md` section 8 is kept by construction rather than by discipline.
///
/// R8: a public aggregate.
struct AiView
{
  PlayerId player = NO_PLAYER;
  std::span<const EntityRecord> entities;
  PlayerBlock own{};
  std::span<const Placement> field;
};

/// **THE STUB AI** (M3.10, `GameDesign.md` section 10): enough opponent that one person can play a match, and no
/// more. It builds miners and sends each to one of the nearest rocks, then builds fighters. It defends its home
/// field when an enemy ship comes inside `AI_DEFEND_RADIUS_UNITS`, and otherwise, with `AI_STRIKE_FIGHTERS` or more,
/// attacks the nearest enemy station. **Section 8's proper state machine is M4's.**
///
/// **IT ORDERS AS A PLAYER DOES**: `Decide` turns a view into commands, and the host applies them through
/// `CommandIntake::Apply`, which validates them like any client's. It keeps a memory of what it has ordered, and
/// that memory is reset with the match. Integers throughout and no clock (R16).
class StubAi
{
public:
  /// A new match: forgets everything it had ordered.
  void Begin() noexcept;

  /// One decision for _view's player, into _outCommands (cleared first).
  void Decide(const AiView& _view, std::vector<Command>& _outCommands);

private:
  [[nodiscard]] std::uint16_t NextSequence() noexcept
  {
    return m_nextSequence++;
  }

  /// Miners already sent to a rock, by wire identity.
  std::vector<WireIdentity> m_orderedMiners;

  /// The fighters and the target of the last attack it ordered, so an unchanged order is not sent again.
  std::vector<WireIdentity> m_attackers;
  WireIdentity m_attackTarget = NO_WIRE_IDENTITY;

  std::uint16_t m_nextSequence = 1;
};

/// **THE HOST'S AI SEATS** (M3.10): which players are the stub's, one `StubAi` each, and the once-a-second cadence.
/// The host builds each seat's view from `RecordOf` and `PlayerBlockFor`, as it would for a client.
class AiSeats
{
public:
  /// _players is the match's seat count; the last _aiSeats of them are the AI's.
  void Begin(std::size_t _players, std::size_t _aiSeats) noexcept;

  [[nodiscard]] bool IsAi(PlayerId _player) const noexcept;

  [[nodiscard]] std::size_t Count() const noexcept
  {
    return m_count;
  }

  /// Every `AI_DECISION_INTERVAL_TICKS`, each AI seat decides over its view and its orders go through _intake.
  void Advance(World& _world, BuildSystem& _build, CommandIntake& _intake, std::uint32_t _tick);

private:
  std::size_t m_players = 0;
  std::size_t m_count = 0;
  std::array<StubAi, MAX_PLAYERS + 1> m_ais{};
  std::vector<EntityRecord> m_view;
  std::vector<Command> m_commands;
};

} // namespace Outpost
