#include "pch.h"
#include "MeshData.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
// The .cmo layout, as DirectXTK's CMO reader defines it (ADR-011). Sizes are in bytes.
constexpr size_t MATERIAL_CONSTANTS_BYTES = 132;
constexpr size_t TEXTURES_PER_MATERIAL = 8;
constexpr size_t SUBMESH_BYTES = 20;
constexpr size_t FILE_VERTEX_BYTES = 52;
constexpr size_t SKINNING_VERTEX_BYTES = 32;
constexpr size_t EXTENTS_BYTES = 40;

// Reads the file front to back. Every read checks that the bytes are there, so a short file is an error and never a
// read past the end.
class CmoReader
{
public:
  CmoReader(std::span<const std::uint8_t> _bytes, std::string_view _fileName)
    : m_bytes(_bytes),
      m_fileName(_fileName)
  {
  }

  [[noreturn]] void Fail(std::string_view _message) const
  {
    throw Neuron::Exception(std::format("The mesh {} cannot be read: {} (at byte {}).", m_fileName, _message, m_offset));
  }

  std::span<const std::uint8_t> Take(size_t _count)
  {
    if (_count > m_bytes.size() - m_offset)
      Fail("the file ends early");
    const auto taken = m_bytes.subspan(m_offset, _count);
    m_offset += _count;
    return taken;
  }

  template <typename T> T Read()
  {
    T value;
    std::memcpy(&value, Take(sizeof(T)).data(), sizeof(T));
    return value;
  }

  // A count followed by that many items of _itemBytes each; the product is checked before it is used.
  std::span<const std::uint8_t> TakeArray(std::uint32_t _count, size_t _itemBytes)
  {
    if (_count > (m_bytes.size() - m_offset) / _itemBytes)
      Fail("the file ends early");
    return Take(_count * _itemBytes);
  }

  // A name: a count of UTF-16 code units, then the units. The game does not use the text.
  void SkipName()
  {
    (void)TakeArray(Read<std::uint32_t>(), sizeof(char16_t));
  }

  [[nodiscard]] bool AtEnd() const noexcept
  {
    return m_offset == m_bytes.size();
  }

private:
  std::span<const std::uint8_t> m_bytes;
  std::string_view m_fileName;
  size_t m_offset = 0;
};

struct Submesh
{
  std::uint32_t materialIndex;
  std::uint32_t indexBufferIndex;
  std::uint32_t vertexBufferIndex;
  std::uint32_t startIndex;
  std::uint32_t primitiveCount;
};

void ReadMesh(CmoReader& _reader, Neuron::MeshData& _mesh)
{
  _reader.SkipName();

  const auto materialCount = _reader.Read<std::uint32_t>();
  for (std::uint32_t i = 0; i < materialCount; ++i)
  {
    _reader.SkipName();
    (void)_reader.Take(MATERIAL_CONSTANTS_BYTES);
    _reader.SkipName(); // the pixel shader
    for (size_t texture = 0; texture < TEXTURES_PER_MATERIAL; ++texture)
      _reader.SkipName();
  }

  if (_reader.Read<std::uint8_t>() != 0)
    _reader.Fail("it has a skeleton, and the game draws rigid meshes only");

  std::vector<Submesh> submeshes(_reader.Read<std::uint32_t>());
  for (Submesh& submesh : submeshes)
  {
    const auto bytes = _reader.Take(SUBMESH_BYTES);
    std::memcpy(&submesh, bytes.data(), SUBMESH_BYTES);
  }

  std::vector<std::vector<std::uint16_t>> indexBuffers(_reader.Read<std::uint32_t>());
  for (auto& indices : indexBuffers)
  {
    const auto count = _reader.Read<std::uint32_t>();
    const auto bytes = _reader.TakeArray(count, sizeof(std::uint16_t));
    indices.resize(count);
    std::memcpy(indices.data(), bytes.data(), bytes.size());
  }

  std::vector<std::vector<Neuron::MeshVertex>> vertexBuffers(_reader.Read<std::uint32_t>());
  for (auto& vertices : vertexBuffers)
  {
    const auto count = _reader.Read<std::uint32_t>();
    const auto bytes = _reader.TakeArray(count, FILE_VERTEX_BYTES);
    vertices.resize(count);
    // A file vertex starts with its position and normal; its tangent, color and texture coordinates follow.
    for (std::uint32_t v = 0; v < count; ++v)
      std::memcpy(&vertices[v], bytes.data() + (v * FILE_VERTEX_BYTES), sizeof(Neuron::MeshVertex));
  }

  // Skinning vertex buffers are written even for a mesh without a skeleton, as an empty list.
  const auto skinningBufferCount = _reader.Read<std::uint32_t>();
  for (std::uint32_t i = 0; i < skinningBufferCount; ++i)
    (void)_reader.TakeArray(_reader.Read<std::uint32_t>(), SKINNING_VERTEX_BYTES);

  (void)_reader.Take(EXTENTS_BYTES);

  // Each submesh draws a range of one index buffer into one vertex buffer. The game draws each mesh whole, so the
  // ranges are appended to one list, with the vertex buffers after each other.
  std::vector<std::uint32_t> firstVertex(vertexBuffers.size());
  for (size_t i = 0; i < vertexBuffers.size(); ++i)
  {
    firstVertex[i] = static_cast<std::uint32_t>(_mesh.vertices.size());
    _mesh.vertices.insert(_mesh.vertices.end(), vertexBuffers[i].begin(), vertexBuffers[i].end());
  }
  for (const Submesh& submesh : submeshes)
  {
    if (submesh.indexBufferIndex >= indexBuffers.size() || submesh.vertexBufferIndex >= vertexBuffers.size())
      _reader.Fail("a submesh names a buffer the mesh does not have");
    const auto& indices = indexBuffers[submesh.indexBufferIndex];
    const size_t indexCount = static_cast<size_t>(submesh.primitiveCount) * 3;
    if (submesh.startIndex > indices.size() || indexCount > indices.size() - submesh.startIndex)
      _reader.Fail("a submesh's triangles run past its index buffer");
    const size_t vertexCount = vertexBuffers[submesh.vertexBufferIndex].size();
    for (size_t i = submesh.startIndex; i < submesh.startIndex + indexCount; ++i)
    {
      if (indices[i] >= vertexCount)
        _reader.Fail("an index is past the end of its vertex buffer");
      _mesh.indices.push_back(firstVertex[submesh.vertexBufferIndex] + indices[i]);
    }
  }
}

void MeasureBounds(Neuron::MeshData& _mesh) noexcept
{
  constexpr float BIG = std::numeric_limits<float>::max();
  DirectX::XMFLOAT3 low{BIG, BIG, BIG};
  DirectX::XMFLOAT3 high{-BIG, -BIG, -BIG};
  for (const Neuron::MeshVertex& vertex : _mesh.vertices)
  {
    low = {std::min(low.x, vertex.position.x), std::min(low.y, vertex.position.y), std::min(low.z, vertex.position.z)};
    high = {std::max(high.x, vertex.position.x), std::max(high.y, vertex.position.y), std::max(high.z, vertex.position.z)};
  }
  _mesh.boundsMin = low;
  _mesh.boundsMax = high;
}

// Turns a point about y so that _forward becomes +x. Each of these is a rotation, never a mirror, so the triangles keep
// their winding.
DirectX::XMFLOAT3 FaceForward(const DirectX::XMFLOAT3& _point, Neuron::MeshAxis _forward) noexcept
{
  switch (_forward)
  {
  case Neuron::MeshAxis::NegativeX:
    return {-_point.x, _point.y, -_point.z};
  case Neuron::MeshAxis::PositiveZ:
    return {_point.z, _point.y, -_point.x};
  case Neuron::MeshAxis::NegativeZ:
    return {-_point.z, _point.y, _point.x};
  case Neuron::MeshAxis::PositiveX:
  default:
    return _point;
  }
}
} // namespace

Neuron::MeshData Neuron::ParseCmo(std::span<const std::uint8_t> _bytes, std::string_view _fileName)
{
  CmoReader reader(_bytes, _fileName);
  MeshData mesh;
  const auto meshCount = reader.Read<std::uint32_t>();
  for (std::uint32_t i = 0; i < meshCount; ++i)
    ReadMesh(reader, mesh);
  if (!reader.AtEnd())
    reader.Fail("there are bytes after the last mesh");
  if (mesh.indices.empty())
    reader.Fail("it has no triangles");
  MeasureBounds(mesh);
  return mesh;
}

void Neuron::OrientMesh(MeshData& _mesh, MeshAxis _forward, float _lengthMeters)
{
  const DirectX::XMFLOAT3 center{(_mesh.boundsMin.x + _mesh.boundsMax.x) / 2.0f, (_mesh.boundsMin.y + _mesh.boundsMax.y) / 2.0f,
                                 (_mesh.boundsMin.z + _mesh.boundsMax.z) / 2.0f};
  const DirectX::XMFLOAT3 extents = FaceForward(_mesh.Extents(), _forward);
  const float lengthInFile = std::abs(extents.x);
  if (lengthInFile <= 0.0f)
    throw Exception("A mesh with no length along its forward axis cannot be scaled to one.");
  const float scale = _lengthMeters / lengthInFile;

  for (MeshVertex& vertex : _mesh.vertices)
  {
    const DirectX::XMFLOAT3 centered{vertex.position.x - center.x, vertex.position.y - center.y, vertex.position.z - center.z};
    const DirectX::XMFLOAT3 turned = FaceForward(centered, _forward);
    vertex.position = {turned.x * scale, turned.y * scale, turned.z * scale};
    vertex.normal = FaceForward(vertex.normal, _forward);
  }
  MeasureBounds(_mesh);
}