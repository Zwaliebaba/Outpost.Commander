#include "pch.h"

#include "NeuronServer.h"
#include "GameClient.h" // edge-include, sideways: the two libraries share what is below them, never each other.

namespace Fixture
{

std::uint32_t ClientValue(const NeuronServer& _item)
{
  return _item.value;
}

} // namespace Fixture
