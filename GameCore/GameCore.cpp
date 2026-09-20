// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "GameCore.h"

namespace Outpost
{
std::string_view CoreLibraryName() noexcept
{
  return "GameCore";
}
} // namespace Outpost