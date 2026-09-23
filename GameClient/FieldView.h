#pragma once

#include "GameCore.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// The asteroid field as this client draws it: **derived here, from the match's seed and player count, by
/// the same `GameCore` function the host would run** (M2.3). R23 -- no map is transmitted -- and Q22 --
/// no asteroid is replicated before M3, because an inexhaustible rock has no simulation state to send.
///
/// **THIS IS NOT THE CLIENT SIMULATING** (R19). A generator is a rule, and a rule both sides evaluate
/// lives in `GameCore`; what the simulation owns about a rock -- how much ore is left, from M3 -- will
/// arrive in the update like any other state, and this class will not grow it.
///
/// **THE TWO INPUTS COME FROM THE JOIN AND NOWHERE ELSE** (ADR-013): a seed and a count the host sent,
/// never a count the client inferred from how many stations it has been told about. Records arrive in
/// priority order over several ticks (ADR-024), so a count read off the replica store could be low for
/// the first second and the field drawn wrong, then redrawn.
class FieldView
{
public:
  /// Derives the field for this pair, or keeps the one already held when the pair has not changed -- a
  /// rejoin into the same match answers with the same seed and count, and regenerating would cost a few
  /// thousand draws for the same rows. True when the rows changed.
  ///
  /// A count of zero is no match and clears, which is what a refused reply carries.
  bool Derive(std::uint64_t _matchSeed, std::size_t _playerCount);

  /// Back to nothing: no match, no field.
  void Clear() noexcept;

  /// The rocks, in `GenerateField`'s order: player one's region, then each copy one step further round.
  /// Empty until a match has been derived.
  [[nodiscard]] std::span<const Placement> Rocks() const noexcept
  {
    return m_rocks;
  }

  [[nodiscard]] bool IsDerived() const noexcept
  {
    return m_playerCount != 0;
  }

  [[nodiscard]] std::uint64_t MatchSeed() const noexcept
  {
    return m_matchSeed;
  }

  [[nodiscard]] std::size_t PlayerCount() const noexcept
  {
    return m_playerCount;
  }

private:
  std::vector<Placement> m_rocks;
  std::uint64_t m_matchSeed = 0;

  /// Zero is "nothing derived", which no match is: a host seats at least one player.
  std::size_t m_playerCount = 0;
};

} // namespace Outpost
