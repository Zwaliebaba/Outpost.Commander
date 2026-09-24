#pragma once

#include "World.h"

#include <span>
#include <vector>

namespace Outpost
{

/// One entity that died this tick, as it was the moment before. R8: a public aggregate.
struct Death
{
  EntityId id{};
  PlayerId owner = NO_PLAYER;
  DesignId design = DesignId::Miner;
};

/// **DEATH** (M3.4, `TechnicalDesign.md` section 2: after build queues, before victory). Everything whose hull reached
/// zero this tick is destroyed, in slot order, so what a later system sees does not depend on who fired first.
///
/// **NOTHING HERE TELLS A CLIENT.** The accumulator finds a dead slot by looking and sends its removal ten
/// times (ADR-003, ADR-024), so no system that destroys has to remember to report it. **Nothing here names a
/// station either** (R24): a station at zero hull dies like anything else, and what that means for its owner is
/// M3.7's.
class DeathSystem
{
public:
  /// One tick: destroys every live entity at zero hull.
  void Advance(World& _world);

  /// What died this tick, in slot order.
  [[nodiscard]] std::span<const Death> Died() const noexcept
  {
    return m_died;
  }

private:
  std::vector<Death> m_died;
};

} // namespace Outpost
