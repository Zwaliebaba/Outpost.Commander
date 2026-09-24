#include "pch.h"

#include <array>
#include <cmath>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
/// The seed M0 to M2 run (`GameDesign.md` section 3).
constexpr std::uint64_t MATCH_SEED = 20260922;

constexpr std::uint64_t SEEDS_SWEPT = 200;

/// The five variants' bounding radii, from the generated catalog -- the figures the client uses.
[[nodiscard]] std::array<float, Outpost::ASTEROID_VARIANT_COUNT> CatalogRadii()
{
  std::array<float, Outpost::ASTEROID_VARIANT_COUNT> radii{};
  for (std::size_t variant = 0; variant < radii.size(); ++variant)
  {
    const Outpost::MeshEntry* entry = Outpost::FindMesh(Outpost::AsteroidVariantNames()[variant]);
    Assert::IsNotNull(entry);
    radii[variant] = Outpost::BoundingRadiusUnits(*entry);
  }
  return radii;
}

[[nodiscard]] float Units(Neuron::Fixed _value)
{
  return static_cast<float>(_value) / static_cast<float>(Neuron::FIXED_ONE);
}

[[nodiscard]] Outpost::HullVertex Vertex(float _x, float _y, float _z, float _normalX, float _normalY, float _normalZ)
{
  Outpost::HullVertex vertex;
  vertex.x = _x;
  vertex.y = _y;
  vertex.z = _z;
  vertex.normalX = _normalX;
  vertex.normalY = _normalY;
  vertex.normalZ = _normalZ;
  return vertex;
}

/// One triangle facing out along (1, 1, 1): wound so that its cross product agrees with that normal.
[[nodiscard]] Outpost::HullMesh Triangle()
{
  const float n = 1.0f / std::sqrt(3.0f);
  Outpost::HullMesh mesh;
  mesh.vertices = {Vertex(1.0f, 0.0f, 0.0f, n, n, n), Vertex(0.0f, 1.0f, 0.0f, n, n, n), Vertex(0.0f, 0.0f, 1.0f, n, n, n)};
  mesh.indices = {0, 1, 2};
  return mesh;
}

[[nodiscard]] Outpost::Placement RockAt(std::int32_t _xUnits, std::int32_t _yUnits)
{
  return Outpost::Placement{.kind = Outpost::PlacedKind::Asteroid,
                            .field = Outpost::FieldKind::Home,
                            .owner = Outpost::NO_PLAYER,
                            .position = Neuron::Vec2{.x = Neuron::FixedFromWholeUnits(_xUnits), .y = Neuron::FixedFromWholeUnits(_yUnits)}};
}
} // namespace

/// M2.4. **What a rock looks like is the client's and nothing about it is simulated** (R22, ADR-001), so
/// these are the properties a player would see go wrong: two rocks through each other, a rock turned inside
/// out, a field that changes between two runs.
TEST_CLASS(TheRockLooks)
{
public:
  /// The five the handoff delivered, all in the catalog -- a name that drifted would leave a variant
  /// undrawn with nothing but a log line to say so.
  TEST_METHOD(EveryVariantIsInTheCatalog)
  {
    Assert::AreEqual(std::size_t{5}, Outpost::AsteroidVariantNames().size());
    const std::array<float, Outpost::ASTEROID_VARIANT_COUNT> radii = CatalogRadii();
    for (const float radius : radii)
    {
      Assert::IsTrue(radius > 0.0f);
    }
  }

  /// **THE SAME FIELD TWICE.** Not a determinism rule -- nothing here is simulated -- but two players
  /// looking at one map should see the same rocks, and a relaunch should not reshuffle them.
  TEST_METHOD(OneSeedGivesTheSameLooksTwice)
  {
    const std::vector<Outpost::Placement> field = Outpost::GenerateField(MATCH_SEED, 2);
    const std::array<float, Outpost::ASTEROID_VARIANT_COUNT> radii = CatalogRadii();
    const std::vector<Outpost::RockLook> first = Outpost::RockLooks(MATCH_SEED, field, radii);
    const std::vector<Outpost::RockLook> second = Outpost::RockLooks(MATCH_SEED, field, radii);

    Assert::AreEqual(field.size(), first.size());
    for (std::size_t index = 0; index < first.size(); ++index)
    {
      Assert::AreEqual(first[index].variant, second[index].variant);
      Assert::AreEqual(first[index].yaw, second[index].yaw);
      Assert::AreEqual(first[index].pitch, second[index].pitch);
      Assert::AreEqual(first[index].roll, second[index].roll);
      Assert::AreEqual(first[index].scalePercent, second[index].scalePercent);
      Assert::AreEqual(first[index].liftUnits, second[index].liftUnits);
      Assert::AreEqual(first[index].scale, second[index].scale);
    }
  }

  /// **ROCKS DIFFER.** On the match seed at two players every one of the five variants appears -- the one
  /// part of M2.4's "rocks differ from one another" a suite can say; the rest is looked at on the device.
  TEST_METHOD(TheMatchSeedUsesEveryVariant)
  {
    const std::vector<Outpost::Placement> field = Outpost::GenerateField(MATCH_SEED, 2);
    std::array<bool, Outpost::ASTEROID_VARIANT_COUNT> seen{};
    for (const Outpost::RockLook& look : Outpost::RockLooks(MATCH_SEED, field, CatalogRadii()))
    {
      seen[look.variant] = true;
    }
    for (const bool used : seen)
    {
      Assert::IsTrue(used, L"a variant never appears");
    }
  }

  /// **THE HANDOFF'S RANGES** (`design_handoff_meshes/` section 8): uniform scale 0.75 to 1.35 as drawn,
  /// up to 240 either side of the plane, and a drawn scale never larger than what was drawn for it.
  TEST_METHOD(EveryLookIsInsideTheHandoffsRanges)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        const std::vector<Outpost::Placement> field = Outpost::GenerateField(seed, players);
        for (const Outpost::RockLook& look : Outpost::RockLooks(seed, field, CatalogRadii()))
        {
          Assert::IsTrue(look.variant < Outpost::ASTEROID_VARIANT_COUNT);
          Assert::IsTrue((look.scalePercent >= 75) && (look.scalePercent <= 135));
          Assert::IsTrue((look.liftUnits >= -240) && (look.liftUnits <= 240));
          Assert::IsTrue(look.scale <= (static_cast<float>(look.scalePercent) / 100.0f));
          Assert::IsTrue(look.scale > 0.0f);
        }
      }
    }
  }

  /// **NO TWO ROCKS TOUCH.** The generator keeps centers 150 apart and the largest variant at the top of
  /// the range is 225 across, so without the clamp this fails on the first seed. Measured on the plane, with
  /// each rock's sphere at its drawn scale -- the lift only separates them further.
  TEST_METHOD(NoTwoRocksTouch)
  {
    const std::array<float, Outpost::ASTEROID_VARIANT_COUNT> radii = CatalogRadii();
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        const std::vector<Outpost::Placement> field = Outpost::GenerateField(seed, players);
        const std::vector<Outpost::RockLook> looks = Outpost::RockLooks(seed, field, radii);
        for (std::size_t first = 0; first < field.size(); ++first)
        {
          for (std::size_t second = first + 1; second < field.size(); ++second)
          {
            const float dx = Units(field[first].position.x) - Units(field[second].position.x);
            const float dy = Units(field[first].position.y) - Units(field[second].position.y);
            const float reach = (radii[looks[first].variant] * looks[first].scale) + (radii[looks[second].variant] * looks[second].scale);
            Assert::IsTrue(reach <= (std::sqrt((dx * dx) + (dy * dy)) + 0.01f), L"two rocks overlap");
          }
        }
      }
    }
  }

  /// **A CLAMP READS NO DRAW**, so where the rocks are cannot change what any rock looks like apart from
  /// its drawn scale. The same seed over a crowded pair and a spread pair draws the same six numbers.
  TEST_METHOD(ClampingARockDoesNotShiftTheLooksAfterIt)
  {
    const std::array<float, Outpost::ASTEROID_VARIANT_COUNT> radii = CatalogRadii();
    const std::vector<Outpost::Placement> crowded{RockAt(0, 0), RockAt(150, 0), RockAt(5000, 0)};
    const std::vector<Outpost::Placement> spread{RockAt(0, 0), RockAt(3000, 0), RockAt(6000, 0)};
    const std::vector<Outpost::RockLook> a = Outpost::RockLooks(MATCH_SEED, crowded, radii);
    const std::vector<Outpost::RockLook> b = Outpost::RockLooks(MATCH_SEED, spread, radii);
    for (std::size_t index = 0; index < a.size(); ++index)
    {
      Assert::AreEqual(a[index].variant, b[index].variant);
      Assert::AreEqual(a[index].yaw, b[index].yaw);
      Assert::AreEqual(a[index].scalePercent, b[index].scalePercent);
      Assert::AreEqual(a[index].liftUnits, b[index].liftUnits);
    }
  }
};

/// The placement of one vertex, and the bake of one variant's rocks.
TEST_CLASS(TheBakedField)
{
public:
  /// **THE EXACT SPHERE IS THE FARTHEST VERTEX**, and the catalog's box corner is never inside it.
  TEST_METHOD(TheExactRadiusIsTheFarthestVertex)
  {
    const Outpost::HullMesh triangle = Triangle();
    Assert::AreEqual(1.0f, Outpost::BoundingRadiusUnits(triangle), 0.0001f);

    Outpost::HullMesh stretched = triangle;
    stretched.vertices[1].y = -3.0f;
    stretched.vertices[1].z = 4.0f;
    Assert::AreEqual(5.0f, Outpost::BoundingRadiusUnits(stretched), 0.0001f);
    Assert::AreEqual(0.0f, Outpost::BoundingRadiusUnits(Outpost::HullMesh{}));
  }

  /// An unturned, unscaled, unlifted look only moves the vertex to the rock.
  TEST_METHOD(AnIdentityLookOnlyTranslates)
  {
    const Outpost::RockLook look{};
    const Outpost::HullVertex placed = Outpost::PlaceRockVertex(Vertex(3.0f, 4.0f, 5.0f, 0.0f, 0.0f, 1.0f), RockAt(1000, -2000), look);
    Assert::AreEqual(1003.0f, placed.x, 0.001f);
    Assert::AreEqual(-1996.0f, placed.y, 0.001f);
    Assert::AreEqual(5.0f, placed.z, 0.001f);
    Assert::AreEqual(1.0f, placed.normalZ, 0.0001f);
  }

  /// **THE LIFT IS A DRAWN HEIGHT AND THE ORIGIN LANDS ON IT**: a rock's center is at its place on the
  /// plane, raised or lowered, whatever it was turned or scaled by.
  TEST_METHOD(TheCenterLandsOnThePlaceAtItsLift)
  {
    const Outpost::RockLook look{.yaw = 12345, .pitch = 40000, .roll = 777, .scalePercent = 120, .scale = 1.2f, .liftUnits = -180};
    const Outpost::HullVertex placed = Outpost::PlaceRockVertex(Vertex(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f), RockAt(-400, 250), look);
    Assert::AreEqual(-400.0f, placed.x, 0.001f);
    Assert::AreEqual(250.0f, placed.y, 0.001f);
    Assert::AreEqual(-180.0f, placed.z, 0.001f);
  }

  /// **A TURN KEEPS A NORMAL UNIT AND A TRIANGLE FACING OUT.** The mesh's winding is the handoff's and
  /// nothing on the way to the screen may change it; a placement that reversed it would draw rocks inside out.
  TEST_METHOD(PlacementKeepsNormalsUnitAndWindingOutward)
  {
    const Outpost::HullMesh triangle = Triangle();
    for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
    {
      const std::vector<Outpost::RockLook> looks = Outpost::RockLooks(seed, std::vector<Outpost::Placement>{RockAt(0, 0)}, CatalogRadii());
      const Outpost::RockLook& look = looks.front();

      std::array<Outpost::HullVertex, 3> placed{};
      for (std::size_t corner = 0; corner < 3; ++corner)
      {
        placed[corner] = Outpost::PlaceRockVertex(triangle.vertices[corner], RockAt(0, 0), look);
      }

      const float normalLength = std::sqrt((placed[0].normalX * placed[0].normalX) + (placed[0].normalY * placed[0].normalY) +
                                           (placed[0].normalZ * placed[0].normalZ));
      Assert::AreEqual(1.0f, normalLength, 0.0001f, L"a normal was scaled");

      const float ax = placed[1].x - placed[0].x;
      const float ay = placed[1].y - placed[0].y;
      const float az = placed[1].z - placed[0].z;
      const float bx = placed[2].x - placed[0].x;
      const float by = placed[2].y - placed[0].y;
      const float bz = placed[2].z - placed[0].z;
      const float facing = (((ay * bz) - (az * by)) * placed[0].normalX) + (((az * bx) - (ax * bz)) * placed[0].normalY) +
                           (((ax * by) - (ay * bx)) * placed[0].normalZ);
      Assert::IsTrue(facing > 0.0f, L"a placement turned a triangle inside out");
    }
  }

  /// **ONE VARIANT'S ROCKS, AND ONLY THEM**, each a whole copy of the mesh with its indices moved past the
  /// copies before it.
  TEST_METHOD(AVariantsFieldHoldsItsRocksAndNoOthers)
  {
    const Outpost::HullMesh triangle = Triangle();
    const std::vector<Outpost::Placement> rocks{RockAt(0, 0), RockAt(1000, 0), RockAt(2000, 0)};
    std::vector<Outpost::RockLook> looks(3);
    looks[0].variant = 2;
    looks[1].variant = 0;
    looks[2].variant = 2;

    Outpost::HullMesh baked;
    Assert::IsTrue(Outpost::BuildVariantField(triangle, 2, rocks, looks, baked));
    Assert::AreEqual(std::size_t{6}, baked.vertices.size());
    Assert::AreEqual(std::size_t{6}, baked.indices.size());
    Assert::AreEqual(std::uint16_t{3}, baked.indices[3], L"the second copy's indices were not offset");
    Assert::AreEqual(2001.0f, baked.vertices[3].x, 0.001f, L"the second copy is not the third rock");

    Assert::IsTrue(Outpost::BuildVariantField(triangle, 4, rocks, looks, baked));
    Assert::AreEqual(std::size_t{0}, baked.vertices.size(), L"a variant nobody chose has no rocks");
  }

  /// **PAST SIXTEEN BITS OF INDEX, NOTHING RATHER THAN PART.** The field holds at most 88 rocks and a
  /// variant 378 vertices, so this is a guard and not a case -- which is why it is pinned.
  TEST_METHOD(AFieldPastTheIndexLimitIsRefusedWhole)
  {
    Outpost::HullMesh big;
    big.vertices.resize(400);
    big.indices = {0, 1, 2};
    const std::vector<Outpost::Placement> rocks(200, RockAt(0, 0));
    const std::vector<Outpost::RockLook> looks(200);

    Outpost::HullMesh baked;
    Assert::IsFalse(Outpost::BuildVariantField(big, 0, rocks, looks, baked));
    Assert::AreEqual(std::size_t{0}, baked.vertices.size());
  }
};

/// Q83. **A spent rock is left out of the bake**, and the others keep their looks.
TEST_CLASS(TheSpentRocksAreGone)
{
public:
  TEST_METHOD(OmittingKeepsTheSurvivorsInStep)
  {
    const std::vector<Outpost::Placement> field = Outpost::GenerateField(MATCH_SEED, 2);
    const std::vector<Outpost::RockLook> looks = Outpost::RockLooks(MATCH_SEED, field, CatalogRadii());
    const std::vector<std::uint8_t> spent{0x02, 0x00, 0x01};

    std::vector<Outpost::Placement> liveRocks;
    std::vector<Outpost::RockLook> liveLooks;
    Outpost::OmitSpentRocks(field, looks, spent, liveRocks, liveLooks);
    Assert::AreEqual(field.size() - 2, liveRocks.size());
    Assert::AreEqual(liveRocks.size(), liveLooks.size());
    Assert::IsTrue(liveRocks[1].position == field[2].position, L"rock 1 was not the one left out");
    Assert::AreEqual(looks[2].scale, liveLooks[1].scale, L"a survivor's look moved");
    Assert::IsTrue(liveRocks[15].position == field[17].position, L"rock 16 was not the one left out");
  }
};

} // namespace GameClientTests
