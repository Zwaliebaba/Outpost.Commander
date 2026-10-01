#include "pch.h"
#include "RepositoryAssets.h"

#include <cmath>
#include <numbers>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr float WIDE_SCREEN = 16.0f / 9.0f;
constexpr float LAPTOP_SCREEN = 16.0f / 10.0f;
// Meters on the ground; the math is single precision.
constexpr float TOLERANCE_METERS = 0.05f;
constexpr float TOLERANCE_RADIANS = 1e-4f;

Outpost::CameraSettings RepositorySettings()
{
  return Outpost::LoadCameraSettings(ReadRepositoryAssetText("Camera.json"));
}

float Radians(float _degrees)
{
  return _degrees * std::numbers::pi_v<float> / 180.0f;
}

// Where a point of the screen meets the ground; the test fails if it does not.
DirectX::XMFLOAT2 GroundPoint(const Outpost::Camera& _camera, float _screenX, float _screenY, float _aspectRatio)
{
  const std::optional<DirectX::XMFLOAT2> point = _camera.GroundPoint(_screenX, _screenY, _aspectRatio);
  // Assert::Fail does not return, so the value is there below it.
  if (!point.has_value())
    Assert::Fail(L"The ray does not meet the ground.");
  return *point;
}

// The width of the ground across the middle of the screen, from its left edge to its right.
float GroundWidth(const Outpost::Camera& _camera, float _aspectRatio)
{
  const DirectX::XMFLOAT2 left = GroundPoint(_camera, -1.0f, 0.0f, _aspectRatio);
  const DirectX::XMFLOAT2 right = GroundPoint(_camera, 1.0f, 0.0f, _aspectRatio);
  return std::hypot(right.x - left.x, right.y - left.y);
}

void ExpectRejected(std::string_view _replace, std::string_view _with)
{
  std::string json = ReadRepositoryAssetText("Camera.json");
  const size_t at = json.find(_replace);
  Assert::AreNotEqual(std::string::npos, at);
  json.replace(at, _replace.size(), _with);
  Assert::ExpectException<Neuron::Exception>([&]
  {
    (void)Outpost::LoadCameraSettings(json);
  });
}
} // namespace

TEST_CLASS(CameraTests)
{
public:
  // Design §4: the default view is about 500 m wide, whatever the screen's shape.
  TEST_METHOD(ShowsTheDefaultWidthAtTheDefaultZoom)
  {
    const Outpost::CameraSettings settings = RepositorySettings();
    Assert::AreEqual(500.0f, settings.defaultViewWidthMeters);
    const Outpost::Camera camera(settings);
    Assert::AreEqual(500.0f, GroundWidth(camera, WIDE_SCREEN), TOLERANCE_METERS);
    Assert::AreEqual(500.0f, GroundWidth(camera, LAPTOP_SCREEN), TOLERANCE_METERS);
  }

  TEST_METHOD(LooksAtTheFocusThroughTheMiddleOfTheScreen)
  {
    Outpost::Camera camera(RepositorySettings());
    camera.Pan(120.0f, -40.0f);
    const DirectX::XMFLOAT2 middle = GroundPoint(camera, 0.0f, 0.0f, WIDE_SCREEN);
    Assert::AreEqual(camera.Focus().x, middle.x, TOLERANCE_METERS);
    Assert::AreEqual(camera.Focus().y, middle.y, TOLERANCE_METERS);
  }

  TEST_METHOD(TakesThePitchFromTheZoomAtEachLimit)
  {
    const Outpost::CameraSettings settings = RepositorySettings();
    Outpost::Camera camera(settings);

    camera.Zoom(100.0f);
    Assert::AreEqual(settings.minimumViewWidthMeters, camera.ViewWidthMeters());
    Assert::AreEqual(Radians(settings.pitchAtMinimumDegrees), camera.PitchRadians(), TOLERANCE_RADIANS);
    Assert::AreEqual(settings.minimumViewWidthMeters, GroundWidth(camera, WIDE_SCREEN), TOLERANCE_METERS);

    camera.Zoom(-100.0f);
    Assert::AreEqual(settings.maximumViewWidthMeters, camera.ViewWidthMeters());
    Assert::AreEqual(Radians(settings.pitchAtMaximumDegrees), camera.PitchRadians(), TOLERANCE_RADIANS);
    Assert::AreEqual(settings.maximumViewWidthMeters, GroundWidth(camera, WIDE_SCREEN), TOLERANCE_METERS);
  }

  TEST_METHOD(ZoomsInByTheFactorPerNotch)
  {
    const Outpost::CameraSettings settings = RepositorySettings();
    Outpost::Camera camera(settings);
    camera.Zoom(1.0f);
    Assert::AreEqual(settings.defaultViewWidthMeters / settings.zoomFactorPerNotch, camera.ViewWidthMeters(), TOLERANCE_METERS);
  }

  // At the start the camera looks along +z, so the screen's right is +x and its top is +z.
  TEST_METHOD(PansAlongTheScreensAxes)
  {
    Outpost::Camera camera(RepositorySettings());
    camera.Pan(100.0f, 0.0f);
    Assert::AreEqual(100.0f, camera.Focus().x, TOLERANCE_METERS);
    Assert::AreEqual(0.0f, camera.Focus().y, TOLERANCE_METERS);
    camera.Pan(0.0f, 50.0f);
    Assert::AreEqual(50.0f, camera.Focus().y, TOLERANCE_METERS);

    // A quarter turn counterclockwise: the screen's right is now +z.
    camera.Rotate(std::numbers::pi_v<float> / 2.0f);
    camera.Pan(10.0f, 0.0f);
    Assert::AreEqual(100.0f, camera.Focus().x, TOLERANCE_METERS);
    Assert::AreEqual(60.0f, camera.Focus().y, TOLERANCE_METERS);
  }

  TEST_METHOD(KeepsTheFocusOverTheMap)
  {
    const Outpost::CameraSettings settings = RepositorySettings();
    Outpost::Camera camera(settings);
    camera.Pan(1e6f, -1e6f);
    Assert::AreEqual(settings.focusLimitMeters, camera.Focus().x);
    Assert::AreEqual(-settings.focusLimitMeters, camera.Focus().y);
  }

  TEST_METHOD(IgnoresInputWhileTheWindowIsInTheBackground)
  {
    Outpost::Camera camera(RepositorySettings());
    Neuron::InputState input;
    input.active = false;
    input.wheelNotches = 5.0f;
    input.keysDown.set(VK_RIGHT);
    camera.Update(input, 1.0f, 1920, 1080);
    Assert::AreEqual(500.0f, camera.ViewWidthMeters());
    Assert::AreEqual(0.0f, camera.Focus().x);
  }

  TEST_METHOD(EdgeScrollsOnlyWhileTheCursorIsHeld)
  {
    Outpost::Camera camera(RepositorySettings());
    Neuron::InputState input;
    input.active = true;
    input.cursorXPixels = 1919;
    input.cursorYPixels = 500;

    input.cursorClipped = false;
    camera.Update(input, 0.5f, 1920, 1080);
    Assert::AreEqual(0.0f, camera.Focus().x);

    input.cursorClipped = true;
    camera.Update(input, 0.5f, 1920, 1080);
    Assert::IsTrue(camera.Focus().x > 0.0f);
  }

  // A and S are orders (design §9): they must not move the camera. The arrow keys do.
  TEST_METHOD(PansWithTheArrowKeysAndNotWithAOrS)
  {
    Outpost::Camera camera(RepositorySettings());
    Neuron::InputState input;
    input.active = true;
    input.cursorXPixels = 960;
    input.cursorYPixels = 540;
    input.keysDown.set('A');
    input.keysDown.set('S');
    input.keysDown.set('D');
    input.keysDown.set('W');
    camera.Update(input, 0.5f, 1920, 1080);
    Assert::AreEqual(0.0f, camera.Focus().x);
    Assert::AreEqual(0.0f, camera.Focus().y);

    input.keysDown.reset();
    input.keysDown.set(VK_RIGHT);
    camera.Update(input, 0.5f, 1920, 1080);
    Assert::IsTrue(camera.Focus().x > 0.0f);
  }

  // A pixel and the ground point under it map to each other, which picking relies on.
  TEST_METHOD(MapsPixelsAndGroundPointsBothWays)
  {
    Outpost::Camera camera(RepositorySettings());
    camera.Rotate(0.7f);
    camera.Pan(-300.0f, 120.0f);
    constexpr Outpost::Viewport viewport{.widthPixels = 1920, .heightPixels = 1080};
    for (const auto& [x, y] : {std::pair{960.0f, 540.0f}, std::pair{10.0f, 20.0f}, std::pair{1900.0f, 1000.0f}})
    {
      const std::optional<Outpost::PlanePosition> ground = camera.GroundPointAtPixel(x, y, viewport);
      Assert::IsTrue(ground.has_value());
      const std::optional<DirectX::XMFLOAT2> pixel = camera.PixelOf(ground.value_or(Outpost::PlanePosition{}), viewport);
      Assert::IsTrue(pixel.has_value());
      Assert::AreEqual(x, pixel.value_or(DirectX::XMFLOAT2{}).x, 0.05f);
      Assert::AreEqual(y, pixel.value_or(DirectX::XMFLOAT2{}).y, 0.05f);
    }
  }

  TEST_METHOD(RejectsADefaultOutsideTheLimits)
  {
    ExpectRejected(R"("defaultViewWidthMeters": 500)", R"("defaultViewWidthMeters": 5000)");
  }

  TEST_METHOD(RejectsAPitchPastStraightDown)
  {
    ExpectRejected(R"("pitchAtMaximumDegrees": 70)", R"("pitchAtMaximumDegrees": 95)");
  }

  TEST_METHOD(RejectsAZoomFactorThatDoesNotZoom)
  {
    ExpectRejected(R"("zoomFactorPerNotch": 1.15)", R"("zoomFactorPerNotch": 1)");
  }
};
} // namespace GameAppTests