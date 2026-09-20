#include "pch.h"

#include "GroundRay.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Where a ray meets the ground (Design/Interface.md §4 and §11 row 14; m1-vertical-slice/K7). The
// ring the commander points with is drawn ON the landscape and tilted to it, so everything that
// can be wrong about where it lands is decided here rather than in a shader.
namespace ClientTests
{

namespace
{

constexpr std::int32_t SPACING = 16;
constexpr std::uint32_t SIDE = 9; ///< Eight cells a side, 128 world units across

/// A heightfield and the samples under it, so a test names a shape and gets a view of it.
struct Field
{
  std::vector<std::int16_t> samples;
  Neuron::HeightView view{};

  explicit Field(std::int16_t _height = 0, std::int32_t _water = -1000)
    : samples(static_cast<std::size_t>(SIDE) * SIDE, _height)
  {
    view = {samples.data(), SIDE, SPACING, _water, _height};
  }

  void Set(std::uint32_t _column, std::uint32_t _row, std::int16_t _height)
  {
    samples[static_cast<std::size_t>(_row) * SIDE + _column] = _height;
    view.samples = samples.data();
  }
};

[[nodiscard]] std::array<float, 3> Down()
{
  return {0.0f, -1.0f, 0.0f};
}

void AssertClose(float _expected, float _actual, float _tolerance, const wchar_t* _message)
{
  Assert::IsTrue(std::abs(_expected - _actual) <= _tolerance, _message);
}

} // namespace

TEST_CLASS(GroundRayTests)
{
public:
  TEST_METHOD(AVerticalRayLandsOnTheGroundUnderIt)
  {
    Field field(40);
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {50.0f, 500.0f, 30.0f}, Down());
    Assert::IsTrue(hit.hit);
    AssertClose(50.0f, hit.x, 0.01f, L"straight down, so the point is the origin's");
    AssertClose(30.0f, hit.z, 0.01f, nullptr);
    AssertClose(40.0f, hit.y, 0.01f, L"and the height is the ground's");
    AssertClose(460.0f, hit.distance, 0.01f, L"the distance is in world units along a unit direction");
    Assert::IsFalse(hit.water);
  }

  TEST_METHOD(ADirectionThatIsNotAUnitVectorGivesTheSameAnswer)
  {
    // The caller unprojects a screen point, which gives a direction of whatever length the matrices
    // leave it; a distance that came back scaled by that would size the ring by the projection.
    Field field(40);
    const Neuron::GroundHit unit = Neuron::RayAgainstGround(field.view, {50.0f, 500.0f, 30.0f}, {0.0f, -1.0f, 0.0f});
    const Neuron::GroundHit longer = Neuron::RayAgainstGround(field.view, {50.0f, 500.0f, 30.0f}, {0.0f, -37.5f, 0.0f});
    Assert::IsTrue(unit.hit && longer.hit);
    AssertClose(unit.distance, longer.distance, 0.01f, L"the same distance in world units");
    AssertClose(unit.y, longer.y, 0.01f, nullptr);
  }

  TEST_METHOD(ARayThatNeverEntersTheLandscapeMisses)
  {
    // Off the landscape the cursor is HIDDEN, which is only possible if this says so: a caller
    // that reused its last position would leave a ring on a hill the commander has turned from.
    Field field(40);
    Assert::IsFalse(Neuron::RayAgainstGround(field.view, {-500.0f, 500.0f, 30.0f}, Down()).hit, L"outside, pointing down");
    Assert::IsFalse(Neuron::RayAgainstGround(field.view, {50.0f, 500.0f, 30.0f}, {0.0f, 1.0f, 0.0f}).hit, L"inside, pointing up");
    Assert::IsFalse(Neuron::RayAgainstGround(field.view, {50.0f, 500.0f, -400.0f}, {0.0f, 0.0f, -1.0f}).hit, L"away from it");
    Assert::IsFalse(Neuron::RayAgainstGround(field.view, {50.0f, 500.0f, 30.0f}, {0.0f, 0.0f, 0.0f}).hit, L"and a ray of no length");
  }

  TEST_METHOD(ARayCrossingTheLandscapeAboveItMisses)
  {
    // Level with the sky over flat ground: it passes from edge to edge and never comes down.
    Field field(40);
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {-50.0f, 200.0f, 64.0f}, {1.0f, 0.0f, 0.0f});
    Assert::IsFalse(hit.hit, L"a ray over the landscape hits nothing on it");
  }

  TEST_METHOD(TheHitIsOnTheDiagonalTheTerrainMeshIsBuiltWith)
  {
    // THE CASE THE MARCH EXISTS FOR. A cell is two triangles and there are two ways to split it;
    // on a saddle - two high corners and two low ones - the two splits differ by most of a cell's
    // height down the middle. A cursor tested against the other diagonal floats in the air over
    // the ground the commander is looking at, or sinks into it, by that much.
    //
    // Cell (0,0) is made a saddle: (0,0) and (1,1) high, (1,0) and (0,1) low. The mesh's shared
    // edge runs from (1,0) to (0,1), which are the LOW pair, so the middle of the cell is low.
    Field field(0);
    field.Set(0, 0, 200);
    field.Set(1, 1, 200);
    const float middle = 0.5f * static_cast<float>(SPACING);
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {middle, 500.0f, middle}, Down());
    Assert::IsTrue(hit.hit);
    AssertClose(0.0f, hit.y, 0.5f, L"the drawn diagonal joins the two LOW corners, so the middle is low");
    // The other diagonal would put it at 100: half way between the two high corners.
    Assert::IsTrue(hit.y < 50.0f, L"and not on the other diagonal, which would be a hundred units up");
  }

  TEST_METHOD(ASlantRayTakesTheFIRSTGroundItMeetsAndNotTheFarthest)
  {
    // A wall across the middle of the landscape, and a shallow ray from one side: it must stop at
    // the near face and not march through to the flat beyond.
    Field field(0);
    for (std::uint32_t row = 0; row < SIDE; ++row)
    {
      field.Set(4, row, 300);
    }
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {8.0f, 100.0f, 64.0f}, {1.0f, -0.2f, 0.0f});
    Assert::IsTrue(hit.hit);
    Assert::IsTrue(hit.x < 4.0f * static_cast<float>(SPACING) + 0.5f, L"it stopped at the wall");
    Assert::IsTrue(hit.y > 20.0f, L"on its face, above the flat ground behind it");
  }

  TEST_METHOD(ARayFromOutsideTheLandscapeLandsOnTheCellItReallyEnters)
  {
    // THE ORDINARY CASE, because the camera is usually off the edge looking in: the ray has to be
    // picked up where it CROSSES the boundary and not where it started. A march that began at the
    // origin's cell would start in the corner nearest it, walk in the wrong order, and give either
    // the wrong cell or no cell at all.
    Field field(40);
    const float extent = Neuron::GroundExtent(field.view);
    Assert::AreEqual(128.0f, extent, L"eight cells of sixteen");
    // From well outside in -x and above, down onto the flat at forty: it crosses y = 40 at x = 48.
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {-80.0f, 168.0f, 64.0f}, {1.0f, -1.0f, 0.0f});
    Assert::IsTrue(hit.hit, L"a ray from off the landscape still lands on it");
    AssertClose(48.0f, hit.x, 0.05f, L"where the ground is, three cells in");
    AssertClose(40.0f, hit.y, 0.05f, nullptr);
  }

  TEST_METHOD(ARayStartingUNDERTheGroundFindsNoSeaThatIsNotThere)
  {
    // The degenerate input the water test's last condition is there for. A ray that starts below
    // the surface - inside a hill, or a camera the floor has not yet pushed out of one - crosses
    // no triangle going forward, so nothing is hit; and it crosses the water's PLANE, which
    // reaches under every hill in the landscape. Without asking whether the ground at that
    // crossing is actually below the water, the ring would appear on a sea inside a mountain.
    Field field(300, 0); // A plateau three hundred units up, and a sea at zero
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {64.0f, 150.0f, 64.0f}, Down());
    Assert::IsFalse(hit.hit, L"there is no sea three hundred units inside a hill");
  }

  TEST_METHOD(TheNEARERTriangleOfACellWinsWhenARayCrossesBoth)
  {
    // A ray along a cell's ridge line meets BOTH its triangles: the near one on the way up and the
    // far one on the way down. The commander sees the near face, so that is the one the ring has
    // to land on - and a cell is the one place the march cannot decide it, because both hits are
    // found in the same step.
    //
    // Cell (0,0) is a saddle the other way up from the case below: the shared diagonal joins
    // (1,0) and (0,1), and those are the HIGH pair here, so the cell is a roof ridge along it.
    Field field(0);
    field.Set(1, 0, 200);
    field.Set(0, 1, 200);
    // Along z = 8 the cell's profile is 100 at each edge and 200 over the diagonal, so a level ray
    // at 150 crosses the near face a quarter of the way in and the far face three quarters.
    const float middle = 0.5f * static_cast<float>(SPACING);
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {-40.0f, 150.0f, middle}, {1.0f, 0.0f, 0.0f});
    Assert::IsTrue(hit.hit);
    AssertClose(0.25f * static_cast<float>(SPACING), hit.x, 0.05f, L"the near face of the ridge, not the far one at three quarters");
  }

  TEST_METHOD(FlatGroundGivesExactlyStraightUp)
  {
    // EXACTLY, not nearly. The pass builds its quad from the cross product of this with world up,
    // which is the zero vector here; normalising a zero vector gives zero and the quad collapses
    // to a point, so the pass has to branch - and it can only compare for equality if this is
    // bit for bit (0, 1, 0). It is the fault SpeciesLook.md §7.1 records as live today.
    Field field(40);
    const std::array<float, 3> normal = Neuron::GroundNormal(field.view, 50.0f, 30.0f);
    Assert::AreEqual(0.0f, normal[0], L"no slope in x at all");
    Assert::AreEqual(1.0f, normal[1]);
    Assert::AreEqual(0.0f, normal[2], L"nor in z");
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {50.0f, 500.0f, 30.0f}, Down());
    Assert::AreEqual(0.0f, hit.nx, L"and the hit carries the same one");
    Assert::AreEqual(1.0f, hit.ny);
    Assert::AreEqual(0.0f, hit.nz);
  }

  TEST_METHOD(ASlopeTiltsTheNormalTheWayTheGroundFalls)
  {
    // A ramp rising along +x at one unit of height per unit of distance: the normal leans back
    // along -x by exactly as much as it leans up, which is 45 degrees.
    Field field(0);
    for (std::uint32_t row = 0; row < SIDE; ++row)
    {
      for (std::uint32_t column = 0; column < SIDE; ++column)
      {
        field.Set(column, row, static_cast<std::int16_t>(column * SPACING));
      }
    }
    const std::array<float, 3> normal = Neuron::GroundNormal(field.view, 64.0f, 64.0f);
    AssertClose(-std::sqrt(0.5f), normal[0], 1.0e-5f, L"leaning back down the slope");
    AssertClose(std::sqrt(0.5f), normal[1], 1.0e-5f, L"and up by as much, which is forty-five degrees");
    AssertClose(0.0f, normal[2], 1.0e-5f, L"and not at all across it");
    AssertClose(1.0f, std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]), 1.0e-5f, L"a unit vector");

    // AND THE HIT CARRIES THE GROUND'S NORMAL AND NOT THE TRIANGLE'S. It is what the ring is
    // tilted by, so a hit that answered straight up would lie the ring flat on a hillside - which
    // is the quad-at-the-pointer this task exists to replace.
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {64.0f, 500.0f, 64.0f}, Down());
    Assert::IsTrue(hit.hit);
    AssertClose(normal[0], hit.nx, 1.0e-5f, L"the same normal the field gives at that point");
    AssertClose(normal[1], hit.ny, 1.0e-5f, nullptr);
    AssertClose(normal[2], hit.nz, 1.0e-5f, nullptr);
  }

  TEST_METHOD(AHeightBetweenSamplesIsBilinearBetweenThem)
  {
    Field field(0);
    field.Set(0, 0, 0);
    field.Set(1, 0, 100);
    field.Set(0, 1, 200);
    field.Set(1, 1, 300);
    AssertClose(0.0f, Neuron::GroundHeight(field.view, 0.0f, 0.0f), 0.01f, L"on a sample it is that sample");
    AssertClose(50.0f, Neuron::GroundHeight(field.view, 8.0f, 0.0f), 0.01f, L"half way along the top edge");
    AssertClose(100.0f, Neuron::GroundHeight(field.view, 0.0f, 8.0f), 0.01f, L"half way down the left");
    AssertClose(150.0f, Neuron::GroundHeight(field.view, 8.0f, 8.0f), 0.01f, L"and the middle is the mean of the four");
  }

  TEST_METHOD(TheRingFloatsOnTheSeaRatherThanSinkingToTheSeabed)
  {
    // The terrain mesh carries on down under the water, so a ray pointed at the sea hits the
    // BOTTOM of it - sunk, and at the wrong point as well, because the seabed is further along the
    // ray than the surface the commander is looking at. The water's own plane answers instead.
    Field field(-200, 0); // A seabed two hundred units down, and a sea at zero
    const Neuron::GroundHit straight = Neuron::RayAgainstGround(field.view, {50.0f, 500.0f, 30.0f}, Down());
    Assert::IsTrue(straight.hit);
    Assert::IsTrue(straight.water, L"it landed on the sea");
    AssertClose(Neuron::RING_WATER_CLEARANCE, straight.y, 0.01f, L"just above the surface, not on the seabed");
    Assert::AreEqual(1.0f, straight.ny, L"and the sea is flat whatever is under it");

    // AND AT THE RIGHT PLACE, which "raise the height" alone would not give: a slanted ray crosses
    // the surface well short of where it reaches the seabed.
    const Neuron::GroundHit slanted = Neuron::RayAgainstGround(field.view, {8.0f, 100.0f, 64.0f}, {1.0f, -1.0f, 0.0f});
    Assert::IsTrue(slanted.hit && slanted.water);
    AssertClose(108.0f, slanted.x, 0.01f, L"where it crosses the surface and not where it would reach the bed");
  }

  TEST_METHOD(GroundStandingOutOfTheWaterIsStillGround)
  {
    // An island: the sea is at zero and the middle of the landscape is above it. Pointing at the
    // island gives the island, with its own normal, and not the water plane over it.
    Field field(-200, 0);
    for (std::uint32_t row = 3; row <= 5; ++row)
    {
      for (std::uint32_t column = 3; column <= 5; ++column)
      {
        field.Set(column, row, 120);
      }
    }
    const float middle = 4.0f * static_cast<float>(SPACING);
    const Neuron::GroundHit hit = Neuron::RayAgainstGround(field.view, {middle, 500.0f, middle}, Down());
    Assert::IsTrue(hit.hit);
    Assert::IsFalse(hit.water, L"the island is not the sea");
    AssertClose(120.0f, hit.y, 0.01f, L"and the ring sits on it");
  }

  TEST_METHOD(AViewWithNoSamplesMissesRatherThanReadingThem)
  {
    Neuron::HeightView empty{};
    Assert::IsFalse(Neuron::RayAgainstGround(empty, {0.0f, 100.0f, 0.0f}, Down()).hit);
    Assert::AreEqual(0.0f, Neuron::GroundExtent(empty));
  }
};

} // namespace ClientTests
