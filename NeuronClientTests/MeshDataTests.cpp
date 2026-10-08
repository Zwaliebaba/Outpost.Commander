#include "pch.h"
#include "RepositoryAssets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numbers>
#include <optional>
#include <utility>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
constexpr float TOLERANCE = 1e-3f;
// Where an .nmf file's vertex count is, after its magic and its version.
constexpr size_t VERTEX_COUNT_OFFSET = 8;
constexpr size_t VERSION_OFFSET = 4;

// The bytes of an .nmf file, written as Tools/BakeMeshes.py writes them (ADR-018, ADR-045).
class NmfWriter
{
public:
  template <typename T> void Put(const T& _value)
  {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&_value);
    m_bytes.insert(m_bytes.end(), bytes, bytes + sizeof(T));
  }

  void PutTag(std::string_view _tag)
  {
    Put(static_cast<std::uint8_t>(_tag.size()));
    m_bytes.insert(m_bytes.end(), _tag.begin(), _tag.end());
  }

  void Put3(const DirectX::XMFLOAT3& _value)
  {
    Put(_value.x);
    Put(_value.y);
    Put(_value.z);
  }

  [[nodiscard]] const Neuron::ByteBuffer& Bytes() const noexcept
  {
    return m_bytes;
  }

private:
  Neuron::ByteBuffer m_bytes;
};

constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
constexpr DirectX::XMFLOAT3 BACK{-1.0f, 0.0f, 0.0f};

// One triangle facing up, 10 m long along x from x = -2, with one hardpoint at its back end pointing back, unless the
// caller asks for another.
Neuron::ByteBuffer OneTriangle(std::string_view _tag = "exhaust", const DirectX::XMFLOAT3& _forward = BACK, float _size = 0.5f)
{
  NmfWriter writer;
  for (const char c : {'N', 'M', 'F', '\0'})
    writer.Put(c);
  for (const std::uint32_t value : {2u, 3u, 3u, 1u, 0u, 0u})
    writer.Put(value);
  for (const DirectX::XMFLOAT3& position :
       {DirectX::XMFLOAT3{-2.0f, 0.0f, 0.0f}, DirectX::XMFLOAT3{-2.0f, 0.0f, 1.0f}, DirectX::XMFLOAT3{8.0f, 0.0f, 0.0f}})
  {
    writer.Put3(position);
    writer.Put3(UP);
  }
  for (const std::uint32_t index : {0u, 1u, 2u})
    writer.Put(index);
  writer.PutTag(_tag);
  writer.Put3({-2.0f, 0.0f, 0.0f});
  writer.Put3(_forward);
  writer.Put3(UP);
  writer.Put(_size);
  return writer.Bytes();
}

// Two triangles facing up: one that stands still, from x = -2 to 8, and a part that turns about _axis through (4, 0, 0)
// once in _periodSeconds, its run of indices as the caller gives it.
Neuron::ByteBuffer TriangleAndPart(std::uint32_t _firstIndex = 3, std::uint32_t _indexCount = 3, const DirectX::XMFLOAT3& _axis = UP,
                                   float _periodSeconds = 8.0f)
{
  NmfWriter writer;
  for (const char c : {'N', 'M', 'F', '\0'})
    writer.Put(c);
  for (const std::uint32_t value : {2u, 6u, 6u, 0u, 1u, 0u})
    writer.Put(value);
  for (const DirectX::XMFLOAT3& position :
       {DirectX::XMFLOAT3{-2.0f, 0.0f, 0.0f}, DirectX::XMFLOAT3{-2.0f, 0.0f, 1.0f}, DirectX::XMFLOAT3{8.0f, 0.0f, 0.0f},
        DirectX::XMFLOAT3{4.0f, 0.0f, 0.0f}, DirectX::XMFLOAT3{4.0f, 0.0f, 1.0f}, DirectX::XMFLOAT3{5.0f, 0.0f, 0.0f}})
  {
    writer.Put3(position);
    writer.Put3(UP);
  }
  for (const std::uint32_t index : {0u, 1u, 2u, 3u, 4u, 5u})
    writer.Put(index);
  writer.Put(_firstIndex);
  writer.Put(_indexCount);
  writer.Put3({4.0f, 0.0f, 0.0f});
  writer.Put3(_axis);
  writer.Put(_periodSeconds);
  return writer.Bytes();
}

void ExpectRejected(const Neuron::ByteBuffer& _bytes)
{
  Assert::ExpectException<Neuron::Exception>([&] { (void)Neuron::ParseNmf(_bytes, "Test.nmf"); });
}

// A box 2 m each way round the origin, flat-shaded as the baker writes one: each face its own four corners and normal,
// split into two triangles along a diagonal.
Neuron::MeshData FlatBox()
{
  Neuron::MeshData box;
  for (int axis = 0; axis < 3; ++axis)
  {
    for (const float sign : {-1.0f, 1.0f})
    {
      std::array<float, 3> normal{};
      normal[static_cast<size_t>(axis)] = sign;
      const auto u = static_cast<size_t>((axis + 1) % 3);
      const auto v = static_cast<size_t>((axis + 2) % 3);
      const auto first = static_cast<std::uint32_t>(box.vertices.size());
      for (const auto& [du, dv] : std::array<std::pair<float, float>, 4>{{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}})
      {
        std::array<float, 3> point = normal;
        point[u] = du;
        point[v] = dv;
        box.vertices.push_back({.position = {point[0], point[1], point[2]}, .normal = {normal[0], normal[1], normal[2]}});
      }
      box.indices.insert(box.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
    }
  }
  box.boundsMin = {-1.0f, -1.0f, -1.0f};
  box.boundsMax = {1.0f, 1.0f, 1.0f};
  return box;
}

// Two triangles 10 m across, facing up and wound as the baker winds them, so that their geometric normals agree with
// their vertex normals. They share the edge along z at x = 0: the first lies flat toward -x, and the second runs out
// toward +x folded down by _foldDegrees.
Neuron::MeshData Hinge(float _foldDegrees)
{
  const float fold = _foldDegrees * std::numbers::pi_v<float> / 180.0f;
  const DirectX::XMFLOAT3 tip{10.0f * std::cos(fold), -10.0f * std::sin(fold), 0.0f};
  Neuron::MeshData hinge;
  hinge.vertices = {{{0.0f, 0.0f, -5.0f}, UP}, {{0.0f, 0.0f, 5.0f}, UP}, {{-10.0f, 0.0f, 0.0f}, UP}, {{0.0f, 0.0f, -5.0f}, UP}, {tip, UP},
                    {{0.0f, 0.0f, 5.0f}, UP}};
  hinge.indices = {0, 2, 1, 3, 5, 4};
  hinge.boundsMin = {-10.0f, tip.y, -5.0f};
  hinge.boundsMax = {std::max(0.0f, tip.x), 0.0f, 5.0f};
  return hinge;
}

size_t LineCount(const Neuron::MeshData& _lines)
{
  return _lines.indices.size() / 2;
}

float Degrees(float _degrees)
{
  return _degrees * std::numbers::pi_v<float> / 180.0f;
}
} // namespace

TEST_CLASS(MeshDataTests)
{
public:
  TEST_METHOD(ReadsAHardpoint)
  {
    const Neuron::MeshData mesh = Neuron::ParseNmf(OneTriangle(), "Test.nmf");
    Assert::AreEqual(size_t{1}, mesh.hardpoints.size());
    const Neuron::MeshHardpoint& exhaust = mesh.hardpoints.front();
    Assert::AreEqual(std::string("exhaust"), exhaust.tag);
    Assert::AreEqual(-2.0f, exhaust.position.x);
    Assert::AreEqual(-1.0f, exhaust.forward.x);
    Assert::AreEqual(1.0f, exhaust.up.y);
    Assert::AreEqual(0.5f, exhaust.size);
    Assert::AreEqual(10.0f, mesh.Extents().x);
  }

  TEST_METHOD(RejectsAFileCutShort)
  {
    Neuron::ByteBuffer bytes = ReadRepositoryAsset("Models\\Human\\Small.nmf");
    bytes.resize(bytes.size() - 1);
    ExpectRejected(bytes);
  }

  TEST_METHOD(RejectsBytesAfterTheLastHardpoint)
  {
    Neuron::ByteBuffer bytes = ReadRepositoryAsset("Models\\Human\\Small.nmf");
    bytes.push_back(0);
    ExpectRejected(bytes);
  }

  TEST_METHOD(RejectsAnEmptyFile)
  {
    ExpectRejected({});
  }

  TEST_METHOD(RejectsAnotherVersion)
  {
    Neuron::ByteBuffer bytes = OneTriangle();
    const std::uint32_t version = 1;
    std::memcpy(bytes.data() + VERSION_OFFSET, &version, sizeof(version));
    ExpectRejected(bytes);
  }

  TEST_METHOD(RejectsAVertexCountTooLargeForTheFile)
  {
    Neuron::ByteBuffer bytes = ReadRepositoryAsset("Models\\Human\\Small.nmf");
    const std::uint32_t vertexCount = 1'000'000;
    std::memcpy(bytes.data() + VERTEX_COUNT_OFFSET, &vertexCount, sizeof(vertexCount));
    ExpectRejected(bytes);
  }

  TEST_METHOD(RejectsABadHardpoint)
  {
    (void)Neuron::ParseNmf(OneTriangle(), "Test.nmf");
    ExpectRejected(OneTriangle("Exhaust"));
    ExpectRejected(OneTriangle("9exhaust"));
    ExpectRejected(OneTriangle(""));
    ExpectRejected(OneTriangle("exhaust", {-2.0f, 0.0f, 0.0f}));
    ExpectRejected(OneTriangle("exhaust", UP));
    ExpectRejected(OneTriangle("exhaust", BACK, 0.0f));
    ExpectRejected(OneTriangle("exhaust", BACK, std::nanf("")));
  }

  // ADR-045: a part is a run of the triangles after the ones that stand still, and turns about its pivot.
  TEST_METHOD(ReadsASpinningPart)
  {
    const Neuron::MeshData mesh = Neuron::ParseNmf(TriangleAndPart(), "Test.nmf");
    Assert::AreEqual(size_t{1}, mesh.parts.size());
    Assert::AreEqual(3u, mesh.FixedIndexCount());
    const Neuron::MeshPart& part = mesh.parts.front();
    Assert::AreEqual(3u, part.firstIndex);
    Assert::AreEqual(3u, part.indexCount);
    Assert::AreEqual(4.0f, part.pivot.x);
    Assert::AreEqual(1.0f, part.axis.y);
    Assert::AreEqual(8.0f, part.periodSeconds);
    Assert::AreEqual(3u, Neuron::ParseNmf(OneTriangle(), "Test.nmf").FixedIndexCount());
  }

  TEST_METHOD(RejectsABadPart)
  {
    ExpectRejected(TriangleAndPart(0, 6));
    ExpectRejected(TriangleAndPart(3, 0));
    ExpectRejected(TriangleAndPart(3, 6));
    ExpectRejected(TriangleAndPart(2, 4));
    ExpectRejected(TriangleAndPart(6, 3));
    ExpectRejected(TriangleAndPart(3, 3, {0.0f, 2.0f, 0.0f}));
    ExpectRejected(TriangleAndPart(3, 3, UP, 0.0f));
    ExpectRejected(TriangleAndPart(3, 3, UP, std::nanf("")));
  }

  // Fitting moves a part's pivot with the mesh, and leaves its axis and period.
  TEST_METHOD(FitsAPartsPivotWithTheMesh)
  {
    Neuron::MeshData mesh = Neuron::ParseNmf(TriangleAndPart(), "Test.nmf");
    Neuron::FitMesh(mesh, 20.0f);
    const Neuron::MeshPart& part = mesh.parts.front();
    // The mesh is 10 m long from x = -2 and 1 m across from z = 0, so its center is (3, 0, 0.5) and it doubles.
    Assert::AreEqual(2.0f, part.pivot.x, TOLERANCE);
    Assert::AreEqual(-1.0f, part.pivot.z, TOLERANCE);
    Assert::AreEqual(1.0f, part.axis.y);
    Assert::AreEqual(8.0f, part.periodSeconds);
  }

  TEST_METHOD(APieceIsItsTrianglesAndTheVerticesTheyUse)
  {
    const Neuron::MeshData mesh = Neuron::ParseNmf(TriangleAndPart(), "Test.nmf");
    const Neuron::MeshData piece = Neuron::MeshPiece(mesh, mesh.parts.front().firstIndex, mesh.parts.front().indexCount);
    Assert::AreEqual(size_t{3}, piece.vertices.size());
    Assert::IsTrue(piece.indices == std::vector<std::uint32_t>{0, 1, 2});
    Assert::AreEqual(4.0f, piece.vertices[0].position.x);
    Assert::AreEqual(4.0f, piece.boundsMin.x);
    Assert::AreEqual(5.0f, piece.boundsMax.x);
    Assert::IsTrue(piece.hardpoints.empty() && piece.parts.empty());
  }

  // A part about +y turns clockwise seen from above, as a heading does: a quarter of its period takes a point 1 m ahead of
  // its pivot to 1 m to its right, at -z, and a whole period brings it back.
  TEST_METHOD(APartTurnsAboutItsPivotOverItsPeriod)
  {
    const Neuron::MeshPart part = Neuron::ParseNmf(TriangleAndPart(), "Test.nmf").parts.front();
    DirectX::XMFLOAT4X4 identity;
    DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
    const auto turned = [&](double _seconds)
    {
      const DirectX::XMFLOAT4X4 world = Neuron::PartWorld(part, _seconds, identity);
      DirectX::XMFLOAT3 point;
      DirectX::XMStoreFloat3(
        &point, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(5.0f, 0.0f, 0.0f, 1.0f), DirectX::XMLoadFloat4x4(&world)));
      return point;
    };
    const DirectX::XMFLOAT3 quarter = turned(2.0);
    Assert::AreEqual(4.0f, quarter.x, TOLERANCE);
    Assert::AreEqual(-1.0f, quarter.z, TOLERANCE);
    const DirectX::XMFLOAT3 later = turned((8.0 * 1000.0) + 2.0);
    Assert::AreEqual(4.0f, later.x, TOLERANCE);
    Assert::AreEqual(-1.0f, later.z, TOLERANCE);
    Assert::AreEqual(5.0f, turned(8.0).x, TOLERANCE);
  }

  TEST_METHOD(FitsTheMeshAndItsHardpointsToTheLength)
  {
    Neuron::MeshData mesh = Neuron::ParseNmf(OneTriangle(), "Test.nmf");
    Neuron::FitMesh(mesh, 20.0f);

    Assert::AreEqual(20.0f, mesh.Extents().x, TOLERANCE);
    // Centered: the bounds are symmetric about the origin.
    Assert::AreEqual(0.0f, mesh.boundsMin.x + mesh.boundsMax.x, TOLERANCE);
    Assert::AreEqual(0.0f, mesh.boundsMin.z + mesh.boundsMax.z, TOLERANCE);
    // The hardpoint was at the back end, and stays there; it is twice as big, as the mesh is twice as long.
    const Neuron::MeshHardpoint& exhaust = mesh.hardpoints.front();
    Assert::AreEqual(-10.0f, exhaust.position.x, TOLERANCE);
    Assert::AreEqual(-1.0f, exhaust.position.z, TOLERANCE);
    Assert::AreEqual(1.0f, exhaust.size, TOLERANCE);
    Assert::AreEqual(-1.0f, exhaust.forward.x, TOLERANCE);
    // The normal still points up, and keeps its length.
    Assert::AreEqual(1.0f, mesh.vertices[0].normal.y, TOLERANCE);
  }

  // The owner's asteroid edges (2026-10-02): a box shows its twelve edges, not the diagonals that split its faces.
  TEST_METHOD(CreaseLinesAreTheEdgesWhereTheSurfaceBends)
  {
    const Neuron::MeshData lines = Neuron::BuildCreaseLines(FlatBox(), Degrees(30.0f));
    Assert::AreEqual(size_t{12}, LineCount(lines));
    Assert::AreEqual(lines.vertices.size(), lines.indices.size());
    for (size_t i = 0; i < lines.indices.size(); ++i)
      Assert::AreEqual(static_cast<std::uint32_t>(i), lines.indices[i]);
    // Every line runs along an edge of the box: two of its coordinates are at a face.
    for (const Neuron::MeshVertex& vertex : lines.vertices)
    {
      const int atFaces = (std::abs(vertex.position.x) == 1.0f ? 1 : 0) + (std::abs(vertex.position.y) == 1.0f ? 1 : 0) +
                          (std::abs(vertex.position.z) == 1.0f ? 1 : 0);
      Assert::IsTrue(atFaces >= 2);
    }
  }

  // A fold shallower than the angle is not drawn; a steeper one is. The open rim is drawn either way.
  TEST_METHOD(TheAngleDecidesWhichFoldsAreDrawn)
  {
    const Neuron::MeshData hinge = Hinge(45.0f);
    Assert::AreEqual(size_t{5}, LineCount(Neuron::BuildCreaseLines(hinge, Degrees(30.0f))));
    Assert::AreEqual(size_t{4}, LineCount(Neuron::BuildCreaseLines(hinge, Degrees(60.0f))));
    Assert::AreEqual(size_t{4}, LineCount(Neuron::BuildCreaseLines(Hinge(0.0f), Degrees(30.0f))), L"a flat sheet shows its rim");
  }

  // A line is lit along the mean of its faces' normals, and lies on the edge: DrawLines, not the mesh, keeps it in front
  // of the surface (ADR-040).
  TEST_METHOD(CreaseLinesAreLitAlongTheMeanNormalAndLieOnTheEdge)
  {
    const Neuron::MeshData lines = Neuron::BuildCreaseLines(Hinge(90.0f), Degrees(30.0f));
    // The fold's line is the one at x = 0 whose normal leans out between up and +x, the faces' two normals.
    const auto fold = std::ranges::find_if(lines.vertices, [](const Neuron::MeshVertex& _vertex)
                                           { return _vertex.normal.x > 0.1f && _vertex.normal.y > 0.1f; });
    Assert::IsTrue(fold != lines.vertices.end());
    const float half = std::sqrt(0.5f);
    Assert::AreEqual(half, fold->normal.x, TOLERANCE);
    Assert::AreEqual(half, fold->normal.y, TOLERANCE);
    Assert::AreEqual(0.0f, fold->position.x, TOLERANCE);
    Assert::AreEqual(0.0f, fold->position.y, TOLERANCE);
  }

  // A line straight down meets the box's top, wherever over it, and misses it beside the box.
  TEST_METHOD(SurfaceHeightIsTheHighestTriangleBelowThePoint)
  {
    const Neuron::MeshData box = FlatBox();
    const std::optional<float> center = Neuron::SurfaceHeightAt(box, 0.0f, 0.0f);
    Assert::IsTrue(center.has_value());
    Assert::AreEqual(1.0f, *center, TOLERANCE);
    const std::optional<float> corner = Neuron::SurfaceHeightAt(box, 0.9f, -0.9f);
    Assert::IsTrue(corner.has_value());
    Assert::AreEqual(1.0f, *corner, TOLERANCE);
    Assert::IsFalse(Neuron::SurfaceHeightAt(box, 1.5f, 0.0f).has_value());
    // On a slope, the height is where the line meets it: the hinge folded down by 45 degrees, 5 m out along it.
    const std::optional<float> slope = Neuron::SurfaceHeightAt(Hinge(45.0f), 5.0f * std::sqrt(0.5f), 0.0f);
    Assert::IsTrue(slope.has_value());
    Assert::AreEqual(-5.0f * std::sqrt(0.5f), *slope, TOLERANCE);
  }
};
} // namespace NeuronClientTests