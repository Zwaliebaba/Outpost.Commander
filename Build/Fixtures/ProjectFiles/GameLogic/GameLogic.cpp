#include "pch.h"

#include "WindowsHeader.h" // platform-header: GameLogic stays portable.

#include "GameLogic.h"
#include "NeuronClient.h" // edge-include, upward: NeuronClient is built on GameLogic, not the reverse.

namespace Fixture
{

std::uint32_t SimValue(const GameLogic& _item)
{
  return _item.value;
}

} // namespace Fixture
