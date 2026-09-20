#include "pch.h"

#include "GroundRay.h"

#include <algorithm>
#include <cmath>

namespace Neuron
{

namespace
{

/// The sample at (column, row), clamped to the grid.
[[nodiscard]] float Sample(const HeightView& _view, std::int32_t _column, std::int32_t _row) noexcept
{
  if (_view.samples == nullptr || _view.samplesPerSide == 0)
  {
    return 0.0f;
  }
  const auto last = static_cast<std::int32_t>(_view.samplesPerSide) - 1;
  const std::int32_t column = std::clamp(_column, 0, last);
  const std::int32_t row = std::clamp(_row, 0, last);
  return static_cast<float>(_view.samples[static_cast<std::size_t>(row) * _view.samplesPerSide + column]);
}

/// The normal at a SAMPLE, from central differences across its neighbours. The clamp above makes
/// the edge one-sided, which is the right answer there and not a special case.
[[nodiscard]] std::array<float, 3> SampleNormal(const HeightView& _view, std::int32_t _column, std::int32_t _row) noexcept
{
  const float spacing = static_cast<float>(std::max(_view.spacingWorldUnits, 1));
  const float acrossX = Sample(_view, _column + 1, _row) - Sample(_view, _column - 1, _row);
  const float acrossZ = Sample(_view, _column, _row + 1) - Sample(_view, _column, _row - 1);
  // The surface is y = h(x, z), whose normal is (-dh/dx, 1, -dh/dz). FLAT GROUND GIVES EXACTLY
  // (0, 1, 0): both differences are zero, the vector is already unit length, and the divide below
  // is by exactly one - so the pass that branches on this can compare for equality and mean it.
  const std::array<float, 3> normal{-acrossX / (2.0f * spacing), 1.0f, -acrossZ / (2.0f * spacing)};
  const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
  return {normal[0] / length, normal[1] / length, normal[2] / length};
}

/// Möller-Trumbore, in the one place a triangle is intersected. Returns the distance along a UNIT
/// direction, or a negative number for a miss; back faces hit as readily as front ones, because a
/// commander looking up a cliff from below is still pointing at it.
[[nodiscard]] float RayAgainstTriangle(const std::array<float, 3>& _origin, const std::array<float, 3>& _direction,
                                       const std::array<float, 3>& _a, const std::array<float, 3>& _b,
                                       const std::array<float, 3>& _c) noexcept
{
  constexpr float EPSILON = 1.0e-7f;
  const std::array<float, 3> edge1{_b[0] - _a[0], _b[1] - _a[1], _b[2] - _a[2]};
  const std::array<float, 3> edge2{_c[0] - _a[0], _c[1] - _a[1], _c[2] - _a[2]};
  const std::array<float, 3> across{_direction[1] * edge2[2] - _direction[2] * edge2[1],
                                    _direction[2] * edge2[0] - _direction[0] * edge2[2],
                                    _direction[0] * edge2[1] - _direction[1] * edge2[0]};
  const float determinant = edge1[0] * across[0] + edge1[1] * across[1] + edge1[2] * across[2];
  if (std::abs(determinant) < EPSILON)
  {
    return -1.0f; // Parallel to the triangle's plane.
  }
  const float inverse = 1.0f / determinant;
  const std::array<float, 3> toOrigin{_origin[0] - _a[0], _origin[1] - _a[1], _origin[2] - _a[2]};
  const float u = inverse * (toOrigin[0] * across[0] + toOrigin[1] * across[1] + toOrigin[2] * across[2]);
  if (u < 0.0f || u > 1.0f)
  {
    return -1.0f;
  }
  const std::array<float, 3> other{toOrigin[1] * edge1[2] - toOrigin[2] * edge1[1], toOrigin[2] * edge1[0] - toOrigin[0] * edge1[2],
                                   toOrigin[0] * edge1[1] - toOrigin[1] * edge1[0]};
  const float v = inverse * (_direction[0] * other[0] + _direction[1] * other[1] + _direction[2] * other[2]);
  if (v < 0.0f || u + v > 1.0f)
  {
    return -1.0f;
  }
  const float distance = inverse * (edge2[0] * other[0] + edge2[1] * other[1] + edge2[2] * other[2]);
  return distance > EPSILON ? distance : -1.0f;
}

/// The part of the ray inside the landscape rectangle, as a range of distances. False when it
/// never enters: a slab test in x and z, with the y axis left alone because the march below is
/// what decides whether the ray ever comes down.
///
/// IT IS AN EARLY-OUT AND NOT A CORRECTNESS CONDITION, which is worth saying because a test cannot
/// tell: a ray that never enters the rectangle meets none of its triangles either, so the march
/// would answer miss anyway - after walking cells to find it out. What the clip buys is where the
/// march STARTS, which is the cell the ray crosses the boundary in rather than the one nearest its
/// origin, and that is not an optimisation at all.
[[nodiscard]] bool ClipToLandscape(const HeightView& _view, const std::array<float, 3>& _origin, const std::array<float, 3>& _direction,
                                   float& _outEnter, float& _outExit) noexcept
{
  const float extent = GroundExtent(_view);
  float enter = 0.0f;
  float exit = 1.0e30f;
  for (const std::size_t axis : {std::size_t{0}, std::size_t{2}})
  {
    if (std::abs(_direction[axis]) < 1.0e-9f)
    {
      if (_origin[axis] < 0.0f || _origin[axis] > extent)
      {
        return false; // Parallel to this pair of edges and outside them.
      }
      continue;
    }
    const float inverse = 1.0f / _direction[axis];
    // NOT `near` AND `far`, which are macros of the Windows SDK (AGENTS.md §3) and which
    // Build/CheckProjectFiles.py refuses on sight.
    float first = (0.0f - _origin[axis]) * inverse;
    float last = (extent - _origin[axis]) * inverse;
    if (first > last)
    {
      std::swap(first, last);
    }
    enter = std::max(enter, first);
    exit = std::min(exit, last);
  }
  _outEnter = enter;
  _outExit = exit;
  return exit > enter;
}

} // namespace

float GroundExtent(const HeightView& _view) noexcept
{
  if (_view.samplesPerSide == 0)
  {
    return 0.0f;
  }
  return static_cast<float>(_view.samplesPerSide - 1) * static_cast<float>(_view.spacingWorldUnits);
}

float GroundHeight(const HeightView& _view, float _x, float _z) noexcept
{
  const float spacing = static_cast<float>(std::max(_view.spacingWorldUnits, 1));
  const float column = _x / spacing;
  const float row = _z / spacing;
  const auto lowColumn = static_cast<std::int32_t>(std::floor(column));
  const auto lowRow = static_cast<std::int32_t>(std::floor(row));
  const float alongX = column - static_cast<float>(lowColumn);
  const float alongZ = row - static_cast<float>(lowRow);
  const float top = Sample(_view, lowColumn, lowRow) * (1.0f - alongX) + Sample(_view, lowColumn + 1, lowRow) * alongX;
  const float bottom = Sample(_view, lowColumn, lowRow + 1) * (1.0f - alongX) + Sample(_view, lowColumn + 1, lowRow + 1) * alongX;
  return top * (1.0f - alongZ) + bottom * alongZ;
}

std::array<float, 3> GroundNormal(const HeightView& _view, float _x, float _z) noexcept
{
  const float spacing = static_cast<float>(std::max(_view.spacingWorldUnits, 1));
  const float column = _x / spacing;
  const float row = _z / spacing;
  const auto lowColumn = static_cast<std::int32_t>(std::floor(column));
  const auto lowRow = static_cast<std::int32_t>(std::floor(row));
  const float alongX = column - static_cast<float>(lowColumn);
  const float alongZ = row - static_cast<float>(lowRow);
  const std::array<std::array<float, 3>, 4> corners{SampleNormal(_view, lowColumn, lowRow), SampleNormal(_view, lowColumn + 1, lowRow),
                                                    SampleNormal(_view, lowColumn, lowRow + 1),
                                                    SampleNormal(_view, lowColumn + 1, lowRow + 1)};
  std::array<float, 3> mixed{};
  for (std::size_t axis = 0; axis < 3; ++axis)
  {
    const float top = corners[0][axis] * (1.0f - alongX) + corners[1][axis] * alongX;
    const float bottom = corners[2][axis] * (1.0f - alongX) + corners[3][axis] * alongX;
    mixed[axis] = top * (1.0f - alongZ) + bottom * alongZ;
  }
  // FOUR (0, 1, 0)s MIX TO EXACTLY (0, 1, 0), and the length below is exactly one, so flat ground
  // comes out of here bit for bit what the pass compares against.
  const float length = std::sqrt(mixed[0] * mixed[0] + mixed[1] * mixed[1] + mixed[2] * mixed[2]);
  if (length < 1.0e-12f)
  {
    return {0.0f, 1.0f, 0.0f};
  }
  return {mixed[0] / length, mixed[1] / length, mixed[2] / length};
}

GroundHit RayAgainstGround(const HeightView& _view, const std::array<float, 3>& _origin, const std::array<float, 3>& _direction) noexcept
{
  GroundHit miss{};
  const float length = std::sqrt(_direction[0] * _direction[0] + _direction[1] * _direction[1] + _direction[2] * _direction[2]);
  if (_view.samples == nullptr || _view.samplesPerSide < 2 || length < 1.0e-12f)
  {
    return miss;
  }
  const std::array<float, 3> direction{_direction[0] / length, _direction[1] / length, _direction[2] / length};

  float enter = 0.0f;
  float exit = 0.0f;
  if (!ClipToLandscape(_view, _origin, direction, enter, exit))
  {
    return miss;
  }

  const float spacing = static_cast<float>(std::max(_view.spacingWorldUnits, 1));
  const auto lastCell = static_cast<std::int32_t>(_view.samplesPerSide) - 2;
  const auto at = [&](float _distance, std::size_t _axis) { return _origin[_axis] + direction[_axis] * _distance; };

  // The cell the ray is in where it enters, nudged inside so that a ray entering exactly on an
  // edge starts in the cell it is about to cross rather than in the one behind it.
  auto column = static_cast<std::int32_t>(std::floor(at(enter + 1.0e-4f, 0) / spacing));
  auto row = static_cast<std::int32_t>(std::floor(at(enter + 1.0e-4f, 2) / spacing));
  column = std::clamp(column, 0, lastCell);
  row = std::clamp(row, 0, lastCell);

  // Amanatides and Woo: how far along the ray one cell of each axis is, and how far to the next
  // boundary. A direction with no component on an axis never crosses one, which is the infinity.
  const std::int32_t stepX = direction[0] > 0.0f ? 1 : (direction[0] < 0.0f ? -1 : 0);
  const std::int32_t stepZ = direction[2] > 0.0f ? 1 : (direction[2] < 0.0f ? -1 : 0);
  const float perCellX = stepX == 0 ? 1.0e30f : std::abs(spacing / direction[0]);
  const float perCellZ = stepZ == 0 ? 1.0e30f : std::abs(spacing / direction[2]);
  const auto boundary = [&](std::int32_t _cell, std::int32_t _step, std::size_t _axis, float _perCell)
  {
    if (_step == 0)
    {
      return 1.0e30f;
    }
    const float edge = static_cast<float>(_step > 0 ? _cell + 1 : _cell) * spacing;
    const float toEdge = (edge - at(enter, _axis)) / direction[_axis];
    return enter + std::max(toEdge, 0.0f) + (toEdge < 0.0f ? _perCell : 0.0f);
  };
  float nextX = boundary(column, stepX, 0, perCellX);
  float nextZ = boundary(row, stepZ, 2, perCellZ);

  float hitAt = -1.0f;
  while (column >= 0 && column <= lastCell && row >= 0 && row <= lastCell)
  {
    const float x0 = static_cast<float>(column) * spacing;
    const float x1 = static_cast<float>(column + 1) * spacing;
    const float z0 = static_cast<float>(row) * spacing;
    const float z1 = static_cast<float>(row + 1) * spacing;
    const std::array<float, 3> corner00{x0, Sample(_view, column, row), z0};
    const std::array<float, 3> corner10{x1, Sample(_view, column + 1, row), z0};
    const std::array<float, 3> corner01{x0, Sample(_view, column, row + 1), z1};
    const std::array<float, 3> corner11{x1, Sample(_view, column + 1, row + 1), z1};
    // TerrainChunk.cpp's two triangles, in its own order, so the diagonal is the drawn one.
    const float first = RayAgainstTriangle(_origin, direction, corner00, corner10, corner01);
    const float second = RayAgainstTriangle(_origin, direction, corner11, corner01, corner10);
    const float nearer = first < 0.0f ? second : (second < 0.0f ? first : std::min(first, second));
    if (nearer >= 0.0f)
    {
      hitAt = nearer;
      break;
    }
    if (nextX <= nextZ)
    {
      column += stepX;
      nextX += perCellX;
    }
    else
    {
      row += stepZ;
      nextZ += perCellZ;
    }
    if (std::min(nextX, nextZ) - std::min(perCellX, perCellZ) > exit)
    {
      break; // Past the far side of the landscape.
    }
  }

  // THE SEA IS A PLANE AND NOT THE SEABED. The terrain mesh carries on down under the water, so a
  // ray pointed at the sea hits the bottom of it - which is where the ring would sit, sunk, and at
  // the WRONG POINT as well: the seabed is further along the ray than the surface the commander is
  // looking at. So the water's own plane is intersected, and it wins when it is nearer and the
  // ground under that crossing is below it. m1-vertical-slice/K7's acceptance says "the hit's
  // height is raised", which fixes the sinking and leaves the position wrong; this fixes both, and
  // the task's notes say so rather than the difference being silent.
  const float water = static_cast<float>(_view.waterLevel);
  if (direction[1] < -1.0e-9f)
  {
    const float toWater = (water - _origin[1]) / direction[1];
    if (toWater > 0.0f && toWater >= enter && toWater <= exit && (hitAt < 0.0f || toWater < hitAt))
    {
      const float x = at(toWater, 0);
      const float z = at(toWater, 2);
      if (x >= 0.0f && x <= GroundExtent(_view) && z >= 0.0f && z <= GroundExtent(_view) && GroundHeight(_view, x, z) < water)
      {
        GroundHit sea{};
        sea.hit = true;
        sea.x = x;
        sea.y = water + RING_WATER_CLEARANCE;
        sea.z = z;
        sea.ny = 1.0f;
        sea.distance = toWater;
        sea.water = true;
        return sea;
      }
    }
  }

  if (hitAt < 0.0f)
  {
    return miss;
  }
  GroundHit hit{};
  hit.hit = true;
  hit.x = at(hitAt, 0);
  hit.y = at(hitAt, 1);
  hit.z = at(hitAt, 2);
  const std::array<float, 3> normal = GroundNormal(_view, hit.x, hit.z);
  hit.nx = normal[0];
  hit.ny = normal[1];
  hit.nz = normal[2];
  hit.distance = hitAt;
  return hit;
}

} // namespace Neuron
