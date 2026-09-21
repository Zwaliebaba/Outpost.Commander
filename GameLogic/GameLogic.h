#pragma once

// GameLogic -- the authoritative simulation. It runs on the host and nowhere else: the client
// links none of it, which is what makes the interest set a boundary the linker keeps rather than a
// convention inside one address space.
//
// The master include of this library.

#include "NeuronServer.h"

#include "GameCore.h"

// This library's own headers. World first, because the other two are declared over it.
#include "World.h"

#include "CommandIntake.h"
#include "StateHash.h"
#include "Tick.h"

namespace Outpost
{
/// The name of this library, so that a suite can prove it linked and that the include path reaches
/// this header. Delete it when the first real declaration lands.
[[nodiscard]] std::string_view LogicLibraryName() noexcept;
} // namespace Outpost