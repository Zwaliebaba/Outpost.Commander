#pragma once

// GameCore -- the game rules that both sides need: what the host validates with and what the
// client is allowed to predict against.
//
// The master include of this library.

#include "NeuronCore.h"

// This library's own headers (AGENTS.md section 2): a consumer includes this one file and gets
// the whole chain.
#include "Entity.h"

namespace Outpost
{
/// The name of this library, so that a suite can prove it was compiled into the binary under test
/// and that the include path reaches this header. Delete it when the first real declaration lands.
[[nodiscard]] std::string_view CoreLibraryName() noexcept;
} // namespace Outpost