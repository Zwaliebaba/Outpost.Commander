#pragma once

// GameClient -- the game code only the client runs: what a player is shown, and what they are
// allowed to ask for. It holds no simulation.
//
// The master include of this library.

#include "NeuronClient.h"

#include "GameCore.h"

// This library's own headers, so that a consumer includes this one file and gets the whole chain
// (AGENTS.md section 2). Interpolation is the arithmetic and ReplicaStore is what holds the
// snapshots it runs over; the split is what lets a suite pin the arithmetic without a clock.
#include "Camera.h"
#include "Interpolation.h"
#include "OrderMarker.h"
#include "ReplicaStore.h"
#include "TapOrder.h"

namespace Outpost
{
/// The name of this library, so that a suite can prove it linked and that the include path reaches
/// this header. Delete it when the first real declaration lands.
[[nodiscard]] std::string_view ClientLibraryName() noexcept;
} // namespace Outpost