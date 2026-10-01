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

// A triangle list on the CPU, and its hardpoints: what an .nmf file holds (ADR-018). The mesh's front is +x and its up
// +y, and the triangles wind clockwise seen from their front, Direct3D's default.
struct MeshData
{
  std::vector<MeshVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<MeshHardpoint> hardpoints;
  DirectX::XMFLOAT3 boundsMin{};
  DirectX::XMFLOAT3 boundsMax{};

  // The size of the bounds along each axis.
  [[nodiscard]] DirectX::XMFLOAT3 Extents() const noexcept
  {
    return {boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y, boundsMax.z - boundsMin.z};
  }
};

// Reads the bytes of an .nmf file, the format Tools/BakeMeshes.py writes (ADR-018). Throws Neuron::Exception naming
// _fileName and the byte offset when the file is not version 1 of the format, is cut short or has bytes left over, has
// no triangles or an index past its vertices, holds a number that is not finite, or has a hardpoint whose tag is not
// lowercase letters and digits, whose directions are not unit vectors at right angles, or whose size is not positive.
[[nodiscard]] MeshData ParseNmf(std::span<const std::uint8_t> _bytes, std::string_view _fileName);

// Puts a mesh at its size in the game: the center of its bounds at the origin, and scaled uniformly so that its length
// along x, its front, is _lengthMeters. Its hardpoints move and scale with it; their directions do not change.
void FitMesh(MeshData& _mesh, float _lengthMeters);
} // namespace Neuron
