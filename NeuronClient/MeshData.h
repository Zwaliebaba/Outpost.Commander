#pragma once

namespace Neuron
{
// One vertex as the mesh pipeline reads it: a position and a normal, in meters once the mesh is fitted (ADR-011).
struct MeshVertex
{
  DirectX::XMFLOAT3 position;
  DirectX::XMFLOAT3 normal;
};

// A place on a mesh where something attaches, such as where a gun fires from or an engine's exhaust leaves (ADR-018).
// The engine carries the tag and never reads it: what a tag means is the game's (R9).
struct MeshHardpoint
{
  std::string tag;
  DirectX::XMFLOAT3 position{};
  // Unit vectors at right angles: where the hardpoint points, and which way is its up.
  DirectX::XMFLOAT3 forward{};
  DirectX::XMFLOAT3 up{};
  // How big it is, in the mesh's units: an exhaust's radius, say.
  float size = 0.0f;
};

// A run of a mesh's triangles that spins, such as a radar on its mast (ADR-045). It turns a whole turn each period about
// the axis through its pivot, the way XMMatrixRotationAxis turns a point as the angle grows. Its triangles are stored as
// they stand when it has not turned.
struct MeshPart
{
  std::uint32_t firstIndex = 0;
  std::uint32_t indexCount = 0;
  DirectX::XMFLOAT3 pivot{};
  // A unit vector.
  DirectX::XMFLOAT3 axis{};
  float periodSeconds = 0.0f;
};

// A triangle list on the CPU, its hardpoints and its spinning parts: what an .nmf file holds (ADR-018, ADR-045). The
// mesh's front is +x and its up +y, and the triangles wind clockwise seen from their front, Direct3D's default. The
// triangles that stand still come first, and the parts' runs of indices follow each other to the end.
struct MeshData
{
  std::vector<MeshVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<MeshHardpoint> hardpoints;
  std::vector<MeshPart> parts;
  DirectX::XMFLOAT3 boundsMin{};
  DirectX::XMFLOAT3 boundsMax{};

  // The size of the bounds along each axis.
  [[nodiscard]] DirectX::XMFLOAT3 Extents() const noexcept
  {
    return {boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y, boundsMax.z - boundsMin.z};
  }

  // How many of the indices, from the first, are the triangles that stand still.
  [[nodiscard]] std::uint32_t FixedIndexCount() const noexcept
  {
    return parts.empty() ? static_cast<std::uint32_t>(indices.size()) : parts.front().firstIndex;
  }
};

// Reads the bytes of an .nmf file, the format Tools/BakeMeshes.py writes (ADR-018, ADR-045). Throws Neuron::Exception
// naming _fileName and the byte offset when the file is not version 2 of the format, is cut short or has bytes left
// over, has no triangles or an index past its vertices, holds a number that is not finite, has a hardpoint whose tag is
// not lowercase letters and digits, whose directions are not unit vectors at right angles, or whose size is not
// positive, or has a part whose axis is not a unit vector or whose period is not positive, or whose runs of indices do
// not follow the triangles that stand still, and each other, to the end in whole triangles.
[[nodiscard]] MeshData ParseNmf(std::span<const std::uint8_t> _bytes, std::string_view _fileName);

// Puts a mesh at its size in the game: the center of its bounds at the origin, and scaled uniformly so that its length
// along x, its front, is _lengthMeters. Its hardpoints and its parts' pivots move and scale with it; directions do not
// change.
void FitMesh(MeshData& _mesh, float _lengthMeters);

// Puts a mesh at its size in the game as FitMesh does, but scaled so that the wider of its length along x and its depth
// along z is _widestMeters: it stands within a square that wide on the ground.
void FitMeshAcross(MeshData& _mesh, float _widestMeters);

// The center of a mesh's bounds moved to the origin, and the mesh scaled uniformly about it by _scale, its hardpoints and
// its parts' pivots with it, and its bounds measured again.
void CenterAndScale(MeshData& _mesh, float _scale);

// The triangles of _indexCount of _mesh's indices from _firstIndex, as a mesh of their own: the vertices they use, in the
// order they first use them, and their bounds. It has no hardpoints and no parts.
[[nodiscard]] MeshData MeshPiece(const MeshData& _mesh, std::uint32_t _firstIndex, std::uint32_t _indexCount);

// The world matrix that draws _part _seconds after it started turning, on a mesh drawn by _world.
[[nodiscard]] DirectX::XMFLOAT4X4 PartWorld(const MeshPart& _part, double _seconds, const DirectX::XMFLOAT4X4& _world) noexcept;

// The edges of _mesh's triangles where its surface bends by more than _minAngleRadians, and the edges only one triangle
// has, as a line list: two vertices an edge, with indices counting up from zero. Corners are matched by position, since a
// flat-shaded mesh repeats a corner once for each triangle at it. Each vertex's normal is the mean of the normals of the
// faces that meet at the edge, so that the line is lit as the surface beside it is. The lines lie on the surface; what
// keeps them in front of it is MeshPipeline::DrawLines (ADR-040). The bounds are the mesh's. A mesh with no such edge
// gives no vertices.
[[nodiscard]] MeshData BuildCreaseLines(const MeshData& _mesh, float _minAngleRadians);

// The height of _mesh's highest surface over the point (_x, _z): where a line straight down through that point first
// meets one of its triangles. Nothing when the line misses every triangle.
[[nodiscard]] std::optional<float> SurfaceHeightAt(const MeshData& _mesh, float _x, float _z);
} // namespace Neuron
