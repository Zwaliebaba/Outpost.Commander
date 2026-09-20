#pragma once

#include "HeightView.h"

#include <array>
#include <cstdint>

// Where a ray meets the ground, and which way the ground faces there (Design/Interface.md §4 and
// §11 row 14; m1-vertical-slice/K7). The ground cursor is a ring drawn ON the landscape and tilted
// to its slope, so pointing at a hillside points at the hillside; this is the half of that which is
// arithmetic, and CursorPass is the half that is a draw call.
//
// NO DIRECT3D AND NO DirectXMath HERE, deliberately. Everything in this header is arithmetic over
// the heightfield and a ray, so it is a unit test on any machine rather than something only the
// capture can show. The pass above it may use whatever the SDK offers; the answer it draws is
// decided here, where a wrong one is a failing assertion instead of a ring in the wrong valley.
//
// THE RAY IS TESTED AGAINST THE SAME DIAGONAL THE TERRAIN MESH IS BUILT WITH, which is the point
// of marching the grid rather than reading a height. A cell is two triangles and there are two
// ways to split it; on a saddle the two splits differ by most of a cell's height, so a cursor
// tested against the other diagonal floats over the ground the commander is looking at. The split
// here is Client/TerrainChunk.cpp's: the shared edge runs from (x+1, z) to (x, z+1).
//
// THE HEIGHTS ARE THE LANDSCAPE'S AND NOT THE MESH'S. The mesh dips its shore samples below the
// water to hide the seam (TerrainChunk.cpp's SHORE_DIP); that is a cosmetic of the draw and not
// where the ground is, and the sea is answered by the water plane below rather than by the seabed.

namespace Neuron
{

/// How far above the water the ring is put when it floats on the sea, in world units. One, so that
/// it is not coplanar with the water pass's own surface and does not fight it for the depth test.
inline constexpr float RING_WATER_CLEARANCE = 1.0f;

/// What a ray found. `distance` is along the ray from its origin, in world units, for a caller
/// that wants to size the ring by it; the normal is the ground's, smoothed, not the triangle's.
struct GroundHit
{
  bool hit = false;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float nx = 0.0f; ///< The surface normal at (x, z); exactly (0, 1, 0) on flat ground and on water
  float ny = 1.0f;
  float nz = 0.0f;
  float distance = 0.0f;
  bool water = false; ///< It landed on the sea rather than on the ground
};

/// The landscape's extent in world units: the distance from the first sample to the last.
[[nodiscard]] float GroundExtent(const HeightView& _view) noexcept;

/// The height at a world point, bilinear over the four samples around it, in world units. Outside
/// the landscape it is the nearest edge's, which is what clamping the sample indices gives and is
/// what a caller that has not checked GroundExtent deserves rather than a read out of bounds.
[[nodiscard]] float GroundHeight(const HeightView& _view, float _x, float _z) noexcept;
/// The surface normal at a world point: central differences at the four surrounding samples,
/// bilinear between them, normalised. Flat ground gives EXACTLY (0, 1, 0), which matters because
/// the pass builds its quad from the cross product of this with world up and that cross is zero
/// there - a case that has to be branched rather than normalised, and is a live fault in the tree
/// this cursor is taken from (SpeciesLook.md §7.1).
[[nodiscard]] std::array<float, 3> GroundNormal(const HeightView& _view, float _x, float _z) noexcept;

/// Where _direction from _origin first meets the landscape, or a miss. _direction need not be a
/// unit vector; it is normalised here, so `distance` is in world units whatever the caller passes.
///
/// A ray that never enters the landscape rectangle misses, and so does one that crosses it without
/// coming down to the ground: OFF THE LANDSCAPE THE CURSOR IS HIDDEN, and a caller that reuses its
/// last position instead would leave a ring sitting on a hill the commander has turned away from.
[[nodiscard]] GroundHit RayAgainstGround(const HeightView& _view, const std::array<float, 3>& _origin,
                                         const std::array<float, 3>& _direction) noexcept;

} // namespace Neuron
