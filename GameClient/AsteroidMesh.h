#pragma once

#include "HitTest.h"
#include "HullMesh.h"

#include "GameCore.h"
#include "NeuronClient.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Outpost
{

/// **HOW A ROCK IS DRAWN, AND NOTHING ABOUT WHERE IT IS** (M2.4). Where it is comes from `FieldView`, which
/// is `GameCore`'s generator and is the same on both sides (R23). What it looks like -- which of the five
/// authored variants, turned how, how large, how far off the plane -- is decided here, in the client, and
/// **never reaches a `GameCore` record** (R22, ADR-001): nothing is simulated about it and the host does not
/// know it exists.
///
/// **ADR-005 IS WHY THIS FILE EXISTS.** A mesh is a file and a file is one rock, so variation is the seed
/// choosing among authored variants and jittering them -- weaker than a generator, because a variant set
/// repeats. `Design/design_handoff_meshes/` section 8 states the jitter the set was authored for and this
/// applies exactly that.
///
/// **THE FIELD IS BAKED, NOT INSTANCED.** The ship pass turns an instance about Z and nothing else, and a
/// rock turns on all three axes, scales and leaves the plane. The field never moves once it is derived, so
/// each variant's rocks are transformed once, here, into one static mesh, and the ship pass draws that mesh
/// with one identity instance -- **one draw per variant, five for the field, and no second shader.** A rock
/// instance layout would have needed a new vertex stage for the same five draws and a per-frame upload of
/// geometry that does not change.

/// Five: `AsteroidA` to `AsteroidE`, the handoff's set, smallest first.
inline constexpr std::size_t ASTEROID_VARIANT_COUNT = 5;

/// **PCG32's STREAM FOR THE ROCKS' LOOK.** `Sessions` took 1, the sky 2 and the generator 3. This takes 4,
/// and it is a CLIENT stream: nothing on the host draws from it, so it cannot fall out of step with
/// anything the simulation owns -- it is separate from the generator's so that changing how a rock LOOKS
/// can never move where one IS.
inline constexpr std::uint64_t ROCK_LOOK_STREAM = 4;

/// The handoff's uniform scale range, 0.75 to 1.35, in percent so the draw is an integer. **Uniform only**:
/// non-uniform scale would break the baked per-face normals, which nothing renormalizes.
inline constexpr std::int32_t ROCK_SCALE_LOWEST_PERCENT = 75;
inline constexpr std::int32_t ROCK_SCALE_HIGHEST_PERCENT = 135;

/// The handoff's off-plane offset, up to 240 units either side. Rocks are the only thing drawn off the
/// plane (ADR-001), and this is a drawn height, not a position.
inline constexpr std::int32_t ROCK_LIFT_UNITS = 240;

/// One rock's look.
///
/// R8: a public aggregate.
struct RockLook
{
  /// Into `AsteroidVariantNames()`.
  std::uint8_t variant = 0;

  /// **ALL THREE AXES, THE WHOLE CIRCLE.** The variants were authored with no up and no flat bottom, so any
  /// orientation is a legal one (handoff section 8).
  Neuron::Angle yaw = 0;
  Neuron::Angle pitch = 0;
  Neuron::Angle roll = 0;

  /// What the seed drew, 75 to 135.
  std::int32_t scalePercent = 100;

  /// What is drawn: the drawn scale, **clamped so that the rock cannot touch its nearest neighbor.** See
  /// `RockLooks`.
  float scale = 1.0f;

  /// Units above the plane, negative below.
  std::int32_t liftUnits = 0;
};

/// The five names, in variant order, as the catalog spells them.
[[nodiscard]] std::span<const std::string_view> AsteroidVariantNames() noexcept;

/// **A SPHERE AROUND THE MESH, FROM THE CATALOG'S BOUNDS**: the farthest corner of its box from its origin.
/// Loose -- the true farthest vertex is nearer -- and that is the safe direction, because it is what the
/// no-touching clamp divides by. Any orientation keeps a rock inside it, which is why a sphere and not the
/// box.
[[nodiscard]] float BoundingRadiusUnits(const MeshEntry& _entry) noexcept;

/// **THE EXACT SPHERE**: the farthest vertex of a loaded mesh from its origin, which no orientation can
/// carry a rock outside. What the client uses once the variants are read, because the catalog's box
/// corner is well outside the rock -- 135 units for `AsteroidE` against its 167-unit length -- and a
/// loose radius clamps rocks smaller than they need to be.
[[nodiscard]] float BoundingRadiusUnits(const HullMesh& _mesh) noexcept;

/// One look per rock, in `FieldView`'s order, from the match seed on `ROCK_LOOK_STREAM`. Every rock takes
/// the same six draws in the same order, so rock _k_'s look depends on the seed and _k_ alone.
///
/// **THE SCALE IS CLAMPED SO NO TWO ROCKS TOUCH.** The generator keeps rock centers 150 units apart and the
/// largest variant at the top of the range is 225 across, so the seed alone would put two rocks through each
/// other. Each rock is held to half the distance to its nearest neighbor, measured on the plane: two rocks
/// A and B then satisfy `rA + rB <= dA/2 + dB/2 <= |AB|`, whatever their variants. The lift only moves them
/// further apart. A clamped rock can come out smaller than 0.75, which is smaller and not wrong.
///
/// _radiusUnits is each variant's bounding radius, in variant order.
[[nodiscard]] std::vector<RockLook> RockLooks(std::uint64_t _matchSeed, std::span<const Placement> _rocks,
                                              std::span<const float, ASTEROID_VARIANT_COUNT> _radiusUnits);

/// One vertex of one rock, from the variant's own vertex (already in world axes, `ToWorldVertex`) to the
/// field: turned by roll, then pitch, then yaw -- about X, Y and Z -- scaled, and moved to the rock's place
/// and lift. **The normal turns and is not scaled**: a rotation keeps it unit, and a uniform scale does not
/// change its direction. Neither changes a triangle's winding, so the indices copy as they are.
[[nodiscard]] HullVertex PlaceRockVertex(const HullVertex& _vertex, const Placement& _rock, const RockLook& _look) noexcept;

/// **WHERE EACH ROCK IS DRAWN, FOR THE TAP** (M2.8): its plane position and its lift, in field order. The lift
/// is the same six-draw look the bake uses, so a tap aims where the rock is on screen.
[[nodiscard]] std::vector<RockPickPoint> RockPickPoints(std::span<const Placement> _rocks, std::span<const RockLook> _looks);

/// **ONE VARIANT'S ROCKS AS ONE MESH**: every rock whose look chose _variant, placed and appended, with its
/// indices offset. False when the result would pass the 16-bit index limit -- 173 rocks of one variant,
/// against a field of at most 88 -- and _outMesh is then left empty rather than drawn in part.
[[nodiscard]] bool BuildVariantField(const HullMesh& _variantMesh, std::uint8_t _variant, std::span<const Placement> _rocks,
                                     std::span<const RockLook> _looks, HullMesh& _outMesh);

/// **A SPENT ROCK IS GONE** (the owner, 2026-09-24, `OpenQuestions.md` Q83): _rocks and _looks, in step, without every
/// rock _spent marks, into _outRocks and _outLooks -- what the bake draws. The looks are computed over the whole field
/// first and only then filtered, so no surviving rock changes shape when its neighbor runs dry.
void OmitSpentRocks(std::span<const Placement> _rocks, std::span<const RockLook> _looks, std::span<const std::uint8_t> _spent,
                    std::vector<Placement>& _outRocks, std::vector<RockLook>& _outLooks);

} // namespace Outpost
