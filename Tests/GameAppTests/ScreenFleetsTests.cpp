#include "pch.h"
#include "RepositoryAssets.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId PLAYER{1};
constexpr Outpost::PlayerId ENEMY{2};
// A plate's size on a 1080p screen, as the HUD lays it out with a count over its disc.
constexpr DirectX::XMFLOAT2 PLATE{28.0f, 46.0f};
constexpr float SMALL_RADIUS_METERS = 8.0f;

Outpost::ScreenFleets::Ship At(std::uint32_t _id, Outpost::PlayerId _owner, DirectX::XMFLOAT2 _footPixels,
                               Outpost::PlanePosition _position = {})
{
  return {
    .id = Outpost::EntityId{_id}, .owner = _owner, .position = _position, .radiusMeters = SMALL_RADIUS_METERS, .footPixels = _footPixels};
}

// The ships of _ids standing at _positions, placed on a 1920×1080 screen by _camera.
std::vector<Outpost::ScreenFleets::Ship> Seen(const Outpost::Camera& _camera, std::span<const Outpost::PlanePosition> _positions)
{
  const Outpost::Viewport viewport{.widthPixels = 1920, .heightPixels = 1080};
  std::vector<Outpost::ScreenFleets::Ship> ships;
  ships.reserve(_positions.size());
  for (std::size_t i = 0; i < _positions.size(); ++i)
  {
    const DirectX::XMFLOAT2 foot = _camera.PixelOf(_positions[i], viewport).value_or(DirectX::XMFLOAT2{});
    ships.push_back(At(static_cast<std::uint32_t>(i + 1), PLAYER, foot, _positions[i]));
  }
  return ships;
}
} // namespace

TEST_CLASS(ScreenFleetsTests)
{
public:
  // ADR-101 decision 1: two groups of a side's ships, each in formation, 40 m apart: two fleets at the default view, where
  // their plates stand far apart, and one once the camera pulls back so far that their plates would meet.
  TEST_METHOD(FindsTheFleetsByNearnessOnTheScreen)
  {
    std::vector<Outpost::PlanePosition> positions;
    positions.reserve(8);
    for (int ship = 0; ship < 4; ++ship)
      positions.push_back({.xMeters = 20.0f * static_cast<float>(ship), .zMeters = 0.0f});
    for (int ship = 0; ship < 4; ++ship)
      positions.push_back({.xMeters = 100.0f + (20.0f * static_cast<float>(ship)), .zMeters = 0.0f});
    Outpost::Camera camera(Outpost::LoadCameraSettings(ReadRepositoryAssetText("Camera.json")));
    camera.SetFocus(80.0f, 0.0f);

    // Not "near" and "far", which Windows.h defines away.
    Outpost::ScreenFleets closeIn;
    closeIn.Update(Seen(camera, positions), PLATE, 0.0f);
    Assert::AreEqual(std::size_t{2}, closeIn.Fleets().size(), L"apart at the default view");
    Assert::AreEqual(std::size_t{4}, closeIn.Fleets()[0].ships.size(), L"a formation is one fleet");
    Assert::AreEqual(std::size_t{4}, closeIn.Fleets()[1].ships.size());

    camera.Zoom(-20.0f);
    Outpost::ScreenFleets pulledBack;
    pulledBack.Update(Seen(camera, positions), PLATE, 0.0f);
    Assert::AreEqual(std::size_t{1}, pulledBack.Fleets().size(), L"one as the camera pulls back");
    Assert::AreEqual(std::size_t{8}, pulledBack.Fleets()[0].ships.size());
  }

  // ADR-101 decision 1: a formation keeps three of its widest ship's radii between neighbors (ADR-010 decision 4), so two
  // small ships beside a large one are of its fleet however far apart the camera shows them; a side's widest ship sets
  // its own reach, not another side's.
  TEST_METHOD(HoldsAMixedFormationTogether)
  {
    Outpost::ScreenFleets fleets;
    std::array ships{At(1, PLAYER, {0.0f, 500.0f}, {.xMeters = 0.0f, .zMeters = 0.0f}),
                     At(2, PLAYER, {300.0f, 500.0f}, {.xMeters = 72.0f, .zMeters = 0.0f}),
                     At(3, PLAYER, {600.0f, 500.0f}, {.xMeters = 144.0f, .zMeters = 0.0f}),
                     At(4, ENEMY, {900.0f, 500.0f}, {.xMeters = 1000.0f, .zMeters = 0.0f}),
                     At(5, ENEMY, {1200.0f, 500.0f}, {.xMeters = 1050.0f, .zMeters = 0.0f})};
    ships[0].radiusMeters = 24.0f;
    fleets.Update(ships, PLATE, 0.0f);
    Assert::AreEqual(std::size_t{3}, fleets.Fleets().size());
    Assert::AreEqual(std::size_t{3}, fleets.Fleets()[0].ships.size(), L"one fleet with its large ship");
    Assert::AreEqual(std::size_t{1}, fleets.Fleets()[1].ships.size(), L"the enemy's small ships 50 m apart are two");

    ships[0].radiusMeters = SMALL_RADIUS_METERS;
    fleets.Update(ships, PLATE, 0.0f);
    Assert::AreEqual(std::size_t{5}, fleets.Fleets().size(), L"without it, 72 m is too far for small ships");
  }

  // ADR-101 decision 1: a plate stands centered over its fleet's ships, level with the highest of them on the screen.
  TEST_METHOD(StandsAPlateOverItsShips)
  {
    Outpost::ScreenFleets fleets;
    const std::array ships{At(1, PLAYER, {100.0f, 500.0f}), At(2, PLAYER, {120.0f, 480.0f}), At(3, PLAYER, {140.0f, 510.0f})};
    fleets.Update(ships, PLATE, 0.0f);
    Assert::AreEqual(std::size_t{1}, fleets.Fleets().size());
    Assert::AreEqual(120.0f, fleets.Fleets()[0].footPixels.x, 0.001f);
    Assert::AreEqual(480.0f, fleets.Fleets()[0].footPixels.y, 0.001f);
  }

  // ADR-101 decision 1: a fleet that loses a ship keeps its identifier, and its plate starts where it stood and settles over
  // the ships left, so that it does not jump; a fleet that parts keeps its identifier in the larger part.
  TEST_METHOD(KeepsAFleetAsItLosesAShip)
  {
    Outpost::ScreenFleets fleets;
    std::vector ships{At(1, PLAYER, {100.0f, 500.0f}), At(2, PLAYER, {120.0f, 500.0f}), At(3, PLAYER, {140.0f, 500.0f}),
                      At(4, PLAYER, {160.0f, 500.0f})};
    fleets.Update(ships, PLATE, 0.0f);
    Assert::AreEqual(std::size_t{1}, fleets.Fleets().size());
    const std::uint32_t id = fleets.Fleets()[0].id;
    Assert::AreEqual(130.0f, fleets.Fleets()[0].footPixels.x, 0.001f);

    ships.pop_back();
    fleets.Update(ships, PLATE, 0.0f);
    Assert::AreEqual(id, fleets.Fleets()[0].id, L"the same fleet");
    Assert::AreEqual(std::size_t{3}, fleets.Fleets()[0].ships.size());
    Assert::AreEqual(130.0f, fleets.Fleets()[0].footPixels.x, 0.001f, L"where it stood");
    fleets.Update(ships, PLATE, 0.1f);
    const float settling = fleets.Fleets()[0].footPixels.x;
    Assert::IsTrue(settling < 130.0f && settling > 120.0f, L"on its way");
    fleets.Update(ships, PLATE, 2.0f);
    Assert::AreEqual(120.0f, fleets.Fleets()[0].footPixels.x, 0.01f, L"settled over the ships left");

    // Ship 3 flies off: the two left keep the identifier, and it is a fleet of its own.
    ships[2].footPixels = {600.0f, 500.0f};
    ships[2].position = {.xMeters = 1000.0f, .zMeters = 0.0f};
    fleets.Update(ships, PLATE, 0.0f);
    Assert::AreEqual(std::size_t{2}, fleets.Fleets().size());
    const Outpost::ScreenFleets::Fleet* kept = fleets.Find(id);
    Assert::IsNotNull(kept);
    Assert::IsTrue(kept->ships == std::vector{Outpost::EntityId{1}, Outpost::EntityId{2}});
    Assert::IsTrue(fleets.Fleets()[1].id != id && fleets.Fleets()[1].ships == std::vector{Outpost::EntityId{3}});
  }

  // ADR-101 decision 1: ships of two sides are never one fleet, however near they stand.
  TEST_METHOD(NeverMakesTwoSidesOneFleet)
  {
    Outpost::ScreenFleets fleets;
    const std::array ships{At(1, PLAYER, {100.0f, 500.0f}), At(2, ENEMY, {100.0f, 500.0f}), At(3, PLAYER, {110.0f, 505.0f}),
                           At(4, ENEMY, {105.0f, 495.0f})};
    fleets.Update(ships, PLATE, 0.0f);
    Assert::AreEqual(std::size_t{2}, fleets.Fleets().size());
    for (const Outpost::ScreenFleets::Fleet& fleet : fleets.Fleets())
      Assert::AreEqual(std::size_t{2}, fleet.ships.size());
    Assert::IsTrue(fleets.Fleets()[0].owner != fleets.Fleets()[1].owner);
  }

  // ADR-101 decision 1: two fleets of one side whose plates would meet are one, though no ship of one is near a ship of the
  // other: here a formation that the camera shows across 200 pixels, and a ship 40 pixels below its middle.
  TEST_METHOD(JoinsFleetsWhosePlatesWouldMeet)
  {
    Outpost::ScreenFleets fleets;
    const std::array ships{At(1, PLAYER, {0.0f, 500.0f}, {.xMeters = 0.0f, .zMeters = 0.0f}),
                           At(2, PLAYER, {200.0f, 500.0f}, {.xMeters = 20.0f, .zMeters = 0.0f}),
                           At(3, PLAYER, {100.0f, 540.0f}, {.xMeters = 500.0f, .zMeters = 0.0f})};
    fleets.Update(ships, PLATE, 0.0f);
    Assert::AreEqual(std::size_t{1}, fleets.Fleets().size());
    Assert::AreEqual(std::size_t{3}, fleets.Fleets()[0].ships.size());

    fleets.Clear();
    Assert::IsTrue(fleets.Fleets().empty());
    const std::array apart{ships[0], ships[1], At(3, PLAYER, {100.0f, 560.0f}, {.xMeters = 500.0f, .zMeters = 0.0f})};
    fleets.Update(apart, PLATE, 0.0f);
    Assert::AreEqual(std::size_t{2}, fleets.Fleets().size(), L"a plate's height below, they no longer meet");
  }
};
} // namespace GameAppTests
