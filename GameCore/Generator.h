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
/// **ONE PLAYER'S REGION IS GENERATED AND EVERY OTHER IS A COPY OF IT** (M2.2). `GameDesign.md` section 3
/// generates a half at two players and a quadrant at four and copies it by rotation; the region is player
/// one's, around `StartAnchor(_, 1)` on the negative x axis, and `GenerateField` makes the copies.

/// **PCG32's STREAM FOR THE GENERATOR.** `GameLogic/Sessions.h` took 1 and `NeuronClient/StarField.h` took
/// 2, and each said this one owns the rest. It takes 3. A generator sharing a stream with anything the
/// simulation draws from would fall out of step with the client at the first draw the client never makes.
inline constexpr std::uint64_t GENERATOR_STREAM = 3;

// === Q26's ASTEROID HALF, answered by the owner on 2026-09-23. ============================================
//
// **ALL PROVISIONAL AND ALL NAMED**, which is the one thing the register required: "the answer may be
// provisional, but it may not be anonymous". M3's twenty matches are where they are expected to move.

/// **TEN ROCKS IN EACH HOME FIELD**, the register's figure. **Ten rocks serve about thirty miners** since Q62
/// ruled one extractor per rock per tick: a rock yields one laser's 20 ore a second and a miner spends most
/// of its cycle flying, so six miners -- a running economy -- never queue.
inline constexpr std::size_t HOME_FIELD_ASTEROID_COUNT = 10;

/// **2,000 UNITS OUT, AND 1,200 IN** (`OpenQuestions.md` Q62, ruled 2026-09-24). At 600 to 1,500 the nearest rock
/// paid one miner 6.4 credits a second against the 2.5 `GameDesign.md` section 4 assumed, income passed the
/// 20-a-second build slot inside a minute, and the slot rather than the miners paced the match. Moved out, a
/// miner earns 2.7 to 3.7 a second and six earn less than the slot, which is Q47's intent. The measured table
/// is on Q26.
inline constexpr std::int32_t HOME_FIELD_OUTER_RADIUS_UNITS = 2000;

/// **NOTHING CLOSER THAN 1,200** (Q62). It was 600, which kept rocks off the 400-unit module ring and out of
/// point defense; 1,200 still does both by a wide margin, and it is what moves income under the build slot.
inline constexpr std::int32_t HOME_FIELD_INNER_RADIUS_UNITS = 1200;

/// **TWO CONTESTED CLUSTERS IN EACH REGION**, so two at two players become four on the map once `GenerateField`
/// copies them, and eight at four players. "Richer clusters toward the center, reachable by everyone" is the map's
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

/// How many copies of the region make the map: **two at two players and four at four.** One is treated as
/// two and three as four, as `GenerateRegion` treats them, and so is every stress count above four
/// (ADR-023) -- the field is the four-player field and does not claim to be fair to a ninth player, which
/// its stations already do not. Zero players is zero copies.
[[nodiscard]] std::size_t FieldCopyCount(std::size_t _playerCount) noexcept;

/// **THE WHOLE FIELD** (M2.2): the region, then each copy of it turned about the center by
/// `4 / FieldCopyCount` quarter turns more than the last -- 180 degrees at two players; 90, 180 and 270 at
/// four. **Copy k is the region of the player whose anchor is k steps round** (`StartAnchor`), so at two and
/// four players the home field in copy k sits around player k + 1's station. Each copy keeps the region's
/// order, so a row's index modulo the region's size says which rock of the region it is a copy of.
///
/// **EVERY ROTATION IS `QuarterTurn`, A SWAP AND A NEGATION ON INTEGERS**, so every player's field is the
/// same field to the unit and no seed can be unlucky (`TechnicalDesign.md` section 3). Nothing is drawn from
/// the PRNG here: the copies cost no draws and the region's draws are unchanged, so the pinned region is
/// still the first rows of this.
[[nodiscard]] std::vector<Placement> GenerateField(std::uint64_t _seed, std::size_t _playerCount);

/// **THE MAP, AS ONE NUMBER** (`OpenQuestions.md` Q76): FNV-1a over every row `GenerateField` and `GenerateLayout`
/// produce for _seed and _playerCount -- kind, field, design, owner, position and heading, each little-endian. The
/// host sends it on the join reply and the client compares it with its own, so a generator that reaches a different
/// answer on the two sides -- an ARM64 client against an x64 host, say -- is refused at the join rather than found as
/// a miner mining a rock nobody can see (R23).
[[nodiscard]] std::uint64_t FieldHash(std::uint64_t _seed, std::size_t _playerCount);

/// Whether a point, in whole units, lies inside player one's region with at least _marginUnits to spare
/// from every edge the rotation copies across. **At two players the region is the half-plane `x < 0`; at
/// four it is the quarter between the two diagonals around the negative x axis.** The play area's own edge
/// is not one of those and is checked separately.
[[nodiscard]] bool InRegion(std::int32_t _xUnits, std::int32_t _yUnits, std::size_t _playerCount, std::int32_t _marginUnits) noexcept;

} // namespace Outpost
