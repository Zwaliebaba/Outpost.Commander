#pragma once

// NeuronClient -- the engine code only the client runs: presentation, input and the frame loop.
//
// The master include of this library. It includes the master include of what this library is
// built on, so that a consumer includes this one file and gets the whole chain.

#include "NeuronCore.h"

namespace Neuron
{
/// The name of this library, so that a suite can prove it linked and that the include path reaches
/// this header. Delete it when the first real declaration lands.
[[nodiscard]] std::string_view ClientLibraryName() noexcept;
} // namespace Neuron