#pragma once

#include "Layout.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// The asteroid field, from the match seed (M2.1). `TechnicalDesign.md` section 3: **`GameCore` code that
/// BOTH SIDES RUN** -- the host to populate the match and the client to draw the same rocks -- so no map is
/// ever transmitted (R23).
///
/// **A GENERATOR THAT REACHES A DIFFERENT ANSWER ON THE TWO SIDES IS A DESYNCHRONIZATION**, and it presents
/// as rocks in the wrong place rather than as anything that looks like one. So R16 in full: whole units in
/// `int32`, squared distances in `int64`, one `Pcg32` on its own stream, no container whose order is not
/// the order things were placed in, and no float anywhere.
///
/// **WHAT THIS STEP GENERATES IS ONE PLAYER'S REGION**, and nothing yet copies it. `GameDesign.md` section
/// 3 generates a half at two players and a quadrant at four and copies it by rotation, which is M2.2's; the
/// region is what gets copied, and it is player one's, around `StartAnchor(_, 1)` on the negative x axis.

/// **PCG32's STREAM FOR THE GENERATOR.** `GameLogic/Sessions.h` took 1 and `NeuronClient/StarField.h` took
/// 2, and each said this one owns the rest. It takes 3. A generator sharing a stream with anything the
/// simulation draws from would fall out of step with the client at the first draw the client never makes.
inline constexpr std::uint64_t GENERATOR_STREAM = 3;

// === Q26's ASTEROID HALF, answered by the owner on 2026-09-23. ============================================
//
// **ALL PROVISIONAL AND ALL NAMED**, which is the one thing the register required: "the answer may be
// provisional, but it may not be anonymous". M3's twenty matches are where they are expected to move.

/// **TEN ROCKS IN EACH HOME FIELD**, the register's figure. Income is about 15 credits a second and one
/// miner makes 2.5, so a running economy is about six miners, and ten rocks keeps them from queuing on one.
inline constexpr std::size_t HOME_FIELD_ASTEROID_COUNT = 10;

/// `GameDesign.md` section 3's "within about 1,500 units of each anchor".
inline constexpr std::int32_t HOME_FIELD_OUTER_RADIUS_UNITS = 1500;

/// **NOTHING CLOSER THAN 600.** A module is placed within 400 units of its station (`GameDesign.md` section
/// 5), so a rock inside that would sit on the base; 600 leaves 200 beyond the ring, which is also outside
/// point defense's 400 -- miners at the rocks are raidable, which section 5 says is the point.
inline constexpr std::int32_t HOME_FIELD_INNER_RADIUS_UNITS = 600;

/// **TWO CONTESTED CLUSTERS IN EACH REGION**, so two at two players become four on the map once M2.2 copies
/// them, and eight at four players. "Richer clusters toward the center, reachable by everyone" is the map's
/// only real proposition (`GameDesign.md` section 3).
inline constexpr std::size_t CONTESTED_FIELD_COUNT = 2;

/// Six rocks each. **Fewer than a home field and meant to be richer**, which is M3's to make true: until
/// then an asteroid is inexhaustible and "richer" has nothing to act on.
inline constexpr std::size_t CONTESTED_FIELD_ASTEROID_COUNT = 6;

/// How far a contested cluster's rocks spread from its center.
inline constexpr std::int32_t CONTESTED_FIELD_RADIUS_UNITS = 600;

/// **WHERE A CONTESTED CLUSTER'S CENTER MAY SIT, MEASURED FROM THE MAP'S CENTER.** Between 1,500 and 3,500:
/// at least 2,500 from the nearest anchor, so it is not a second home field, and toward the middle, which
/// is what "contested" means.
inline constexpr std::int32_t CONTESTED_FIELD_NEAREST_UNITS = 1500;
inline constexpr std::int32_t CONTESTED_FIELD_FARTHEST_UNITS = 3500;

/// **NO TWO ROCKS CLOSER THAN 150 UNITS, CENTER TO CENTER**, including across the boundary a copy will be
/// made over: every rock keeps half of this from its region's edge, so M2.2's rotation cannot land one
/// within 150 of another. How large a rock is drawn is M2.4's; this is only the floor under that.
inline constexpr std::int32_t ASTEROID_SPACING_UNITS = 150;

/// The asteroids in one region, **in the order they were placed**: the home field, then each contested
/// cluster in turn. Positions are whole units scaled to `Neuron::Fixed`, heading is zero and owner is
/// `NO_PLAYER` -- the visual yaw a rock is drawn at is the client's (M2.4), never a `GameCore` field.
///
/// A player count of zero places nothing. One is treated as two -- a practice match on half a map -- and
/// three as four, which `Layout.h` already says is not symmetric.
[[nodiscard]] std::vector<Placement> GenerateRegion(std::uint64_t _seed, std::size_t _playerCount);

/// Whether a point, in whole units, lies inside player one's region with at least _marginUnits to spare
/// from every edge the rotation copies across. **At two players the region is the half-plane `x < 0`; at
/// four it is the quarter between the two diagonals around the negative x axis.** The play area's own edge
/// is not one of those and is checked separately.
[[nodiscard]] bool InRegion(std::int32_t _xUnits, std::int32_t _yUnits, std::size_t _playerCount, std::int32_t _marginUnits) noexcept;

} // namespace Outpost
