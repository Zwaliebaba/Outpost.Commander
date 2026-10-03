#include "pch.h"
#include "RepositoryAssets.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr float TOLERANCE = 1e-4f;
constexpr DirectX::XMFLOAT3 UP{0.0f, 1.0f, 0.0f};
constexpr DirectX::XMFLOAT3 BACK{-1.0f, 0.0f, 0.0f};
constexpr DirectX::XMFLOAT4 CYAN{0.3f, 0.85f, 1.0f, 1.0f};

// A 20 m ship's hardpoints: a gun at the bow and one at the stern, and an exhaust at the stern pointing back.
std::vector<Neuron::MeshHardpoint> TwoGunsAndAnExhaust()
{
  return {{.tag = "gun", .position = {8.0f, 1.0f, 0.0f}, .forward = UP, .up = BACK, .size = 0.8f},
          {.tag = "gun", .position = {-6.0f, 1.0f, 0.0f}, .forward = UP, .up = BACK, .size = 0.8f},
          {.tag = "exhaust", .position = {-10.0f, 0.0f, 0.0f}, .forward = BACK, .up = UP, .size = 1.0f}};
}

// The muzzle a shot at _target leaves from; the test fails if there is none.
Outpost::PlanePosition Muzzle(const std::vector<Neuron::MeshHardpoint>& _hardpoints, const Outpost::ModelPose& _pose,
                              Outpost::PlanePosition _target)
{
  const std::optional<Outpost::PlanePosition> muzzle = Outpost::NearestMuzzle(_hardpoints, _pose, _target);
  // Assert::Fail does not return, so the value is there below it.
  if (!muzzle.has_value())
    Assert::Fail(L"The model has no gun.");
  return *muzzle;
}

// The farthest any glow is from the ship's center: how far the exhaust streams out.
float Reach(const std::vector<Neuron::GlowPipeline::Glow>& _glows, Outpost::PlanePosition _ship)
{
  float farthest = 0.0f;
  for (const Neuron::GlowPipeline::Glow& glow : _glows)
    farthest = std::max(farthest, std::hypot(glow.position.x - _ship.xMeters, glow.position.z - _ship.zMeters));
  return farthest;
}
} // namespace

TEST_CLASS(HardpointsTests)
{
public:
  TEST_METHOD(KnowsTheGamesTags)
  {
    Assert::IsTrue(Outpost::HardpointKindOf("gun") == Outpost::HardpointKind::Gun);
    Assert::IsTrue(Outpost::HardpointKindOf("exhaust") == Outpost::HardpointKind::Exhaust);
    Assert::IsFalse(Outpost::HardpointKindOf("dock").has_value());
    Assert::IsFalse(Outpost::HardpointKindOf("Gun").has_value());
  }

  // A hardpoint must land where GameClient draws the mesh's point: scaled, turned counterclockwise seen from above by
  // XMMatrixRotationY of minus the heading, and moved.
  TEST_METHOD(PlacesAPointWhereTheWorldMatrixDrawsIt)
  {
    const Outpost::ModelPose pose{
      .position = {.xMeters = 100.0f, .zMeters = -40.0f}, .liftMeters = 3.0f, .headingRadians = 0.7f, .scale = 2.5f};
    const DirectX::XMFLOAT3 point{4.0f, 1.5f, -2.0f};
    const DirectX::XMMATRIX world = DirectX::XMMatrixScaling(pose.scale, pose.scale, pose.scale) *
                                    DirectX::XMMatrixRotationY(-pose.headingRadians) *
                                    DirectX::XMMatrixTranslation(pose.position.xMeters, pose.liftMeters, pose.position.zMeters);
    DirectX::XMFLOAT3 expected;
    DirectX::XMStoreFloat3(&expected, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), world));
    const DirectX::XMFLOAT3 placed = Outpost::PlacePoint(pose, point);
    Assert::AreEqual(expected.x, placed.x, TOLERANCE);
    Assert::AreEqual(expected.y, placed.y, TOLERANCE);
    Assert::AreEqual(expected.z, placed.z, TOLERANCE);

    // A quarter turn takes the front, +x, to +z.
    const DirectX::XMFLOAT3 front = Outpost::PlaceDirection({.headingRadians = std::numbers::pi_v<float> / 2.0f}, {1.0f, 0.0f, 0.0f});
    Assert::AreEqual(1.0f, front.z, TOLERANCE);
  }

  // ADR-029: a banked hardpoint lands where PoseMatrix, which draws the mesh, puts the point, and a positive bank lowers
  // the left side, +z, which is into a counterclockwise turn. The exhaust and the guns roll with the hull.
  TEST_METHOD(RollsAPointWithTheBank)
  {
    const Outpost::ModelPose pose{
      .position = {.xMeters = -30.0f, .zMeters = 12.0f}, .liftMeters = 2.0f, .headingRadians = 1.1f, .bankRadians = 0.4f, .scale = 1.5f};
    const DirectX::XMFLOAT3 point{4.0f, 1.5f, -2.0f};
    const DirectX::XMFLOAT4X4 matrix = Outpost::PoseMatrix(pose);
    DirectX::XMFLOAT3 expected;
    DirectX::XMStoreFloat3(&expected, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), DirectX::XMLoadFloat4x4(&matrix)));
    const DirectX::XMFLOAT3 placed = Outpost::PlacePoint(pose, point);
    Assert::AreEqual(expected.x, placed.x, TOLERANCE);
    Assert::AreEqual(expected.y, placed.y, TOLERANCE);
    Assert::AreEqual(expected.z, placed.z, TOLERANCE);

    const DirectX::XMFLOAT3 left = Outpost::PlaceDirection({.bankRadians = 0.4f}, {0.0f, 0.0f, 1.0f});
    Assert::AreEqual(-std::sin(0.4f), left.y, TOLERANCE);
    const DirectX::XMFLOAT3 front = Outpost::PlaceDirection({.bankRadians = 0.4f}, {1.0f, 0.0f, 0.0f});
    Assert::AreEqual(1.0f, front.x, TOLERANCE, L"the bank rolls about the front, which stays where it was");
  }

  // A shot leaves from the gun nearest what it fires at, and an exhaust is never a gun.
  TEST_METHOD(AShotLeavesFromTheNearestGun)
  {
    const std::vector<Neuron::MeshHardpoint> hardpoints = TwoGunsAndAnExhaust();
    const Outpost::ModelPose pose{.position = {.xMeters = 50.0f, .zMeters = 0.0f}};
    Assert::AreEqual(58.0f, Muzzle(hardpoints, pose, {.xMeters = 300.0f, .zMeters = 0.0f}).xMeters, TOLERANCE);
    Assert::AreEqual(44.0f, Muzzle(hardpoints, pose, {.xMeters = -300.0f, .zMeters = 0.0f}).xMeters, TOLERANCE);

    // Turned to face +z, the bow gun is ahead along +z.
    const Outpost::ModelPose turned{.headingRadians = std::numbers::pi_v<float> / 2.0f};
    Assert::AreEqual(8.0f, Muzzle(hardpoints, turned, {.xMeters = 0.0f, .zMeters = 300.0f}).zMeters, TOLERANCE);

    const std::vector<Neuron::MeshHardpoint> exhaustOnly{hardpoints.back()};
    Assert::IsFalse(Outpost::NearestMuzzle(exhaustOnly, pose, {}).has_value());
  }

  // ADR-019: the exhaust streams out behind the ship, in its drive's color, and longer and brighter at speed.
  TEST_METHOD(AnExhaustStreamsBehindAndGrowsWithSpeed)
  {
    const std::vector<Neuron::MeshHardpoint> hardpoints = TwoGunsAndAnExhaust();
    const Outpost::PlanePosition at{.xMeters = 10.0f, .zMeters = 20.0f};
    const Outpost::ModelPose pose{.position = at, .headingRadians = std::numbers::pi_v<float> / 2.0f};

    std::vector<Neuron::GlowPipeline::Glow> resting;
    Outpost::AddExhaustGlows(hardpoints, pose, CYAN, 0.0f, resting);
    std::vector<Neuron::GlowPipeline::Glow> moving;
    Outpost::AddExhaustGlows(hardpoints, pose, CYAN, 1.0f, moving);

    Assert::IsFalse(resting.empty());
    Assert::AreEqual(resting.size(), moving.size(), L"one exhaust, the same glows at any speed");
    for (const Neuron::GlowPipeline::Glow& glow : moving)
    {
      // Facing +z, the stern is toward -z, and the glows sit there, in the drive's hue.
      Assert::IsTrue(glow.position.z < at.zMeters - 9.0f);
      Assert::AreEqual(CYAN.y / CYAN.z, glow.color.y / glow.color.z, TOLERANCE);
    }
    Assert::IsTrue(Reach(moving, at) > Reach(resting, at), L"longer at speed");
    Assert::IsTrue(moving.front().color.z > resting.front().color.z, L"brighter at speed");

    // A ship scaled up, as a structure is, has a bigger exhaust.
    std::vector<Neuron::GlowPipeline::Glow> doubled;
    Outpost::AddExhaustGlows(hardpoints, {.position = at, .headingRadians = pose.headingRadians, .scale = 2.0f}, CYAN, 0.0f, doubled);
    Assert::AreEqual(2.0f * resting.front().radiusMeters, doubled.front().radiusMeters, TOLERANCE);
  }

  // Every model that fires has a gun, and every model that moves has an exhaust, in both players' sets (ADR-018): the
  // hulls, the Constructor, and the two armed structures of design §6.
  TEST_METHOD(EveryModelHasTheHardpointsItsRoleNeeds)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    const auto count = [](const Neuron::MeshData& _mesh, Outpost::HardpointKind _kind)
    {
      return std::ranges::count_if(_mesh.hardpoints, [_kind](const Neuron::MeshHardpoint& _hardpoint)
                                   { return Outpost::HardpointKindOf(_hardpoint.tag) == _kind; });
    };
    for (const Outpost::PlayerModels& player : catalog.players)
    {
      const Outpost::ModelSet& set = catalog.Set(player.set);
      for (const Outpost::HullModel& hull : catalog.hulls)
      {
        const Neuron::MeshData mesh = ReadRepositoryModel(set, set.Model(hull.model));
        Assert::IsTrue(count(mesh, Outpost::HardpointKind::Gun) > 0 && count(mesh, Outpost::HardpointKind::Exhaust) > 0);
      }
      Assert::IsTrue(count(ReadRepositoryModel(set, set.Model(catalog.constructor)), Outpost::HardpointKind::Exhaust) > 0);
      for (const Outpost::StructureKind armed : {Outpost::StructureKind::CommandStation, Outpost::StructureKind::DefensePlatform})
      {
        const Neuron::MeshData mesh = ReadRepositoryModel(set, set.Model(catalog.ModelForStructure(armed)->model));
        Assert::IsTrue(count(mesh, Outpost::HardpointKind::Gun) > 0);
      }
    }
  }

  // A tag the game does not know stops the model loading, naming the file, rather than being ignored.
  TEST_METHOD(RefusesAModelWithATagTheGameDoesNotKnow)
  {
    Neuron::ByteBuffer bytes = ReadRepositoryAsset("Models\\Human\\Small.nmf");
    // The hardpoints are the file's last section, so the last match is a tag rather than bytes of a vertex.
    const std::array<std::uint8_t, 4> gun{3, 'g', 'u', 'n'};
    const auto found = std::ranges::find_end(bytes, gun);
    Assert::IsFalse(found.empty());
    found[3] = 'm';
    const Outpost::ModelEntry model{.name = "Small", .lengthMeters = 20.0f};
    Assert::ExpectException<Neuron::Exception>([&] { (void)Outpost::BuildModelMesh(bytes, model, "Small.nmf"); });
  }
};
} // namespace GameAppTests
