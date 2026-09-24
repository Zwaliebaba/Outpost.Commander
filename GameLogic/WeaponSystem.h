#pragma once

#include "UniformGrid.h"
#include "World.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// ADR-014: **a fire event at most once per shooter per this many ticks**, whatever the damage cadence. It draws
/// a tracer and feeds the offscreen alert, and neither needs twenty a second.
inline constexpr std::uint32_t FIRE_EVENT_INTERVAL_TICKS = 10;

/// **WEAPONS IN THE TICK** (M3.2, `TechnicalDesign.md` section 2: after movement, before mining).
///
/// In one pass, in index order, every armed entity picks a target, turns to bear on it if it may, and fires
/// every mount whose range and arc reach it (ADR-004, Q68). What a ship targets depends on its orders:
///
/// | Orders | Target | Turns to bear |
/// |---|---|---|
/// | None | The nearest hostile in range, any bearing | Yes (Q68) |
/// | Attack, standing at its slot | The ordered target | Yes |
/// | Attack, on its way | The ordered target if it is in range and arc, else as a move order | No |
/// | Move | The nearest hostile in range **and** arc (Q78) | No |
///
/// **DAMAGE IS APPLIED AFTER EVERY WEAPON HAS FIRED** (ADR-014). The points each mount settles are summed a
/// target, and taken off the hull at the end of the pass, so no shot depends on the slot order of the shots
/// before it. A hull stops at zero: what happens at zero is M3.4's deaths step.
///
/// **NOTHING HERE NAMES A STATION** (R24). A station fires because its hull carries two point-defense mounts
/// with a 360-degree arc, and does not turn because it has no drive (M3.5).
class WeaponSystem
{
public:
  /// One tick. _tick is the host's tick counter, used only to space fire events.
  void Advance(World& _world, std::uint32_t _tick);

  /// The fire events this tick, for the accumulator (ADR-024), in shooter index order.
  [[nodiscard]] std::span<const FireEvent> Fired() const noexcept
  {
    return m_fired;
  }

private:
  /// Ends attack orders whose target is gone, and solves an order's arc again once its target has moved more
  /// than the arc's spacing (Q67).
  void ResolveAttacks(World& _world);

  UniformGrid m_grid;
  std::vector<EntityId> m_scratch;

  /// Whole points owed to each slot this tick, summed over every shooter and applied at the end.
  std::vector<std::uint32_t> m_damage;

  /// **ONE MORE THAN THE TICK OF EACH SLOT'S LAST FIRE EVENT**, and zero for never. Not hashed and not
  /// simulation: it spaces what is sent, and a tracer thinned differently changes nothing in the match.
  std::vector<std::uint32_t> m_lastFireEvent;

  std::vector<FireEvent> m_fired;
};

} // namespace Outpost
