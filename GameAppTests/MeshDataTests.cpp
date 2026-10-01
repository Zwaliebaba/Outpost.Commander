#include "pch.h"
#include "RepositoryAssets.h"

#include <cmath>
#include <cstring>

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
    // Fourteen models in each ship set, and the asteroid (design §11).
    Assert::AreEqual(size_t{29}, models);
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
};
} // namespace GameAppTests