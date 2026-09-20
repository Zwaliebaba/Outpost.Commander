#pragma once

#include <cstdint>

namespace Outpost
{

// THE STATE IS SHARED VOCABULARY AND THE EVALUATION IS NOT (ADR-018). A frame record carries a
// seat's victory state to the client, so the enum is on the wire and lives here; Annihilated and
// CheckVictory both take a Sim& and are GameLogic's, in Victory.h beside the simulation they read.

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

} // namespace Outpost
