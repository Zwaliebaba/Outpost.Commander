#include "pch.h"
#include "RepositoryAssets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <span>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
// How far a model's lines must stand over its faces, as WCAG's ratio of their luminances, on every face the camera can see:
// the least ratio of each kind of model, less a margin (ADR-090).
constexpr float SHIP_FLOOR = 2.7f;
constexpr float STRUCTURE_FLOOR = 3.3f;
constexpr float ROCK_FLOOR = 2.3f;
// The light's floor, which no face the camera can see falls below (ADR-090 decision 3).
constexpr float LEAST_LIGHT = 0.3f;
// The light on a face that looks straight up is this close to gray: its brightest channel at most this many times its
// dimmest.
constexpr float MOST_TINT_UP = 1.1f;
// The normals are tried in steps of this many degrees of elevation and of turn.
constexpr int STEP_DEGREES = 3;
// The set the rocks are drawn with, as GameClient names it.
constexpr std::string_view ASTEROID_SET = "Asteroids";

float LuminanceOf(const DirectX::XMFLOAT4& _color) noexcept
{
  return Outpost::Luminance(_color.x, _color.y, _color.z);
}

// Every unit normal, in steps, of a face the camera can see from some turn at its lowest pitch: a face that looks down
// further than the camera does is out of its sight.
std::vector<DirectX::XMFLOAT3> SeenNormals()
{
  const Outpost::CameraSettings camera = Outpost::LoadCameraSettings(ReadRepositoryAssetText("Camera.json"));
  const float lowestPitchRadians =
    std::min(camera.pitchAtMinimumDegrees, camera.pitchAtMaximumDegrees) * std::numbers::pi_v<float> / 180.0f;
  const float lowestY = -std::cos(lowestPitchRadians);
  std::vector<DirectX::XMFLOAT3> normals;
  for (int elevation = -90; elevation <= 90; elevation += STEP_DEGREES)
  {
    for (int turn = 0; turn < 360; turn += STEP_DEGREES)
    {
      const float up = static_cast<float>(elevation) * std::numbers::pi_v<float> / 180.0f;
      const float around = static_cast<float>(turn) * std::numbers::pi_v<float> / 180.0f;
      const DirectX::XMFLOAT3 normal{std::cos(up) * std::cos(around), std::sin(up), std::cos(up) * std::sin(around)};
      if (normal.y >= lowestY)
        normals.push_back(normal);
    }
  }
  return normals;
}

// How open a face may be: wholly, half and not at all (ADR-092). A line is never darkened, so a shut-in face only stands
// further below its lines.
constexpr std::array<float, 3> OPENNESSES{1.0f, 0.5f, 0.0f};

// The least ratio of a model's lines to its faces over _normals and every openness, each face and its line lit as
// MeshPS.hlsl and MeshLinePS.hlsl light them.
float LeastContrast(const DirectX::XMFLOAT4& _faces, const DirectX::XMFLOAT4& _lines, std::span<const DirectX::XMFLOAT3> _normals)
{
  float least = std::numeric_limits<float>::max();
  for (const DirectX::XMFLOAT3& normal : _normals)
  {
    for (const float openness : OPENNESSES)
    {
      const DirectX::XMFLOAT3 light = Outpost::FaceLight(normal, openness);
      const float face = Outpost::Luminance(_faces.x * light.x, _faces.y * light.y, _faces.z * light.z);
      const float line = LuminanceOf(_lines) * Outpost::LineLight(normal);
      least = std::min(least, (line + 0.05f) / (face + 0.05f));
    }
  }
  return least;
}

void ExpectContrast(const std::string& _what, const DirectX::XMFLOAT4& _faces, const DirectX::XMFLOAT4& _lines, float _floor,
                    std::span<const DirectX::XMFLOAT3> _normals)
{
  const float least = LeastContrast(_faces, _lines, _normals);
  const std::string message = std::format("{}'s lines stand {:.2f}:1 over its faces, under {:.2f}:1", _what, least, _floor);
  Assert::IsTrue(least >= _floor, std::wstring(message.begin(), message.end()).c_str());
}
} // namespace

TEST_CLASS(SceneShadingTests)
{
public:
  TEST_METHOD(KeepsEveryModelsLinesOverItsFaces)
  {
    const Outpost::ModelCatalog catalog = Outpost::LoadModelCatalog(ReadRepositoryAssetText("Models.json"));
    const std::vector<DirectX::XMFLOAT3> normals = SeenNormals();
    for (const Outpost::ModelSet& set : catalog.sets)
    {
      if (set.name == ASTEROID_SET)
      {
        // An ore asteroid, and a field's darker rocks (ADR-028, ADR-040).
        const DirectX::XMFLOAT4 field{set.color.x * Outpost::FIELD_SHADE, set.color.y * Outpost::FIELD_SHADE,
                                      set.color.z * Outpost::FIELD_SHADE, set.color.w};
        for (const DirectX::XMFLOAT4& rock : {set.color, field})
          ExpectContrast(set.name, Outpost::Shaded(rock, Outpost::ROCK_FILL_SHADE),
                         Outpost::EdgeColor(rock, Outpost::ROCK_EDGE_BRIGHTNESS, 0.0f), ROCK_FLOOR, normals);
        continue;
      }
      ExpectContrast(set.name + " ship", Outpost::Shaded(set.color, Outpost::FILL_SHADE),
                     Outpost::EdgeColor(set.color, Outpost::EDGE_BRIGHTNESS, Outpost::EDGE_WHITE_SHARE), SHIP_FLOOR, normals);
      const DirectX::XMFLOAT4 structure = Outpost::TowardGray(set.color, Outpost::STRUCTURE_GRAY_SHARE);
      ExpectContrast(set.name + " structure", Outpost::Shaded(structure, Outpost::STRUCTURE_FILL_SHADE),
                     Outpost::EdgeColor(structure, Outpost::EDGE_BRIGHTNESS, Outpost::EDGE_WHITE_SHARE), STRUCTURE_FLOOR, normals);
      // A trim face and its line, as GameClient paints them (ADR-095), to the same floors.
      ExpectContrast(set.name + " ship's trim", Outpost::FacePaints(set.color, Outpost::FILL_SHADE).trim,
                     Outpost::LinePaints(set.color, Outpost::EDGE_BRIGHTNESS, Outpost::EDGE_WHITE_SHARE).trim, SHIP_FLOOR, normals);
      ExpectContrast(set.name + " structure's trim", Outpost::FacePaints(structure, Outpost::STRUCTURE_FILL_SHADE).trim,
                     Outpost::LinePaints(structure, Outpost::EDGE_BRIGHTNESS, Outpost::EDGE_WHITE_SHARE).trim, STRUCTURE_FLOOR, normals);
    }
  }

  // ADR-095: the three paints of a model. Team is the model's color, shaded and lined as before paints; trim the gray as
  // light as it, shaded and lined alike; and light the one warm white, faces and lines, glowing.
  TEST_METHOD(PaintsTeamTrimAndLight)
  {
    const DirectX::XMFLOAT4 color{0.9f, 0.3f, 0.1f, 1.0f};
    const Outpost::PaintColors faces = Outpost::FacePaints(color, Outpost::FILL_SHADE);
    const Outpost::PaintColors lines = Outpost::LinePaints(color, Outpost::EDGE_BRIGHTNESS, Outpost::EDGE_WHITE_SHARE);
    const DirectX::XMFLOAT4 team = Outpost::Shaded(color, Outpost::FILL_SHADE);
    Assert::IsTrue(faces.team.x == team.x && faces.team.y == team.y && faces.team.z == team.z, L"team as every face was");
    const DirectX::XMFLOAT4 edge = Outpost::EdgeColor(color, Outpost::EDGE_BRIGHTNESS, Outpost::EDGE_WHITE_SHARE);
    Assert::IsTrue(lines.team.x == edge.x && lines.team.y == edge.y && lines.team.z == edge.z, L"and every line");
    const DirectX::XMFLOAT4 trim = Outpost::TrimColor(color);
    Assert::IsTrue(trim.x == trim.y && trim.y == trim.z, L"trim is neutral");
    Assert::AreEqual(LuminanceOf(color), LuminanceOf(trim), 1e-6f, L"and as light as the team color");
    for (const Outpost::PaintColors& paints : {faces, lines})
    {
      Assert::IsTrue(paints.trim.x == paints.trim.y && paints.trim.y == paints.trim.z, L"its faces and lines neutral too");
      Assert::IsTrue(paints.light.x == Outpost::LIGHT_PAINT_COLOR.x && paints.light.y == Outpost::LIGHT_PAINT_COLOR.y &&
                       paints.light.z == Outpost::LIGHT_PAINT_COLOR.z,
                     L"light is the one warm white");
    }
    Assert::IsTrue(
      Outpost::LIGHT_PAINT_COLOR.x > Outpost::LIGHT_PAINT_COLOR.y && Outpost::LIGHT_PAINT_COLOR.y > Outpost::LIGHT_PAINT_COLOR.z, L"warm");

    const DirectX::XMFLOAT4X4 world{};
    const Neuron::MeshPipeline::Instance painted = Outpost::PaintedInstance(world, faces, 0.25f);
    Assert::IsTrue(painted.color.x == faces.team.x && painted.secondColor.x == faces.trim.x && painted.thirdColor.x == faces.light.x,
                   L"each paint its color, in the mesh's order");
    Assert::AreEqual(1.0f, painted.thirdGlows, L"the light glows");
    Assert::AreEqual(0.25f, painted.liftShare);
    const Neuron::MeshPipeline::Instance plain = Neuron::MeshPipeline::Instance::InOneColor(world, color, 0.0f);
    Assert::IsTrue(plain.secondColor.x == color.x && plain.thirdColor.x == color.x && plain.thirdGlows == 0.0f,
                   L"a copy in one color draws every paint in it, and none glows");
  }

  TEST_METHOD(LightsNoFaceTheCameraSeesBelowTheOldAmbient)
  {
    for (const DirectX::XMFLOAT3& normal : SeenNormals())
      Assert::IsTrue(Outpost::LineLight(normal) >= LEAST_LIGHT, L"as bright as ADR-040's ambient");
  }

  TEST_METHOD(LightsAFaceThatLooksUpNearlyGray)
  {
    const DirectX::XMFLOAT3 light = Outpost::FaceLight({0.0f, 1.0f, 0.0f});
    const float brightest = std::max({light.x, light.y, light.z});
    const float dimmest = std::min({light.x, light.y, light.z});
    Assert::IsTrue(brightest <= MOST_TINT_UP * dimmest, L"nearly gray");
  }

  // ADR-092: a face is darkened by how shut in it is, by OCCLUSION_STRENGTH's share, and a line is not.
  TEST_METHOD(DarkensAFaceByHowShutInItIs)
  {
    const DirectX::XMFLOAT3 normal{-0.6f, 0.0f, 0.8f};
    const DirectX::XMFLOAT3 open = Outpost::FaceLight(normal);
    for (const float openness : OPENNESSES)
    {
      const DirectX::XMFLOAT3 shut = Outpost::FaceLight(normal, openness);
      const float share = 1.0f - (Outpost::OCCLUSION_STRENGTH * (1.0f - openness));
      Assert::AreEqual(open.x * share, shut.x, 1e-6f, L"red darkened by its share");
      Assert::AreEqual(open.y * share, shut.y, 1e-6f, L"green darkened by its share");
      Assert::AreEqual(open.z * share, shut.z, 1e-6f, L"blue darkened by its share");
    }
    Assert::AreEqual(Outpost::FaceLight(normal, 1.0f).x, Outpost::FaceLight(normal, 2.0f).x, L"no more than wholly open");
    Assert::AreEqual(Outpost::FaceLight(normal, 0.0f).x, Outpost::FaceLight(normal, -1.0f).x, L"no less than shut in");
  }

  TEST_METHOD(WarmsTheKeyAndCoolsTheFill)
  {
    Assert::IsTrue(Outpost::KEY_COLOR.x > Outpost::KEY_COLOR.y && Outpost::KEY_COLOR.y > Outpost::KEY_COLOR.z, L"a warm key");
    Assert::IsTrue(Outpost::SKY_FILL.z > Outpost::SKY_FILL.y && Outpost::SKY_FILL.y > Outpost::SKY_FILL.x, L"a cool sky");
    Assert::IsTrue(Outpost::GROUND_FILL.z > Outpost::GROUND_FILL.x, L"a cool ground");
    // The face that turns to the key is warmer than the one that turns from it.
    const DirectX::XMFLOAT3 toward = Outpost::FaceLight({-0.6f, 0.0f, 0.8f});
    const DirectX::XMFLOAT3 away = Outpost::FaceLight({0.6f, 0.0f, -0.8f});
    Assert::IsTrue(toward.x / toward.z > away.x / away.z, L"warmer toward the key");
  }
};
} // namespace GameAppTests
