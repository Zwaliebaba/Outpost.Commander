#pragma once

// NeuronServer -- the engine code only the host runs: the tick loop, and the sockets under it.
//
// The master include of this library.

#include "NeuronCore.h"

namespace Neuron
{

/// The name of this library, so that a suite can prove it linked and that the include path reaches
/// this header. Delete it when the first real declaration lands.
[[nodiscard]] std::string_view ServerLibraryName() noexcept;

} // namespace Neuron
