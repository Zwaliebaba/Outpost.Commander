#pragma once

namespace Neuron
{
// One vertex as the mesh pipeline reads it: a position and a normal, in meters once the mesh is oriented (ADR-011).
struct MeshVertex
{
  DirectX::XMFLOAT3 position;
  DirectX::XMFLOAT3 normal;
};

// The axis a mesh's front points along in its file, seen with y up (ADR-011).
enum class MeshAxis : std::uint8_t
{
  PositiveX,
  NegativeX,
  PositiveZ,
  NegativeZ
};

// A triangle list on the CPU: what a .cmo file holds that the game draws, and nothing it does not (ADR-011). The
// triangles wind clockwise seen from their front, Direct3D's default.
struct MeshData
{
  std::vector<MeshVertex> vertices;
  std::vector<std::uint32_t> indices;
  DirectX::XMFLOAT3 boundsMin{};
  DirectX::XMFLOAT3 boundsMax{};

  // The size of the bounds along each axis.
  [[nodiscard]] DirectX::XMFLOAT3 Extents() const noexcept
  {
    return {boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y, boundsMax.z - boundsMin.z};
  }
};

// Reads the bytes of a .cmo file, the format DirectXMesh's meshconvert writes. Every mesh in the file is kept, as one
// triangle list; materials, tangents, colors and texture coordinates are skipped. Throws Neuron::Exception naming
// _fileName when the file is cut short, has bytes left over, or has a skeleton, an index out of range or a submesh that
// is not a whole number of triangles.
[[nodiscard]] MeshData ParseCmo(std::span<const std::uint8_t> _bytes, std::string_view _fileName);

// Puts a mesh in the game's frame: the center of its bounds at the origin, its front along +x and its length along x
// equal to _lengthMeters. Up stays y, so this turns about y and scales uniformly; normals keep their length.
void OrientMesh(MeshData& _mesh, MeshAxis _forward, float _lengthMeters);
} // namespace Neuron