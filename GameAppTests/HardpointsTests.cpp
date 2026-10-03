#include "pch.h"
#include "RepositoryAssets.h"

#include <algorithm>
#include <array>
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

// A table's four feet, and a tripod's three, in meters from the model's origin.
constexpr std::array<DirectX::XMFLOAT3, 4> TABLE{
  {{10.0f, -2.0f, 8.0f}, {10.0f, -2.0f, -8.0f}, {-10.0f, -2.0f, 8.0f}, {-10.0f, -2.0f, -8.0f}}};
constexpr std::array<DirectX::XMFLOAT3, 3> TRIPOD{{{18.0f, -11.0f, 18.0f}, {18.0f, -11.0f, -18.0f}, {-20.0f, -11.0f, 0.0f}}};

// Uneven ground: a slope, a dome and ripples, much as a rock's top is.
std::optional<float> Lumpy(float _x, float _z)
{
  return 20.0f + (0.15f * _x) - (0.1f * _z) - (0.004f * ((_x * _x) + (_z * _z))) + std::sin(0.2f * _x);
}

// How far each foot stands over the ground once stood: negative when it is in it.
template <std::size_t N>
std::array<float, N> Clearances(const std::array<DirectX::XMFLOAT3, N>& _feet, const Outpost::Stance& _stance,
                                const std::function<std::optional<float>(float, float)>& _groundAt)
{
  std::array<float, N> clearances{};
  for (std::size_t i = 0; i < N; ++i)
  {
    const DirectX::XMFLOAT3 stood = Outpost::StandPoint(_stance, _feet[i]);
    clearances[i] = stood.y - _groundAt(stood.x, stood.z).value_or(stood.y);
  }
  return clearances;
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

  // ADR-044: a stood point lands where StanceMatrix, which draws the mesh, puts it: tilted about the pivot and lifted along
  // the model's own axes, then placed by the pose. A positive pitch raises the front and a positive bank lowers +z.
  TEST_METHOD(StandsAPointWhereTheStanceMatrixDrawsIt)
  {
    const Outpost::ModelPose pose{
      .position = {.xMeters = 40.0f, .zMeters = -15.0f}, .liftMeters = 1.0f, .headingRadians = 0.9f, .scale = 12.5f};
    const Outpost::Stance stance{.pivotMeters = {1.0f, -15.0f, 2.0f}, .bankRadians = 0.2f, .pitchRadians = -0.3f, .liftMeters = 30.0f};
    const DirectX::XMFLOAT3 point{1.6f, -1.2f, 1.9f};
    const DirectX::XMFLOAT4X4 matrix = Outpost::StanceMatrix(pose, stance);
    DirectX::XMFLOAT3 expected;
    DirectX::XMStoreFloat3(&expected, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), DirectX::XMLoadFloat4x4(&matrix)));
    Outpost::ModelPose unscaled = pose;
    unscaled.scale = 1.0f;
    const DirectX::XMFLOAT3 placed =
      Outpost::PlacePoint(unscaled, Outpost::StandPoint(stance, {point.x * pose.scale, point.y * pose.scale, point.z * pose.scale}));
    Assert::AreEqual(expected.x, placed.x, 1e-3f);
    Assert::AreEqual(expected.y, placed.y, 1e-3f);
    Assert::AreEqual(expected.z, placed.z, 1e-3f);

    Assert::IsTrue(Outpost::StandPoint({.pitchRadians = 0.3f}, {1.0f, 0.0f, 0.0f}).y > 0.0f);
    Assert::IsTrue(Outpost::StandPoint({.bankRadians = 0.3f}, {0.0f, 0.0f, 1.0f}).y < 0.0f);
  }

  // ADR-044: on level ground a model on legs stands level on every foot, and on a slope it tilts with the slope, its front
  // raised where the ground rises ahead and its +z side where it rises that way, and stands on every foot.
  TEST_METHOD(StandsOnLevelGroundAndTiltsWithASlope)
  {
    const auto level = [](float, float) { return std::optional<float>(5.0f); };
    const Outpost::Stance flat = Outpost::StandOnFeet(TABLE, level).value_or(Outpost::Stance{.liftMeters = -1.0f});
    Assert::AreEqual(0.0f, flat.pitchRadians, TOLERANCE);
    Assert::AreEqual(0.0f, flat.bankRadians, TOLERANCE);
    for (const float clearance : Clearances(TABLE, flat, level))
      Assert::AreEqual(0.0f, clearance, TOLERANCE);

    const auto slope = [](float _x, float _z) { return std::optional<float>(3.0f + (0.2f * _x) + (0.1f * _z)); };
    const Outpost::Stance tilted = Outpost::StandOnFeet(TABLE, slope).value_or(Outpost::Stance{});
    Assert::AreEqual(std::atan(0.2f), tilted.pitchRadians, 0.01f);
    Assert::IsTrue(tilted.bankRadians < -0.05f);
    for (const float clearance : Clearances(TABLE, tilted, slope))
      Assert::AreEqual(0.0f, clearance, 0.01f);
  }

  // ADR-044: on uneven ground a model on three legs stands on all three. On four it rests on some, none is in the ground,
  // and the others hang no more than the ground's unevenness makes them.
  TEST_METHOD(StandsItsLegsOnUnevenGround)
  {
    const Outpost::Stance tripod = Outpost::StandOnFeet(TRIPOD, Lumpy).value_or(Outpost::Stance{});
    for (const float clearance : Clearances(TRIPOD, tripod, Lumpy))
      Assert::AreEqual(0.0f, clearance, 0.01f);

    constexpr std::array<DirectX::XMFLOAT3, 4> WIDE_TABLE{
      {{20.0f, -16.0f, 24.0f}, {20.0f, -16.0f, -24.0f}, {-20.0f, -16.0f, 24.0f}, {-20.0f, -16.0f, -24.0f}}};
    const Outpost::Stance table = Outpost::StandOnFeet(WIDE_TABLE, Lumpy).value_or(Outpost::Stance{});
    const std::array<float, 4> clearances = Clearances(WIDE_TABLE, table, Lumpy);
    Assert::IsTrue(std::ranges::all_of(clearances, [](float _clearance) { return _clearance > -TOLERANCE; }), L"none in the ground");
    Assert::AreEqual(0.0f, std::ranges::min(clearances), TOLERANCE, L"on the ground");
    Assert::IsTrue(std::ranges::max(clearances) < 0.5f, L"the others hardly hang");
  }

  // ADR-044: a model stands only where there is ground under a foot, and feet in a line lift to the ground without
  // tilting, since no plane fits them.
  TEST_METHOD(StandsOnlyOverGround)
  {
    Assert::IsFalse(Outpost::StandOnFeet(TABLE, [](float, float) { return std::optional<float>(); }).has_value());

    constexpr std::array<DirectX::XMFLOAT3, 3> LINE{{{-10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}}};
    const Outpost::Stance lifted =
      Outpost::StandOnFeet(LINE, [](float _x, float) { return std::optional<float>(0.5f * _x); }).value_or(Outpost::Stance{});
    Assert::AreEqual(0.0f, lifted.pitchRadians, TOLERANCE);
    Assert::AreEqual(0.0f, lifted.bankRadians, TOLERANCE);
    Assert::AreEqual(5.0f, lifted.liftMeters, TOLERANCE, L"the foot over the highest ground stands on it");
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
