#include "pch.h"

#include "GameClient.h"
#include "NeuronCore.h"

namespace Fixture
{

std::uint32_t ReplicaValue(const GameClient& _item)
{
  return static_cast<std::uint32_t>(sizeof _item);
}

} // namespace Fixture
