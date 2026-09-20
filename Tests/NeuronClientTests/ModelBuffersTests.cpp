#include "pch.h"

#include "ModelBuffers.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <tuple>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The loader's split (m1-vertical-slice/K1; ADR-011). What is under test is the rule the pass
// depends on and the shader cannot check: one vertex per (description vertex, triangle colour,
// triangle normal), the normal the SDK's outward one rather than the textbook's, and the positions
// in world units rather than in the subunits Content stores. The box the tests build is the one
// Tools/MakePlaceholderModels.py writes, corner order and winding included, so that what passes
// here is what GameData\Models holds.
namespace ClientTests
{

namespace
{

constexpr float TOLERANCE = 1e-4f;
constexpr std::int32_t HALF_LENGTH = 512; // two world units
constexpr std::int32_t HALF_WIDTH = 256;  // one
constexpr std::int32_t HEIGHT = 1024;     // four
constexpr std::array<std::uint8_t, 4> GRAY = {130, 128, 120, 255};
constexpr std::array<std::uint8_t, 4> RUST = {160, 70, 40, 255};
constexpr std::array<std::uint8_t, 4> SLOT = {0, 0, 0, 0}; ///< The team-colour slot: alpha zero

/// The generator's six faces, in its order: top, bottom, -z, +z, +x, -x.
constexpr std::array<std::array<std::uint16_t, 4>, 6> FACES = {{
  {4, 5, 6, 7},
  {3, 2, 1, 0},
  {0, 1, 5, 4},
  {2, 3, 7, 6},
  {1, 2, 6, 5},
  {3, 0, 4, 7},
}};

/// A box with one colour per triangle, twelve of them, so that a test can colour the two triangles
/// of a face differently and see what the split does.
/// A correctly wound closed solid that the centroid count calls partly inward: the shape ADR-011's
/// promise does not hold for. See its definition for why it is an L and not a plate.
[[nodiscard]] Outpost::ModelDesc ElbowModel();

[[nodiscard]] Outpost::ModelDesc BoxModel(const std::array<std::array<std::uint8_t, 4>, 12>& _colors, bool _reversed = false)
{
  Outpost::ModelDesc model{};
  model.version = Outpost::MODEL_DESC_VERSION;
  model.id = "Box";
  model.vertices = {
    {-HALF_LENGTH, 0, -HALF_WIDTH},    {HALF_LENGTH, 0, -HALF_WIDTH},       {HALF_LENGTH, 0, HALF_WIDTH},
    {-HALF_LENGTH, 0, HALF_WIDTH},     {-HALF_LENGTH, HEIGHT, -HALF_WIDTH}, {HALF_LENGTH, HEIGHT, -HALF_WIDTH},
    {HALF_LENGTH, HEIGHT, HALF_WIDTH}, {-HALF_LENGTH, HEIGHT, HALF_WIDTH},
  };
  std::size_t triangle = 0;
  for (const std::array<std::uint16_t, 4>& face : FACES)
  {
    for (const std::array<std::uint16_t, 3>& corners :
         {std::array<std::uint16_t, 3>{face[0], face[1], face[2]}, std::array<std::uint16_t, 3>{face[0], face[2], face[3]}})
    {
      // Reversed swaps the second and third corner, which is a face wound the other way and the
      // one data fault a baked normal can report.
      model.triangles.push_back(
        {corners[0], _reversed ? corners[2] : corners[1], _reversed ? corners[1] : corners[2], _colors[triangle], 0});
      ++triangle;
    }
  }
  return model;
}

[[nodiscard]] std::array<std::array<std::uint8_t, 4>, 12> SixColors()
{
  std::array<std::array<std::uint8_t, 4>, 12> colors{};
  for (std::size_t face = 0; face < 6; ++face)
  {
    // A colour per face, distinct by its red channel, both triangles of the face alike.
    const std::array<std::uint8_t, 4> color = {static_cast<std::uint8_t>(10 * (face + 1)), 128, 120, 255};
    colors[face * 2] = color;
    colors[face * 2 + 1] = color;
  }
  return colors;
}

[[nodiscard]] std::array<std::array<std::uint8_t, 4>, 12> OneColor(const std::array<std::uint8_t, 4>& _color)
{
  std::array<std::array<std::uint8_t, 4>, 12> colors{};
  colors.fill(_color);
  return colors;
}

/// An L-shaped prism, correctly wound, whose own centroid lies OUTSIDE it.
///
/// WHY AN L AND NOT A PLATE, which is what this fixture was first written as and which proved
/// nothing. The centroid the count measures against is the MEAN OF THE VERTICES, and a mean is a
/// convex combination of them - so on any CONVEX solid it lies inside, every outward normal points
/// away from it, and the count reads zero however thin the solid is. A plate is convex. The
/// authored models are not: a hull with a turret and wheels is an assembly of boxes, and the mean
/// of its corners can fall in the air between two of them.
///
/// THE ARITHMETIC, so that the case does not rest on running it. The outline is the six corners
/// (0,0) (4,0) (4,1) (1,1) (1,4) (0,4) in cells of CORNER subunits, extruded from y = 0 to
/// y = HEIGHT. The mean of the twelve vertices is x = z = 10/6 = 1.67 cells, which is outside the
/// L: at z = 1.67 the solid reaches only to x = 1. So the inner face at x = 1 has an outward
/// normal of +x with the centroid further out along +x, and the inner face at z = 1 has the same
/// in z. Those two quads - four triangles - are the ones the count calls inward, and the solid is
/// wound exactly as ADR-011 asks.
Outpost::ModelDesc ElbowModel()
{
  constexpr std::int32_t CORNER = 256;
  constexpr std::array<std::array<std::int32_t, 2>, 6> OUTLINE = {{{0, 0}, {4, 0}, {4, 1}, {1, 1}, {1, 4}, {0, 4}}};

  Outpost::ModelDesc model{};
  model.version = Outpost::MODEL_DESC_VERSION;
  model.id = "Elbow";
  for (const std::array<std::int32_t, 2>& corner : OUTLINE)
  {
    model.vertices.push_back({corner[0] * CORNER, 0, corner[1] * CORNER});
  }
  for (const std::array<std::int32_t, 2>& corner : OUTLINE)
  {
    model.vertices.push_back({corner[0] * CORNER, HEIGHT, corner[1] * CORNER});
  }
  const auto face = [&model](std::uint16_t _a, std::uint16_t _b, std::uint16_t _c, std::uint16_t _d)
  {
    model.triangles.push_back({_a, _b, _c, GRAY, 0});
    model.triangles.push_back({_a, _c, _d, GRAY, 0});
  };
  // The six walls, each from an outline edge, wound so the normal points out of the solid; then
  // the floor and the ceiling, fanned from corner 0, which is in the L's kernel and so sees all of
  // it. The FLOOR'S winding is invisible to the volume - it lies at y = 0, where its contribution
  // to the sum is zero whichever way round it is - and visible to the centroid count, so it is
  // wound correctly here for the count's sake and the volume says nothing about it.
  for (std::uint16_t edge = 0; edge < 6; ++edge)
  {
    const auto next = static_cast<std::uint16_t>((edge + 1) % 6);
    face(edge, next, static_cast<std::uint16_t>(next + 6), static_cast<std::uint16_t>(edge + 6));
  }
  for (std::uint16_t corner = 1; corner + 1 < 6; ++corner)
  {
    model.triangles.push_back({0, static_cast<std::uint16_t>(corner + 1), corner, GRAY, 0});                                 // floor
    model.triangles.push_back({6, static_cast<std::uint16_t>(corner + 6), static_cast<std::uint16_t>(corner + 7), GRAY, 0}); // ceiling
  }
  return model;
}

/// The normal of the triangle the _triangle-th three indices name.
[[nodiscard]] std::array<float, 3> NormalOf(const Neuron::ModelMesh& _mesh, std::size_t _triangle)
{
  const Neuron::GeometryVertex& vertex = _mesh.vertices[_mesh.indices[_triangle * 3]];
  return {vertex.nx, vertex.ny, vertex.nz};
}

void AssertNormal(const std::array<float, 3>& _expected, const std::array<float, 3>& _actual, const wchar_t* _message)
{
  Assert::AreEqual(_expected[0], _actual[0], TOLERANCE, _message);
  Assert::AreEqual(_expected[1], _actual[1], TOLERANCE, _message);
  Assert::AreEqual(_expected[2], _actual[2], TOLERANCE, _message);
}

} // namespace

TEST_CLASS(ModelBuffersTests)
{
public:
  TEST_METHOD(EveryCornerSplitsOncePerFaceItBelongsTo)
  {
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(BoxModel(SixColors()));
    // Eight corners, each in three faces, and the two triangles of a face agree on colour and
    // normal so they share: 24 rather than the 36 a blind triplication would write.
    Assert::AreEqual(std::size_t{24}, mesh.vertices.size());
    Assert::AreEqual(std::size_t{36}, mesh.indices.size());
  }

  TEST_METHOD(TheTwoTrianglesOfAFaceShareTheirVerticesEvenWhenEveryFaceIsOneColor)
  {
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(BoxModel(OneColor(GRAY)));
    // One colour throughout, so only the normal splits, and it splits by face: still 24.
    Assert::AreEqual(std::size_t{24}, mesh.vertices.size());
  }

  TEST_METHOD(ADifferentColorOnTheSameFaceSplitsThatFace)
  {
    std::array<std::array<std::uint8_t, 4>, 12> colors = OneColor(GRAY);
    colors[0] = RUST; // the top face's first triangle alone
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(BoxModel(colors));
    // The top face's four vertices become six: three for each triangle, sharing nothing, because a
    // flat colour reaches the pixel shader from the triangle's own first vertex.
    Assert::AreEqual(std::size_t{26}, mesh.vertices.size());
  }

  TEST_METHOD(TheOutwardNormalUsesTheSdkCrossNotTheTextbookOne)
  {
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(BoxModel(SixColors()));
    // The face order of the generator: top, bottom, -z, +z, +x, -x, two triangles each. This is
    // the whole of ADR-011's handedness in one assertion; get it backwards and every model is lit
    // from inside and comes out black.
    AssertNormal({0.0f, 1.0f, 0.0f}, NormalOf(mesh, 0), L"the top face faces up");
    AssertNormal({0.0f, -1.0f, 0.0f}, NormalOf(mesh, 2), L"the bottom face faces down");
    AssertNormal({0.0f, 0.0f, -1.0f}, NormalOf(mesh, 4), L"the -z face faces -z");
    AssertNormal({0.0f, 0.0f, 1.0f}, NormalOf(mesh, 6), L"the +z face faces +z");
    AssertNormal({1.0f, 0.0f, 0.0f}, NormalOf(mesh, 8), L"the +x face faces +x");
    AssertNormal({-1.0f, 0.0f, 0.0f}, NormalOf(mesh, 10), L"the -x face faces -x");
  }

  TEST_METHOD(BothTrianglesOfAFaceCarryThatFaceNormal)
  {
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(BoxModel(SixColors()));
    for (std::size_t face = 0; face < 6; ++face)
    {
      AssertNormal(NormalOf(mesh, face * 2), NormalOf(mesh, face * 2 + 1), L"a face is one plane");
    }
    std::set<std::tuple<float, float, float>> normals;
    for (const Neuron::GeometryVertex& vertex : mesh.vertices)
    {
      normals.emplace(vertex.nx, vertex.ny, vertex.nz);
    }
    Assert::AreEqual(std::size_t{6}, normals.size(), L"six faces, six normals");
  }

  TEST_METHOD(NoTriangleOfAClosedSolidFacesInward)
  {
    Assert::AreEqual(std::uint32_t{0}, Neuron::BuildModelMesh(BoxModel(SixColors())).inwardFacingTriangles);
    // Wound the other way, every one of the twelve does. A BOX is convex, which is the only shape
    // this count is trustworthy on - see the orientation case below, and ADR-011's measurements.
    Assert::AreEqual(std::uint32_t{12}, Neuron::BuildModelMesh(BoxModel(SixColors(), true)).inwardFacingTriangles);
  }

  TEST_METHOD(TheEnclosedVolumeIsPositiveOutwardAndNegatedWhenTheModelIsWoundInsideOut)
  {
    // THE ORIENTATION GATE (m1-vertical-slice/C10). Exact for a closed mesh, where the centroid
    // count above is a proxy that only holds on a convex one: summed over the faces, the centre
    // dotted with the outward normal is six times the volume they enclose.
    const Neuron::ModelMesh outward = Neuron::BuildModelMesh(BoxModel(SixColors()));
    const Neuron::ModelMesh insideOut = Neuron::BuildModelMesh(BoxModel(SixColors(), true));
    Assert::IsTrue(outward.signedVolumeSubunits > 0.0, L"a box wound ADR-011's way encloses a positive volume");
    Assert::IsTrue(insideOut.signedVolumeSubunits < 0.0, L"and reversing every face negates it");

    // The magnitudes match, because reversing a winding changes no vertex: six times the box.
    Assert::AreEqual(outward.signedVolumeSubunits, -insideOut.signedVolumeSubunits, 1.0, L"the same solid either way round");
    const double box = static_cast<double>(2 * HALF_LENGTH) * static_cast<double>(2 * HALF_WIDTH) * static_cast<double>(HEIGHT);
    Assert::AreEqual(6.0 * box, outward.signedVolumeSubunits, box / 1000.0, L"six times the box it is");
  }

  TEST_METHOD(ANonConvexSolidFoolsTheCentroidCountAndNotTheVolume)
  {
    // WHY THE GATE MOVED, kept as a case so the reason cannot be lost (m1-vertical-slice/C10).
    // ADR-011 introduced the centroid count on the promise that "a closed solid has none of these".
    // That holds for the CONVEX placeholders it was measured on and for nothing the owner
    // authored: the centroid is the mean of the vertices, which lies inside any convex solid, and
    // an assembly of boxes is not convex. Measured over the twenty-two models, the count read
    // 6,356 of 9,632 while every one of them was inside out, and 3,276 once they were corrected -
    // it moved the right way and reached neither zero nor the total, which is a diagnostic and not
    // a gate. The volume is not fooled by any of it.
    const Neuron::ModelMesh elbow = Neuron::BuildModelMesh(ElbowModel());
    Assert::IsTrue(elbow.signedVolumeSubunits > 0.0, L"the elbow is wound outward");
    Assert::AreEqual(std::uint32_t{4}, elbow.inwardFacingTriangles,
                     L"and the count calls its two inner walls inward anyway, which is the whole point");

    // AND THE VOLUME IS THE L'S OWN, not the box around it: seven cells of footprint and not
    // sixteen, times the height, times the six the sum carries.
    constexpr double CORNER = 256.0;
    const double solid = 7.0 * CORNER * CORNER * static_cast<double>(HEIGHT);
    Assert::AreEqual(6.0 * solid, elbow.signedVolumeSubunits, solid / 1000.0, L"six times the L it is");
  }

  TEST_METHOD(PositionsBecomeWorldUnits)
  {
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(BoxModel(SixColors()));
    float lowest = 0.0f;
    float highest = 0.0f;
    for (const Neuron::GeometryVertex& vertex : mesh.vertices)
    {
      lowest = std::min(lowest, vertex.y);
      highest = std::max(highest, vertex.y);
      Assert::AreEqual(2.0f, std::fabs(vertex.x), TOLERANCE, L"half a length is two world units");
      Assert::AreEqual(1.0f, std::fabs(vertex.z), TOLERANCE, L"half a width is one");
    }
    Assert::AreEqual(0.0f, lowest, TOLERANCE);
    Assert::AreEqual(4.0f, highest, TOLERANCE, L"1024 subunits is four world units");
  }

  TEST_METHOD(TheTeamColorSlotKeepsItsZeroAlpha)
  {
    std::array<std::array<std::uint8_t, 4>, 12> colors = OneColor(GRAY);
    colors[0] = SLOT;
    colors[1] = SLOT;
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(BoxModel(colors));
    std::size_t flagged = 0;
    for (const Neuron::GeometryVertex& vertex : mesh.vertices)
    {
      if ((vertex.color >> 24) == 0)
      {
        ++flagged;
      }
    }
    // The top face's four corners, and nothing else: the alpha survives the split, and it is what
    // the pixel shader tests to write the seat's colour unlit.
    Assert::AreEqual(std::size_t{4}, flagged);
  }

  TEST_METHOD(TheRadiusReachesTheFurthestCorner)
  {
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(BoxModel(SixColors()));
    Assert::AreEqual(std::sqrt(4.0f + 16.0f + 1.0f), mesh.radiusWorldUnits, TOLERANCE, L"a top corner, from the model's origin");
  }

  TEST_METHOD(AModelWithNothingInItBuildsAnEmptyMesh)
  {
    Outpost::ModelDesc model{};
    model.version = Outpost::MODEL_DESC_VERSION;
    model.id = "Nothing";
    const Neuron::ModelMesh empty = Neuron::BuildModelMesh(model);
    Assert::AreEqual(std::size_t{0}, empty.vertices.size());
    Assert::AreEqual(std::size_t{0}, empty.indices.size());
    Assert::AreEqual(0.0f, empty.radiusWorldUnits, TOLERANCE);

    model.vertices = {{0, 0, 0}, {256, 0, 0}, {0, 256, 0}};
    Assert::AreEqual(std::size_t{0}, Neuron::BuildModelMesh(model).vertices.size(), L"vertices without triangles draw nothing");
  }

  TEST_METHOD(ATriangleWithNoAreaOrNoSuchVertexIsDropped)
  {
    Outpost::ModelDesc model{};
    model.version = Outpost::MODEL_DESC_VERSION;
    model.id = "Broken";
    model.vertices = {{0, 0, 0}, {256, 0, 0}, {0, 0, 256}};
    model.triangles = {
      {0, 1, 2, GRAY, 0},
      {0, 1, 1, GRAY, 0}, // no area, so no normal worth writing
      {0, 1, 9, GRAY, 0}, // past the end of the vertex list; ContentValidator is what reports it
    };
    const Neuron::ModelMesh mesh = Neuron::BuildModelMesh(model);
    Assert::AreEqual(std::size_t{3}, mesh.indices.size(), L"one triangle survives");
    Assert::AreEqual(std::size_t{3}, mesh.vertices.size());
  }
};

} // namespace ClientTests
