// A shared-items translation unit: this file is compiled into every project that imports
// NeuronCore.vcxitems, each of which has its own precompiled header. There is no one pch to share
// between them, so these files use none and include their master header first instead.

#include "NeuronCore.h"

namespace Neuron
{

std::string_view CoreLibraryName() noexcept
{
  return "NeuronCore";
}

} // namespace Neuron
