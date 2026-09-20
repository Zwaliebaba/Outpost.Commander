#pragma once

// GameClient -- the game code only the client runs: what a player is shown, and what they are
// allowed to ask for. It holds no simulation.
//
// The master include of this library.

#include "NeuronClient.h"

#include "GameCore.h"

namespace Outpost
{

/// The name of this library, so that a suite can prove it linked and that the include path reaches
/// this header. Delete it when the first real declaration lands.
[[nodiscard]] std::string_view ClientLibraryName() noexcept;

} // namespace Outpost
