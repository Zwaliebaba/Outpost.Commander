#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
[[nodiscard]] Neuron::CmoVertex Authored(float _x, float _y, float _z, std::uint32_t _color = 0) noexcept
{
  Neuron::CmoVertex vertex;
  vertex.positionX = _x;
  vertex.positionY = _y;
  vertex.positionZ = _z;
  vertex.color = _color;
  return vertex;
}
} // namespace

/// **THE HANDOFF AUTHORS Y UP AND THIS CAMERA IS Z UP.** `GameClient/Camera.cpp` said the conversion
/// belonged where the mesh is loaded; these are the tests that say it is right.
TEST_CLASS(TheAuthoredToWorldConversion)
{
public:
  /// The nose is the maximum-Z vertex in the file (`manifest.json`'s landmark test for the Frigate),
  /// and heading zero looks along +x -- so the nose has to come out on +x or every ship in the game
  /// points sideways.
  TEST_METHOD(TheNoseBecomesPositiveX)
  {
    const Outpost::HullVertex nose = Outpost::ToWorldVertex(Authored(0.0f, 0.0f, 45.0f));
    Assert::AreEqual(45.0f, nose.x);
    Assert::AreEqual(0.0f, nose.y);
    Assert::AreEqual(0.0f, nose.z);
  }

  /// The top is the maximum-Y vertex in the file (the Station's hub tower), and the plane is Z = 0.
  TEST_METHOD(TheTopBecomesPositiveZ)
  {
    const Outpost::HullVertex top = Outpost::ToWorldVertex(Authored(0.0f, 42.0f, 0.0f));
    Assert::AreEqual(0.0f, top.x);
    Assert::AreEqual(0.0f, top.y);
    Assert::AreEqual(42.0f, top.z);
  }

  /// **AND THE THIRD AXIS IS NEGATED, WHICH IS THE HANDEDNESS.** The authored frame is left-handed
  /// and the world is right-handed, so the conversion has to be a reflection -- one sign flip, and
  /// this is it. Getting it wrong mirrors every hull, which on a symmetric ship is invisible until
  /// something asymmetric arrives.
  TEST_METHOD(TheRemainingAxisIsNegated)
  {
    const Outpost::HullVertex side = Outpost::ToWorldVertex(Authored(27.0f, 0.0f, 0.0f));
    Assert::AreEqual(-27.0f, side.y);
  }

  /// **IT IS A REFLECTION AND NOT A ROTATION**, asserted through the determinant rather than by
  /// eye: the images of the three authored axes form a matrix whose determinant must be -1.
  TEST_METHOD(TheConversionIsAReflection)
  {
    const Outpost::HullVertex ax = Outpost::ToWorldVertex(Authored(1.0f, 0.0f, 0.0f));
    const Outpost::HullVertex ay = Outpost::ToWorldVertex(Authored(0.0f, 1.0f, 0.0f));
    const Outpost::HullVertex az = Outpost::ToWorldVertex(Authored(0.0f, 0.0f, 1.0f));

    const float determinant =
      (ax.x * ((ay.y * az.z) - (ay.z * az.y))) - (ay.x * ((ax.y * az.z) - (ax.z * az.y))) + (az.x * ((ax.y * ay.z) - (ax.z * ay.y)));
    Assert::AreEqual(-1.0f, determinant, 0.0001f, L"the conversion stopped being a handedness flip");
  }

  /// The normal takes the same conversion as the position, or lighting is wrong in a way that reads
  /// as a bad light rig rather than as a loader bug.
  TEST_METHOD(TheNormalTakesTheSameConversion)
  {
    Neuron::CmoVertex vertex = Authored(0.0f, 0.0f, 0.0f);
    vertex.normalX = 1.0f;
    vertex.normalY = 0.0f;
    vertex.normalZ = 0.0f;

    const Outpost::HullVertex world = Outpost::ToWorldVertex(vertex);
    Assert::AreEqual(0.0f, world.normalX);
    Assert::AreEqual(-1.0f, world.normalY);
    Assert::AreEqual(0.0f, world.normalZ);
  }

  /// **THE COLOUR'S LOW BYTE IS RED.** The converter writes the DWORD little-endian, so `0x00BBGGRR`;
  /// reading it the other way round swaps the team selector for the hull tone, which draws every ship
  /// in its owner's colour with no shading at all.
  TEST_METHOD(TheTeamSelectorIsRedAndTheToneIsGreen)
  {
    const Outpost::HullVertex team = Outpost::ToWorldVertex(Authored(0.0f, 0.0f, 0.0f, 0xFF0080FFu));
    Assert::AreEqual(1.0f, team.teamBlend, 0.001f, L"red is the team selector");
    Assert::AreEqual(128.0f / 255.0f, team.hullTone, 0.001f, L"green is the hull tone");

    const Outpost::HullVertex hull = Outpost::ToWorldVertex(Authored(0.0f, 0.0f, 0.0f, 0xFF00FF00u));
    Assert::AreEqual(0.0f, hull.teamBlend, 0.001f);
    Assert::AreEqual(1.0f, hull.hullTone, 0.001f);
  }
};

TEST_CLASS(LoadingAHull)
{
public:
  /// **THE WINDING IS REVERSED**, because the conversion is a reflection. A mesh copied straight
  /// through faces inward and vanishes under back-face culling, which looks like the mesh failing to
  /// load rather than like a handedness bug.
  TEST_METHOD(EveryTriangleIsReversed)
  {
    Neuron::CmoMesh read;
    read.vertices = {Authored(0.0f, 0.0f, 0.0f), Authored(1.0f, 0.0f, 0.0f), Authored(0.0f, 1.0f, 0.0f), Authored(1.0f, 1.0f, 0.0f)};
    read.indices = {0, 1, 2, 1, 3, 2};

    Outpost::HullMesh mesh;
    Assert::IsTrue(Outpost::LoadHullMesh(read, 60.0f, mesh));

    const std::vector<std::uint16_t> expected{2, 1, 0, 2, 3, 1};
    Assert::IsTrue(mesh.indices == expected, L"the winding was not reversed");
    Assert::AreEqual(static_cast<std::size_t>(4), mesh.vertices.size());
    Assert::AreEqual(60.0f, mesh.longestUnits);
  }

  /// An index list that is not a whole number of triangles is refused rather than half drawn.
  TEST_METHOD(APartialTriangleIsRefused)
  {
    Neuron::CmoMesh read;
    read.vertices = {Authored(0.0f, 0.0f, 0.0f), Authored(1.0f, 0.0f, 0.0f)};
    read.indices = {0, 1};

    Outpost::HullMesh mesh;
    Assert::IsFalse(Outpost::LoadHullMesh(read, 60.0f, mesh));
  }

  TEST_METHOD(AnEmptyMeshIsRefused)
  {
    Outpost::HullMesh mesh;
    Assert::IsFalse(Outpost::LoadHullMesh(Neuron::CmoMesh{}, 60.0f, mesh));
  }
};

/// R9's line, checked: the map from a hull to a file is the only part of this path that knows what a
/// `Scout` is.
TEST_CLASS(TheHullToMeshMap)
{
public:
  TEST_METHOD(EveryHullWithAMeshNamesOneInTheCatalog)
  {
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      const std::string_view name = Outpost::MeshNameForHull(hull.id);
      if (name.empty())
      {
        continue;
      }
      Assert::IsNotNull(Outpost::FindMesh(name), L"a hull names a mesh the manifest does not have");
    }
  }

  /// **THE `Cruiser` IS THE ONE HULL NO FILE BACKS**, because nothing builds one. It names nothing
  /// rather than naming something wrong.
  TEST_METHOD(TheCruiserNamesNoMesh)
  {
    Assert::IsTrue(Outpost::MeshNameForHull(Outpost::HullId::Cruiser).empty());
  }

  /// A snapshot carries a design identity and not a hull (ADR-003), so this is the call the renderer
  /// actually makes.
  TEST_METHOD(EveryShippedDesignResolvesToAMesh)
  {
    Assert::AreEqual(std::string_view{"Scout"}, Outpost::MeshNameForDesign(Outpost::DesignId::Miner));
    Assert::AreEqual(std::string_view{"Frigate"}, Outpost::MeshNameForDesign(Outpost::DesignId::Fighter));
    Assert::AreEqual(std::string_view{"Station"}, Outpost::MeshNameForDesign(Outpost::DesignId::Station));

    // M2.10b: a module draws by its level, so four designs on one hull are four shapes.
    Assert::AreEqual(std::string_view{"ModuleShipyardL1"}, Outpost::MeshNameForDesign(Outpost::DesignId::ModuleShipyardL1));
    Assert::AreEqual(std::string_view{"ModuleShipyardL2"}, Outpost::MeshNameForDesign(Outpost::DesignId::ModuleShipyardL2));
    Assert::AreEqual(std::string_view{"ModuleOreProcessorL1"}, Outpost::MeshNameForDesign(Outpost::DesignId::ModuleOreProcessorL1));
    Assert::AreEqual(std::string_view{"ModuleOreProcessorL2"}, Outpost::MeshNameForDesign(Outpost::DesignId::ModuleOreProcessorL2));

    Assert::IsTrue(Outpost::MeshNameForDesign(static_cast<Outpost::DesignId>(99)).empty());
  }

  /// **SEVEN MESHES SHIP**: M1.9's three and M2.10b's four module levels, and every one of them is a
  /// mesh the manifest has.
  TEST_METHOD(TheSevenShippedMeshesAreInTheCatalog)
  {
    Assert::AreEqual(static_cast<std::size_t>(7), Outpost::ShippedMeshes().size());
    for (const std::string_view name : Outpost::ShippedMeshes())
    {
      Assert::IsNotNull(Outpost::FindMesh(name));
    }
  }

  /// **EVERY DESIGN DRAWS WITH A MESH THAT SHIPS**, which is what sizes the renderer's arrays: a design
  /// whose mesh is not uploaded would be an entity nobody sees.
  TEST_METHOD(EveryDesignDrawsWithAShippedMesh)
  {
    for (const Outpost::DesignEntry& design : Outpost::Designs())
    {
      const std::string_view name = Outpost::MeshNameForDesign(design.id);
      bool shipped = false;
      for (const std::string_view candidate : Outpost::ShippedMeshes())
      {
        shipped = shipped || candidate == name;
      }
      Assert::IsTrue(shipped, L"a design draws with a mesh the client does not upload");
    }
  }

  /// **Q37's TWO STATEMENTS, AGAIN, IN THE CLIENT.** `Scripts/CheckMeshes.py` compares the catalog
  /// against the FILE; this compares it against the generated header, so a manifest that changed
  /// without the catalog moving fails the build as well as the script.
  ///
  /// **A HULL IS BOUNDED BY EVERY MESH THAT DRAWS IT** (M2.10b): its own, and each design's on that hull,
  /// which for the `ModuleFrame` is four module levels whose shipyards are longer than the bare frame.
  TEST_METHOD(EveryHullSizeMatchesItsLongestMesh)
  {
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      const std::string_view name = Outpost::MeshNameForHull(hull.id);
      if (name.empty())
      {
        continue;
      }
      const Outpost::MeshEntry* entry = Outpost::FindMesh(name);
      Assert::IsNotNull(entry);
      float longestUnits = entry->longestUnits;
      for (const Outpost::DesignEntry& design : Outpost::Designs())
      {
        if (design.hull != hull.id)
        {
          continue;
        }
        const Outpost::MeshEntry* drawn = Outpost::FindMesh(Outpost::MeshNameForDesign(design.id));
        Assert::IsNotNull(drawn);
        longestUnits = drawn->longestUnits > longestUnits ? drawn->longestUnits : longestUnits;
      }

      // The catalog rounds UP, because the figure is a bound.
      const auto authored = static_cast<std::uint16_t>(longestUnits + 0.9999f);
      Assert::AreEqual(static_cast<int>(authored), static_cast<int>(hull.sizeUnits), L"Q37's two statements disagree");
    }
  }
};

/// The palette, which is what makes one instanced draw cover every ship of a shape.
TEST_CLASS(ThePalette)
{
public:
  /// The three stops, against `manifest.json`'s hex: DEEP #1B212A, BASE #414B58, EDGE #828E9C.
  TEST_METHOD(TheThreeHullStopsAreTheManifests)
  {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    Outpost::HullToneColor(0.0f, r, g, b);
    Assert::AreEqual(0x1B / 255.0f, r, 0.002f);
    Assert::AreEqual(0x21 / 255.0f, g, 0.002f);
    Assert::AreEqual(0x2A / 255.0f, b, 0.002f);

    Outpost::HullToneColor(0.5f, r, g, b);
    Assert::AreEqual(0x41 / 255.0f, r, 0.002f);
    Assert::AreEqual(0x4B / 255.0f, g, 0.002f);
    Assert::AreEqual(0x58 / 255.0f, b, 0.002f);

    Outpost::HullToneColor(1.0f, r, g, b);
    Assert::AreEqual(0x82 / 255.0f, r, 0.002f);
    Assert::AreEqual(0x8E / 255.0f, g, 0.002f);
    Assert::AreEqual(0x9C / 255.0f, b, 0.002f);
  }

  /// Monotonic from deep to edge, which is what makes it read as a ramp rather than as three
  /// materials.
  TEST_METHOD(TheRampBrightensMonotonically)
  {
    float previous = -1.0f;
    for (float tone = 0.0f; tone <= 1.0f; tone += 0.05f)
    {
      float r = 0.0f;
      float g = 0.0f;
      float b = 0.0f;
      Outpost::HullToneColor(tone, r, g, b);
      Assert::IsTrue(r >= previous - 0.0001f, L"the hull ramp went backwards");
      previous = r;
    }
  }

  /// Player one is `OWN`, the cyan the handoff's plates were composed against.
  TEST_METHOD(PlayerOneIsTheOwnColor)
  {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    Outpost::TeamColor(1, r, g, b);
    Assert::AreEqual(0x38 / 255.0f, r, 0.002f);
    Assert::AreEqual(0xD1 / 255.0f, g, 0.002f);
    Assert::AreEqual(0xF5 / 255.0f, b, 0.002f);
  }

  /// **THE TEAM PALETTE MUST NOT COLLIDE WITH THE HULL PALETTE**, which the manifest says in as many
  /// words -- a team colour that reads as hull shading is a ship whose owner is unreadable.
  TEST_METHOD(NoTeamColorIsNearAHullTone)
  {
    for (Outpost::PlayerId player = 1; player <= 4; ++player)
    {
      float tr = 0.0f;
      float tg = 0.0f;
      float tb = 0.0f;
      Outpost::TeamColor(player, tr, tg, tb);

      for (float tone = 0.0f; tone <= 1.0f; tone += 0.1f)
      {
        float hr = 0.0f;
        float hg = 0.0f;
        float hb = 0.0f;
        Outpost::HullToneColor(tone, hr, hg, hb);

        const float distance = ((tr - hr) * (tr - hr)) + ((tg - hg) * (tg - hg)) + ((tb - hb) * (tb - hb));
        Assert::IsTrue(distance > 0.05f, L"a team colour is too close to a hull tone");
      }
    }
  }

  /// Nobody's ships take the hull's own base tone.
  TEST_METHOD(NoPlayerTakesTheHullBase)
  {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    Outpost::TeamColor(Outpost::NO_PLAYER, r, g, b);
    Assert::AreEqual(0x41 / 255.0f, r, 0.002f);
  }

  /// **A PLAYER PAST THE PALETTE TAKES ITS LAST COLOR** (ADR-023), never an index off the end of the table
  /// -- which is what a stress run's ninety-ninth player would otherwise read.
  TEST_METHOD(APlayerPastThePaletteTakesTheLastColor)
  {
    float lastR = 0.0f;
    float lastG = 0.0f;
    float lastB = 0.0f;
    Outpost::TeamColor(4, lastR, lastG, lastB);

    for (const Outpost::PlayerId player : {Outpost::PlayerId{5}, Outpost::PlayerId{99}, Outpost::PlayerId{254}})
    {
      float r = 0.0f;
      float g = 0.0f;
      float b = 0.0f;
      Outpost::TeamColor(player, r, g, b);
      Assert::AreEqual(lastR, r);
      Assert::AreEqual(lastG, g);
      Assert::AreEqual(lastB, b);
    }
  }
};

} // namespace GameClientTests
