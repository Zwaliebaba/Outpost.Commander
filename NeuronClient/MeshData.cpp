#include "pch.h"
#include "MeshData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <utility>

namespace
{
// The .nmf layout, version 2 (ADR-018, ADR-045). Sizes are in bytes.
constexpr std::array<std::uint8_t, 4> NMF_MAGIC{'N', 'M', 'F', '\0'};
constexpr std::uint32_t NMF_VERSION = 2;
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

// A part as the file gives it; where its triangles are is checked against the parts before it.
Neuron::MeshPart ReadPart(NmfReader& _reader)
{
  Neuron::MeshPart part{.firstIndex = _reader.Read<std::uint32_t>(), .indexCount = _reader.Read<std::uint32_t>()};
  part.pivot = _reader.ReadFinite3();
  part.axis = _reader.ReadFinite3();
  part.periodSeconds = _reader.Read<float>();
  if (!IsUnit(part.axis))
    _reader.Fail("a part's axis is not a unit vector");
  if (!std::isfinite(part.periodSeconds) || part.periodSeconds <= 0.0f)
    _reader.Fail("a part's period is not positive");
  return part;
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
  const auto partCount = reader.Read<std::uint32_t>();
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
  // Some triangles stand still and come first; each part's follow the ones before them, and the last part's end the file's.
  for (std::uint32_t i = 0; i < partCount; ++i)
  {
    const MeshPart part = ReadPart(reader);
    const std::uint32_t follows = mesh.parts.empty() ? part.firstIndex : mesh.parts.back().firstIndex + mesh.parts.back().indexCount;
    if (part.firstIndex == 0 || part.firstIndex % 3 != 0 || part.firstIndex != follows || part.firstIndex >= indexCount ||
        part.indexCount == 0 || part.indexCount % 3 != 0 || part.indexCount > indexCount - part.firstIndex)
      reader.Fail("a part's triangles do not follow the triangles before them");
    mesh.parts.push_back(part);
  }
  if (!mesh.parts.empty() && mesh.parts.back().firstIndex + mesh.parts.back().indexCount != indexCount)
    reader.Fail("the parts' triangles do not reach the last index");
  if (!reader.AtEnd())
    reader.Fail("there are bytes after the last part");

  MeasureBounds(mesh);
  return mesh;
}

void Neuron::FitMesh(MeshData& _mesh, float _lengthMeters)
{
  const float lengthInFile = _mesh.Extents().x;
  if (lengthInFile <= 0.0f)
    throw Exception("A mesh with no length along its front cannot be scaled to one.");
  CenterAndScale(_mesh, _lengthMeters / lengthInFile);
}

void Neuron::FitMeshAcross(MeshData& _mesh, float _widestMeters)
{
  const DirectX::XMFLOAT3 extents = _mesh.Extents();
  const float widestInFile = std::max(extents.x, extents.z);
  if (widestInFile <= 0.0f)
    throw Exception("A mesh with no width on the ground cannot be scaled to one.");
  CenterAndScale(_mesh, _widestMeters / widestInFile);
}

void Neuron::CenterAndScale(MeshData& _mesh, float _scale)
{
  const DirectX::XMFLOAT3 center{(_mesh.boundsMin.x + _mesh.boundsMax.x) / 2.0f, (_mesh.boundsMin.y + _mesh.boundsMax.y) / 2.0f,
                                 (_mesh.boundsMin.z + _mesh.boundsMax.z) / 2.0f};
  const auto fit = [&center, _scale](const DirectX::XMFLOAT3& _point) -> DirectX::XMFLOAT3
  { return {(_point.x - center.x) * _scale, (_point.y - center.y) * _scale, (_point.z - center.z) * _scale}; };

  for (MeshVertex& vertex : _mesh.vertices)
    vertex.position = fit(vertex.position);
  for (MeshHardpoint& hardpoint : _mesh.hardpoints)
  {
    hardpoint.position = fit(hardpoint.position);
    hardpoint.size *= _scale;
  }
  for (MeshPart& part : _mesh.parts)
    part.pivot = fit(part.pivot);
  MeasureBounds(_mesh);
}

Neuron::MeshData Neuron::MeshPiece(const MeshData& _mesh, std::uint32_t _firstIndex, std::uint32_t _indexCount)
{
  MeshData piece;
  // The piece's vertex for each of the mesh's, NO_VERTEX until the piece first uses it.
  constexpr std::uint32_t NO_VERTEX = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::uint32_t> vertexOf(_mesh.vertices.size(), NO_VERTEX);
  piece.indices.reserve(_indexCount);
  for (const std::uint32_t index : std::span(_mesh.indices).subspan(_firstIndex, _indexCount))
  {
    if (vertexOf[index] == NO_VERTEX)
    {
      vertexOf[index] = static_cast<std::uint32_t>(piece.vertices.size());
      piece.vertices.push_back(_mesh.vertices[index]);
    }
    piece.indices.push_back(vertexOf[index]);
  }
  MeasureBounds(piece);
  return piece;
}

DirectX::XMFLOAT4X4 Neuron::PartWorld(const MeshPart& _part, double _seconds, const DirectX::XMFLOAT4X4& _world) noexcept
{
  // In doubles, so that a match's hours do not wear the angle's precision down.
  const double period = _part.periodSeconds;
  const auto angle = static_cast<float>(2.0 * std::numbers::pi * std::fmod(_seconds, period) / period);
  const DirectX::XMVECTOR pivot = DirectX::XMLoadFloat3(&_part.pivot);
  const DirectX::XMMATRIX turn = DirectX::XMMatrixTranslationFromVector(DirectX::XMVectorNegate(pivot)) *
                                 DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&_part.axis), angle) *
                                 DirectX::XMMatrixTranslationFromVector(pivot);
  DirectX::XMFLOAT4X4 world;
  DirectX::XMStoreFloat4x4(&world, turn * DirectX::XMLoadFloat4x4(&_world));
  return world;
}

Neuron::MeshData Neuron::BuildCreaseLines(const MeshData& _mesh, float _minAngleRadians)
{
  // Each corner once, by position, numbered in the order the vertices first reach it. The vertices are sorted by position,
  // ties kept in their order, so that each run of equal positions starts with the vertex that reaches its corner first.
  // Positions compare as floats, so 0 and -0 are one corner; the corner keeps the first vertex's position.
  const auto vertexCount = static_cast<std::uint32_t>(_mesh.vertices.size());
  const auto positionLess = [&_mesh](std::uint32_t _a, std::uint32_t _b)
  {
    const DirectX::XMFLOAT3& a = _mesh.vertices[_a].position;
    const DirectX::XMFLOAT3& b = _mesh.vertices[_b].position;
    if (a.x != b.x)
      return a.x < b.x;
    if (a.y != b.y)
      return a.y < b.y;
    return a.z < b.z;
  };
  std::vector<std::uint32_t> byPosition(vertexCount);
  std::iota(byPosition.begin(), byPosition.end(), 0u);
  std::ranges::stable_sort(byPosition, positionLess);
  // Each run's first vertex, and the run each vertex is in.
  std::vector<std::uint32_t> runFirst;
  std::vector<std::uint32_t> runOf(vertexCount);
  for (std::uint32_t i = 0; i < vertexCount; ++i)
  {
    if (i == 0 || positionLess(byPosition[i - 1], byPosition[i]))
      runFirst.push_back(byPosition[i]);
    runOf[byPosition[i]] = static_cast<std::uint32_t>(runFirst.size() - 1);
  }
  // The corners in the order their first vertices come.
  std::vector<std::uint32_t> runsInOrder(runFirst.size());
  std::iota(runsInOrder.begin(), runsInOrder.end(), 0u);
  std::ranges::sort(runsInOrder, {}, [&runFirst](std::uint32_t _run) { return runFirst[_run]; });
  std::vector<std::uint32_t> cornerOfRun(runFirst.size());
  std::vector<DirectX::XMFLOAT3> corners(runFirst.size());
  for (std::uint32_t corner = 0; corner < runsInOrder.size(); ++corner)
  {
    cornerOfRun[runsInOrder[corner]] = corner;
    corners[corner] = _mesh.vertices[runFirst[runsInOrder[corner]]].position;
  }
  std::vector<std::uint32_t> cornerOf(vertexCount);
  for (std::uint32_t i = 0; i < vertexCount; ++i)
    cornerOf[i] = cornerOfRun[runOf[i]];

  // Each triangle's side, by its two corners in order, with the triangle's normal. A triangle with no area has none. Sorted
  // by corners, ties kept in the order of the triangles, the sides of one edge come together with their faces' normals in
  // that order.
  struct Side
  {
    std::uint32_t from = 0;
    std::uint32_t to = 0;
    DirectX::XMFLOAT3 normal{};
  };
  std::vector<Side> sides;
  sides.reserve(_mesh.indices.size());
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
      sides.push_back({.from = std::min(from, to), .to = std::max(from, to), .normal = normal});
    }
  }
  std::ranges::stable_sort(sides, [](const Side& _a, const Side& _b) { return _a.from != _b.from ? _a.from < _b.from : _a.to < _b.to; });

  const float flatCosine = std::cos(_minAngleRadians);
  MeshData lines;
  lines.boundsMin = _mesh.boundsMin;
  lines.boundsMax = _mesh.boundsMax;
  for (size_t first = 0; first < sides.size();)
  {
    size_t end = first + 1;
    while (end < sides.size() && sides[end].from == sides[first].from && sides[end].to == sides[first].to)
      ++end;
    const std::span<const Side> edge(sides.data() + first, end - first);
    first = end;
    if (edge.size() == 2)
    {
      const float cosine =
        DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&edge[0].normal), DirectX::XMLoadFloat3(&edge[1].normal)));
      if (cosine >= flatCosine)
        continue;
    }
    DirectX::XMVECTOR sum = DirectX::XMVectorZero();
    for (const Side& side : edge)
      sum = DirectX::XMVectorAdd(sum, DirectX::XMLoadFloat3(&side.normal));
    // Faces that fold right back on each other have no mean; the first face's normal stands in.
    const DirectX::XMVECTOR mean = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(sum)) > 1e-6f
                                     ? DirectX::XMVector3Normalize(sum)
                                     : DirectX::XMLoadFloat3(&edge.front().normal);
    MeshVertex vertex{};
    DirectX::XMStoreFloat3(&vertex.normal, mean);
    for (const std::uint32_t corner : {edge.front().from, edge.front().to})
    {
      vertex.position = corners[corner];
      lines.indices.push_back(static_cast<std::uint32_t>(lines.vertices.size()));
      lines.vertices.push_back(vertex);
    }
  }
  return lines;
}

std::optional<float> Neuron::SurfaceHeightAt(const MeshData& _mesh, float _x, float _z)
{
  // A point on a triangle's edge counts as on it, so that a line down a shared edge still meets the surface.
  constexpr float ON_EDGE = -1e-5f;
  std::optional<float> highest;
  for (size_t i = 0; i + 2 < _mesh.indices.size(); i += 3)
  {
    const DirectX::XMFLOAT3& a = _mesh.vertices[_mesh.indices[i]].position;
    const DirectX::XMFLOAT3& b = _mesh.vertices[_mesh.indices[i + 1]].position;
    const DirectX::XMFLOAT3& c = _mesh.vertices[_mesh.indices[i + 2]].position;
    // The point's barycentric weights in the triangle seen from above; a triangle seen edge-on from above has none.
    const float area = ((b.z - c.z) * (a.x - c.x)) + ((c.x - b.x) * (a.z - c.z));
    if (std::abs(area) < 1e-12f)
      continue;
    const float weightA = (((b.z - c.z) * (_x - c.x)) + ((c.x - b.x) * (_z - c.z))) / area;
    const float weightB = (((c.z - a.z) * (_x - c.x)) + ((a.x - c.x) * (_z - c.z))) / area;
    const float weightC = 1.0f - weightA - weightB;
    if (weightA < ON_EDGE || weightB < ON_EDGE || weightC < ON_EDGE)
      continue;
    const float height = (weightA * a.y) + (weightB * b.y) + (weightC * c.y);
    if (!highest.has_value() || height > *highest)
      highest = height;
  }
  return highest;
}
