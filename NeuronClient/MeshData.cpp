#include "pch.h"
#include "MeshData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <utility>

namespace
{
// The .nmf layout, version 1 (ADR-018). Sizes are in bytes.
constexpr std::array<std::uint8_t, 4> NMF_MAGIC{'N', 'M', 'F', '\0'};
constexpr std::uint32_t NMF_VERSION = 1;
constexpr size_t FILE_VERTEX_BYTES = 6 * sizeof(float);
static_assert(sizeof(Neuron::MeshVertex) == FILE_VERTEX_BYTES, "A file vertex is copied straight into a MeshVertex.");
constexpr size_t MAXIMUM_TAG_LENGTH = 31;
// Unit vectors and right angles are checked to this, far looser than float32 rounding and far tighter than a mistake.
constexpr float DIRECTION_TOLERANCE = 1e-3f;

// Reads the file front to back. Every read checks that the bytes are there, so a short file is an error and never a
// read past the end.
class NmfReader
{
public:
  NmfReader(std::span<const std::uint8_t> _bytes, std::string_view _fileName)
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

  // That many items of _itemBytes each; the product is checked before it is used.
  std::span<const std::uint8_t> TakeArray(std::uint32_t _count, size_t _itemBytes)
  {
    if (_count > (m_bytes.size() - m_offset) / _itemBytes)
      Fail("the file ends early");
    return Take(_count * _itemBytes);
  }

  DirectX::XMFLOAT3 ReadFinite3()
  {
    const DirectX::XMFLOAT3 value{Read<float>(), Read<float>(), Read<float>()};
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
      Fail("a number is not finite");
    return value;
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

float Dot(const DirectX::XMFLOAT3& _a, const DirectX::XMFLOAT3& _b) noexcept
{
  return (_a.x * _b.x) + (_a.y * _b.y) + (_a.z * _b.z);
}

bool IsUnit(const DirectX::XMFLOAT3& _vector) noexcept
{
  return std::abs(std::sqrt(Dot(_vector, _vector)) - 1.0f) <= DIRECTION_TOLERANCE;
}

// A tag is lowercase ASCII letters and digits, starting with a letter, as the baker allows.
bool IsTag(std::string_view _tag) noexcept
{
  const auto isLetter = [](char _c) { return _c >= 'a' && _c <= 'z'; };
  return !_tag.empty() && isLetter(_tag.front()) &&
         std::ranges::all_of(_tag, [&](char _c) { return isLetter(_c) || (_c >= '0' && _c <= '9'); });
}

Neuron::MeshHardpoint ReadHardpoint(NmfReader& _reader)
{
  const auto tagLength = _reader.Read<std::uint8_t>();
  if (tagLength == 0 || tagLength > MAXIMUM_TAG_LENGTH)
    _reader.Fail("a hardpoint's tag is empty or too long");
  const auto tagBytes = _reader.Take(tagLength);
  Neuron::MeshHardpoint hardpoint{.tag = std::string(reinterpret_cast<const char*>(tagBytes.data()), tagBytes.size())};
  if (!IsTag(hardpoint.tag))
    _reader.Fail("a hardpoint's tag is not lowercase letters and digits");
  hardpoint.position = _reader.ReadFinite3();
  hardpoint.forward = _reader.ReadFinite3();
  hardpoint.up = _reader.ReadFinite3();
  hardpoint.size = _reader.Read<float>();
  if (!IsUnit(hardpoint.forward) || !IsUnit(hardpoint.up) || std::abs(Dot(hardpoint.forward, hardpoint.up)) > DIRECTION_TOLERANCE)
    _reader.Fail(std::format("the hardpoint \"{}\" has directions that are not unit vectors at right angles", hardpoint.tag));
  if (!std::isfinite(hardpoint.size) || hardpoint.size <= 0.0f)
    _reader.Fail(std::format("the hardpoint \"{}\" has no size", hardpoint.tag));
  return hardpoint;
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
} // namespace

Neuron::MeshData Neuron::ParseNmf(std::span<const std::uint8_t> _bytes, std::string_view _fileName)
{
  NmfReader reader(_bytes, _fileName);
  if (!std::ranges::equal(reader.Take(NMF_MAGIC.size()), NMF_MAGIC))
    reader.Fail("it is not an .nmf file");
  if (reader.Read<std::uint32_t>() != NMF_VERSION)
    reader.Fail(std::format("it is not version {} of the format", NMF_VERSION));
  const auto vertexCount = reader.Read<std::uint32_t>();
  const auto indexCount = reader.Read<std::uint32_t>();
  const auto hardpointCount = reader.Read<std::uint32_t>();
  if (reader.Read<std::uint32_t>() != 0)
    reader.Fail("its flags are not zero");
  if (indexCount == 0 || indexCount % 3 != 0)
    reader.Fail("it is not a whole number of triangles, or has none");

  MeshData mesh;
  const auto vertexBytes = reader.TakeArray(vertexCount, FILE_VERTEX_BYTES);
  mesh.vertices.resize(vertexCount);
  std::memcpy(mesh.vertices.data(), vertexBytes.data(), vertexBytes.size());
  for (const MeshVertex& vertex : mesh.vertices)
  {
    for (const float value : {vertex.position.x, vertex.position.y, vertex.position.z, vertex.normal.x, vertex.normal.y, vertex.normal.z})
    {
      if (!std::isfinite(value))
        reader.Fail("a vertex is not finite");
    }
  }

  const auto indexBytes = reader.TakeArray(indexCount, sizeof(std::uint32_t));
  mesh.indices.resize(indexCount);
  std::memcpy(mesh.indices.data(), indexBytes.data(), indexBytes.size());
  if (std::ranges::any_of(mesh.indices, [vertexCount](std::uint32_t _index) { return _index >= vertexCount; }))
    reader.Fail("an index is past the last vertex");

  // Read one at a time, so a count the file cannot hold fails where the file ends rather than reserving memory first.
  for (std::uint32_t i = 0; i < hardpointCount; ++i)
    mesh.hardpoints.push_back(ReadHardpoint(reader));
  if (!reader.AtEnd())
    reader.Fail("there are bytes after the last hardpoint");

  MeasureBounds(mesh);
  return mesh;
}

void Neuron::FitMesh(MeshData& _mesh, float _lengthMeters)
{
  const DirectX::XMFLOAT3 center{(_mesh.boundsMin.x + _mesh.boundsMax.x) / 2.0f, (_mesh.boundsMin.y + _mesh.boundsMax.y) / 2.0f,
                                 (_mesh.boundsMin.z + _mesh.boundsMax.z) / 2.0f};
  const float lengthInFile = _mesh.Extents().x;
  if (lengthInFile <= 0.0f)
    throw Exception("A mesh with no length along its front cannot be scaled to one.");
  const float scale = _lengthMeters / lengthInFile;
  const auto fit = [&center, scale](const DirectX::XMFLOAT3& _point) -> DirectX::XMFLOAT3
  { return {(_point.x - center.x) * scale, (_point.y - center.y) * scale, (_point.z - center.z) * scale}; };

  for (MeshVertex& vertex : _mesh.vertices)
    vertex.position = fit(vertex.position);
  for (MeshHardpoint& hardpoint : _mesh.hardpoints)
  {
    hardpoint.position = fit(hardpoint.position);
    hardpoint.size *= scale;
  }
  MeasureBounds(_mesh);
}

Neuron::MeshData Neuron::BuildCreaseLines(const MeshData& _mesh, float _minAngleRadians, float _liftShare)
{
  // Each corner once, by position.
  std::map<std::array<float, 3>, std::uint32_t> cornerAt;
  std::vector<DirectX::XMFLOAT3> corners;
  std::vector<std::uint32_t> cornerOf(_mesh.vertices.size());
  for (size_t i = 0; i < _mesh.vertices.size(); ++i)
  {
    const DirectX::XMFLOAT3& position = _mesh.vertices[i].position;
    const auto [found, added] = cornerAt.try_emplace({position.x, position.y, position.z}, static_cast<std::uint32_t>(corners.size()));
    if (added)
      corners.push_back(position);
    cornerOf[i] = found->second;
  }

  // Each edge, by its two corners in order, and the normals of the faces that have it. A triangle with no area has none.
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<DirectX::XMFLOAT3>> faceNormals;
  for (size_t i = 0; i + 2 < _mesh.indices.size(); i += 3)
  {
    const std::array<std::uint32_t, 3> triangle{cornerOf[_mesh.indices[i]], cornerOf[_mesh.indices[i + 1]], cornerOf[_mesh.indices[i + 2]]};
    const DirectX::XMVECTOR a = DirectX::XMLoadFloat3(&corners[triangle[0]]);
    const DirectX::XMVECTOR face = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&corners[triangle[1]]), a),
                                                           DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&corners[triangle[2]]), a));
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(face)) <= 0.0f)
      continue;
    DirectX::XMFLOAT3 normal;
    DirectX::XMStoreFloat3(&normal, DirectX::XMVector3Normalize(face));
    for (size_t side = 0; side < 3; ++side)
    {
      const std::uint32_t from = triangle[side];
      const std::uint32_t to = triangle[(side + 1) % 3];
      faceNormals[{std::min(from, to), std::max(from, to)}].push_back(normal);
    }
  }

  const DirectX::XMFLOAT3 extents = _mesh.Extents();
  const float lift = _liftShare * std::max({extents.x, extents.y, extents.z});
  const float flatCosine = std::cos(_minAngleRadians);
  MeshData lines;
  lines.boundsMin = _mesh.boundsMin;
  lines.boundsMax = _mesh.boundsMax;
  for (const auto& [edge, normals] : faceNormals)
  {
    if (normals.size() == 2)
    {
      const float cosine =
        DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&normals[0]), DirectX::XMLoadFloat3(&normals[1])));
      if (cosine >= flatCosine)
        continue;
    }
    DirectX::XMVECTOR sum = DirectX::XMVectorZero();
    for (const DirectX::XMFLOAT3& normal : normals)
      sum = DirectX::XMVectorAdd(sum, DirectX::XMLoadFloat3(&normal));
    // Faces that fold right back on each other have no mean; the first face's normal stands in.
    const DirectX::XMVECTOR mean = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(sum)) > 1e-6f ? DirectX::XMVector3Normalize(sum)
                                                                                                  : DirectX::XMLoadFloat3(&normals.front());
    MeshVertex vertex{};
    DirectX::XMStoreFloat3(&vertex.normal, mean);
    for (const std::uint32_t corner : {edge.first, edge.second})
    {
      DirectX::XMStoreFloat3(&vertex.position,
                             DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&corners[corner]), DirectX::XMVectorScale(mean, lift)));
      lines.indices.push_back(static_cast<std::uint32_t>(lines.vertices.size()));
      lines.vertices.push_back(vertex);
    }
  }
  return lines;
}
