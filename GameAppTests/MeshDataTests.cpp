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

// A mesh the tests can reason about: one triangle, its front toward +y, stretched 10 m along -z.
Neuron::MeshData Triangle()
{
  Neuron::MeshData mesh;
  const DirectX::XMFLOAT3 up{0.0f, 1.0f, 0.0f};
  mesh.vertices = {{{0.0f, 0.0f, -10.0f}, up}, {{1.0f, 0.0f, 0.0f}, up}, {{-1.0f, 0.0f, 0.0f}, up}};
  mesh.indices = {0, 1, 2};
  mesh.boundsMin = {-1.0f, 0.0f, -10.0f};
  mesh.boundsMax = {1.0f, 0.0f, 0.0f};
  return mesh;
}

// The y of the triangle's geometric normal, cross(v1 - v0, v2 - v0): its sign says which way it winds seen from above.
float WindingUp(const Neuron::MeshData& _mesh)
{
  const DirectX::XMFLOAT3& a = _mesh.vertices[_mesh.indices[0]].position;
  const DirectX::XMFLOAT3& b = _mesh.vertices[_mesh.indices[1]].position;
  const DirectX::XMFLOAT3& c = _mesh.vertices[_mesh.indices[2]].position;
  return ((c.x - a.x) * (b.z - a.z)) - ((b.x - a.x) * (c.z - a.z));
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

  TEST_METHOD(RejectsAFileCutShort)
  {
    Neuron::ByteBuffer bytes = ReadRepositoryAsset("Models\\Human\\Small.cmo");
    bytes.resize(bytes.size() - 1);
    Assert::ExpectException<Neuron::Exception>([&] { (void)Neuron::ParseCmo(bytes, "Small.cmo"); });
  }

  TEST_METHOD(RejectsBytesAfterTheLastMesh)
  {
    Neuron::ByteBuffer bytes = ReadRepositoryAsset("Models\\Human\\Small.cmo");
    bytes.push_back(0);
    Assert::ExpectException<Neuron::Exception>([&] { (void)Neuron::ParseCmo(bytes, "Small.cmo"); });
  }

  TEST_METHOD(RejectsAnEmptyFile)
  {
    Assert::ExpectException<Neuron::Exception>([] { (void)Neuron::ParseCmo({}, "Empty.cmo"); });
  }

  TEST_METHOD(RejectsAMeshCountTooLargeForTheFile)
  {
    Neuron::ByteBuffer bytes = ReadRepositoryAsset("Models\\Human\\Small.cmo");
    const std::uint32_t meshCount = 1000;
    std::memcpy(bytes.data(), &meshCount, sizeof(meshCount));
    Assert::ExpectException<Neuron::Exception>([&] { (void)Neuron::ParseCmo(bytes, "Small.cmo"); });
  }

  TEST_METHOD(TurnsTheFrontToPlusXAndScalesToTheLength)
  {
    Neuron::MeshData mesh = Triangle();
    Neuron::OrientMesh(mesh, Neuron::MeshAxis::NegativeZ, 20.0f);

    // The tip was at -z, the front; it is now the far end along +x.
    Assert::AreEqual(10.0f, mesh.vertices[0].position.x, TOLERANCE);
    Assert::AreEqual(20.0f, mesh.Extents().x, TOLERANCE);
    // Centered: the bounds are symmetric about the origin.
    Assert::AreEqual(0.0f, mesh.boundsMin.x + mesh.boundsMax.x, TOLERANCE);
    Assert::AreEqual(0.0f, mesh.boundsMin.z + mesh.boundsMax.z, TOLERANCE);
    // The normal still points up, and keeps its length.
    Assert::AreEqual(1.0f, mesh.vertices[0].normal.y, TOLERANCE);
  }

  TEST_METHOD(KeepsTheWindingOnEveryAxis)
  {
    const float before = WindingUp(Triangle());
    for (const Neuron::MeshAxis axis :
         {Neuron::MeshAxis::PositiveX, Neuron::MeshAxis::NegativeX, Neuron::MeshAxis::PositiveZ, Neuron::MeshAxis::NegativeZ})
    {
      Neuron::MeshData mesh = Triangle();
      Neuron::OrientMesh(mesh, axis, 20.0f);
      // A turn keeps the triangle's winding; a mirror would flip it, and the pipeline would cull its front.
      Assert::IsTrue(std::signbit(before) == std::signbit(WindingUp(mesh)));
    }
  }
};
} // namespace GameAppTests
