#pragma once

#include <cstdint>

namespace Outpost
{

// THE STATE IS SHARED VOCABULARY AND THE EVALUATION IS NOT (ADR-018). A frame record carries a
// seat's victory state to the client, so the enum and the count of its values are on the wire and
// live here; Annihilated and CheckVictory both take a Sim& and are GameLogic's, in Victory.h beside
// the simulation they read.

/// Where a commander stands. One value a seat, written by stage 12 and by nothing else, carried by
/// the snapshot and reported to the client in the SeatState record.
///
/// Eliminated and Lost are not the same answer. Eliminated is leaving the match while it runs -
/// surrendered, or annihilated - and the seat plays no further part from that tick. Lost is being
/// on the board when the match ended and not being the side that won, which is what the survival
/// clock hands out and what the last enemy alliance's elimination hands to nobody. A seat that
/// surrendered and whose alliance then won on the clock stays Eliminated: it was not there.
enum class VictoryState : std::uint8_t
{
  Playing,
  Won,
  Lost,
  Eliminated
};

/// THE COUNT LIVES BESIDE THE ENUM AND NOT BESIDE ITS READERS. Both the snapshot's reader and the
/// record's validator bound a byte from a file or the wire with it, and a value added above with
/// the count left behind is a bound that silently admits one too few.
inline constexpr std::uint8_t VICTORY_STATE_COUNT = 4;

} // namespace Outpost
