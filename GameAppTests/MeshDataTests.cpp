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

namespace GameAppTests
{
namespace
{
constexpr float TOLERANCE = 1e-3f;
// Where an .nmf file's vertex count is, after its magic and its version.
constexpr size_t VERTEX_COUNT_OFFSET = 8;
constexpr size_t VERSION_OFFSET = 4;

// The bytes of an .nmf file, written as Tools/BakeMeshes.py writes them (ADR-018).
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
  for (const std::uint32_t value : {1u, 3u, 3u, 1u, 0u})
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
  TEST_METHOD(ReadsEveryModelTheGameShips)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    size_t models = 0;
    for (const Outpost::ModelSet& set : catalog.sets)
    {
      for (const Outpost::ModelEntry& model : set.models)
      {
        const Neuron::MeshData mesh = ReadRepositoryModel(set, model);
        Assert::IsFalse(mesh.indices.empty());
        Assert::AreEqual(size_t{0}, mesh.indices.size() % 3);
        for (const std::uint32_t index : mesh.indices)
          Assert::IsTrue(index < mesh.vertices.size());
        ++models;
      }
    }
    // Nine models in each ship set, three hulls, the Constructor and the five structures, and the three rocks (design §11).
    Assert::AreEqual(size_t{21}, models);
  }

  // The baker turns the source's right-handed triangles into the game's left-handed ones (ADR-018): seen from its front,
  // where its normals point, a triangle winds clockwise, so the pipeline does not cull it. A triangle or two that a
  // modeler wound against its own normals is allowed; a model turned inside out is not.
  TEST_METHOD(EveryShippedModelWindsClockwiseFromItsFront)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    for (const Outpost::ModelSet& set : catalog.sets)
    {
      for (const Outpost::ModelEntry& model : set.models)
      {
        const Neuron::MeshData mesh = ReadRepositoryModel(set, model);
        size_t facingNormals = 0;
        for (size_t i = 0; i < mesh.indices.size(); i += 3)
        {
          const Neuron::MeshVertex& a = mesh.vertices[mesh.indices[i]];
          const Neuron::MeshVertex& b = mesh.vertices[mesh.indices[i + 1]];
          const Neuron::MeshVertex& c = mesh.vertices[mesh.indices[i + 2]];
          const DirectX::XMVECTOR face =
            DirectX::XMVector3Cross(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&b.position), DirectX::XMLoadFloat3(&a.position)),
                                    DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&c.position), DirectX::XMLoadFloat3(&a.position)));
          const DirectX::XMVECTOR normals = DirectX::XMVectorAdd(
            DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&a.normal), DirectX::XMLoadFloat3(&b.normal)), DirectX::XMLoadFloat3(&c.normal));
          if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(face, normals)) > 0.0f)
            ++facingNormals;
        }
        const size_t triangles = mesh.indices.size() / 3;
        // The loader allows only ASCII letters and digits in names, so widening them character by character is exact.
        const std::wstring name =
          std::wstring(set.name.begin(), set.name.end()) + L"/" + std::wstring(model.name.begin(), model.name.end());
        Assert::IsTrue(facingNormals * 100 >= triangles * 99, name.c_str());
      }
    }
  }

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
    const std::uint32_t version = 2;
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

  // The owner's low-poly rocks (2026-10-02, ADR-027): each is closed, has few and so big
  // facets, shows most of its edges as ridges at GameClient's CREASE_DEGREES of 10, and fits inside its radius,
  // which is half its length once fitted, since the game blocks that circle.
  TEST_METHOD(TheRocksAreLowPolyAndShowTheirRidges)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    const Outpost::ModelSet& set = catalog.Set("Asteroids");
    Assert::AreEqual(size_t{3}, set.models.size());
    for (const Outpost::ModelEntry& model : set.models)
    {
      const Neuron::MeshData rock = ReadRepositoryModel(set, model);
      const std::wstring name(model.name.begin(), model.name.end());
      const size_t triangles = rock.indices.size() / 3;
      Assert::IsTrue(triangles <= 100, name.c_str());
      // Nothing folds by 179 degrees, so the only lines left are open edges, and a closed rock has none.
      Assert::AreEqual(size_t{0}, LineCount(Neuron::BuildCreaseLines(rock, Degrees(179.0f))), name.c_str());
      // A closed mesh has one and a half edges per triangle; at least half of them are ridges.
      const size_t creases = LineCount(Neuron::BuildCreaseLines(rock, Degrees(10.0f)));
      Assert::IsTrue(creases * 4 >= triangles * 3, name.c_str());

      const float half = rock.Extents().x / 2.0f;
      const DirectX::XMFLOAT3 center{(rock.boundsMin.x + rock.boundsMax.x) / 2.0f, (rock.boundsMin.y + rock.boundsMax.y) / 2.0f,
                                     (rock.boundsMin.z + rock.boundsMax.z) / 2.0f};
      for (const Neuron::MeshVertex& vertex : rock.vertices)
      {
        const float dx = vertex.position.x - center.x;
        const float dy = vertex.position.y - center.y;
        const float dz = vertex.position.z - center.z;
        Assert::IsTrue(std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) <= half * 1.01f, name.c_str());
      }
    }
  }
};
} // namespace GameAppTests