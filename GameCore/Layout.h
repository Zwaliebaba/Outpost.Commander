#pragma once

#include "Design.h"
#include "Entity.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// The match's starting layout, from a seed. `TechnicalDesign.md` section 3: **this is `GameCore` code
/// that BOTH SIDES RUN** -- the host to populate the match and the client to draw the same field -- which
/// is R23, and the reason no map is ever transmitted.
///
/// **M0 AND M1 PASS ONE FIXED SEED AND THIS FUNCTION IGNORES IT** (`GameDesign.md` section 3). The seed
/// is in the signature from the first line anyway: M2's generator is this function growing asteroids, not
/// a different function, and a parameter added later is a call site to find everywhere.
///
/// **WHAT IS HERE IS THE STATIONS AND NOTHING ELSE.** Home fields and contested fields are M2's, and the
/// asteroid half of Q26 is still open; the anchor half is answered and is below.

/// Q26, answered 2026-09-22: **6,000 world units.** Two opposed stations then sit 12,000 apart, which at
/// the `Fighter`'s 140 units a second is **86 seconds** -- inside `GameDesign.md` section 7's stated 80 to
/// 100 second crossing, which is the arithmetic the raid balance rests on. It leaves 2,192 units of margin
/// to the play area's edge for the camera clamp.
///
/// **IT GETS SHORTER AT FOUR PLAYERS AND THAT IS NOT OBVIOUS.** Anchors a quarter turn apart on this
/// radius put OPPOSED players 12,000 apart and ADJACENT ones 8,485 -- sixty seconds, not eighty-six. The
/// register names it as a consequence to measure at M4 rather than a reason to move the number now.
inline constexpr std::int32_t ANCHOR_RADIUS_UNITS = 6000;

inline constexpr Neuron::Fixed ANCHOR_RADIUS = ANCHOR_RADIUS_UNITS * Neuron::FIXED_ONE;

/// One per quadrant (`GameDesign.md` section 3), and four is the design's slot count rather than a
/// coincidence.
inline constexpr std::size_t ANCHOR_COUNT = 4;

/// One placed object. `GameDesign.md` section 3: "the generator's interface is a seed in and a list of
/// placed objects out, so adding a kind later does not change its shape" -- which is why an asteroid,
/// when M2 has one, is another row here rather than a second function.
///
/// R8: a public aggregate.
struct Placement
{
  DesignId design = DesignId::Station;

  /// `NO_PLAYER` for anything nobody owns, which is what an asteroid will be.
  PlayerId owner = NO_PLAYER;

  Neuron::Vec2 position{};
  Neuron::Angle heading = 0;

  [[nodiscard]] friend constexpr bool operator==(const Placement&, const Placement&) noexcept = default;
};

/// Where a player starts.
///
/// **THE FOUR ANCHORS ARE FIXED AND A SHORTER MATCH TAKES A SUBSET OF THEM.** `GameDesign.md` section 3
/// describes a quadrant copied at 90, 180 and 270 degrees for four players and a half copied at 180 for
/// two; both are the same four points, and choosing two OPPOSED ones is what "copied at 180" means. The
/// step between chosen anchors is therefore `4 / _playerCount` quarter turns: two at two players, one at
/// four.
///
/// **EVERY ROTATION IS EXACT BECAUSE THEY ARE ALL QUARTER TURNS ON INTEGERS** -- a swap and a negation
/// (`TechnicalDesign.md` section 3). A floating-point rotation would make the starts subtly unequal, which
/// is the kind of unfairness nobody would find for a year.
///
/// **A THREE-PLAYER MATCH IS NOT ROTATIONALLY SYMMETRIC AND THE DESIGN DOES NOT CONTEMPLATE ONE.** A
/// match is four slots, any of which is a human or an AI (`GameDesign.md` section 2), so the counts that
/// occur are two and four. Three lands on anchors 0, 1 and 2 -- exact, adjacent, and unfair, which is
/// stated here rather than discovered.
///
/// _player is one-based; `NO_PLAYER` and anything past the count returns the origin, which no anchor is.
[[nodiscard]] Neuron::Vec2 StartAnchor(std::size_t _playerCount, PlayerId _player) noexcept;

/// **FACING THE CENTER**, which falls out of the same quarter turns: the base anchor sits on the negative
/// x axis looking along positive x, which is heading zero (`GameClient/Camera.cpp`), and each quarter turn
/// of the position is a quarter turn of the heading. A station never moves (`GameDesign.md` section 5), so
/// this is the only heading it will ever have.
[[nodiscard]] Neuron::Angle StartHeading(std::size_t _playerCount, PlayerId _player) noexcept;

/// The whole layout, in player order. One `Station` on each anchor, owned.
///
/// **A STATION NEEDS NO CODE OF ITS OWN** (M1.2): it is a `Station` hull with two `PointDefense` mounts
/// and no drive, which is a row in the design table, so this function places an ordinary entity and the
/// snapshot carries it as one.
[[nodiscard]] std::vector<Placement> GenerateLayout(std::uint64_t _seed, std::size_t _playerCount);

} // namespace Outpost
