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
  Assert::ExpectException<Neuron::Exception>([&] { (void)Outpost::LoadCameraSettings(json); });
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

  // ADR-019: a glow is laid along the screen's right and up. Right is level, up leans back from the camera as the
  // view tilts, the two are at right angles, and right is where panning right moves the view.
  TEST_METHOD(GivesTheScreensAxesInTheWorld)
  {
    Outpost::Camera camera(RepositorySettings());
    camera.Rotate(0.6f);
    const auto [right, up] = camera.ScreenAxes(WIDE_SCREEN);
    const auto length = [](const DirectX::XMFLOAT3& _v) { return std::sqrt((_v.x * _v.x) + (_v.y * _v.y) + (_v.z * _v.z)); };
    Assert::AreEqual(1.0f, length(right), TOLERANCE_RADIANS);
    Assert::AreEqual(1.0f, length(up), TOLERANCE_RADIANS);
    Assert::AreEqual(0.0f, right.y, TOLERANCE_RADIANS);
    Assert::IsTrue(up.y > 0.0f);
    Assert::AreEqual(0.0f, (right.x * up.x) + (right.y * up.y) + (right.z * up.z), TOLERANCE_RADIANS);

    const DirectX::XMFLOAT2 before = camera.Focus();
    camera.Pan(10.0f, 0.0f);
    const DirectX::XMFLOAT2 after = camera.Focus();
    Assert::AreEqual(10.0f, ((after.x - before.x) * right.x) + ((after.y - before.y) * right.z), TOLERANCE_METERS);
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
    const Outpost::Viewport viewport{.widthPixels = 1920, .heightPixels = 1080};
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

  // ADR-067: a length of ground across the line of sight, as long as MetersPerPixelAt gives three pixels at its point,
  // shows three pixels wide, near and far on the screen and zoomed in and out, which keeps the selection's ring a fixed
  // width on the screen at any zoom.
  TEST_METHOD(SpansAPixelAtAnyDepth)
  {
    const Outpost::Viewport viewport{.widthPixels = 1920, .heightPixels = 1080};
    for (const float notches : {-30.0f, 0.0f, 30.0f})
    {
      Outpost::Camera camera(RepositorySettings());
      camera.Rotate(0.4f);
      camera.Zoom(notches);
      const DirectX::XMFLOAT3 right = camera.ScreenAxes(viewport.AspectRatio()).first;
      std::array<float, 3> spans{};
      for (std::size_t row = 0; row < spans.size(); ++row)
      {
        // Toward the top of the screen the ground is farther, and a pixel spans more of it.
        const std::optional<Outpost::PlanePosition> point =
          camera.GroundPointAtPixel(700.0f, 300.0f + (300.0f * static_cast<float>(row)), viewport);
        Assert::IsTrue(point.has_value());
        const Outpost::PlanePosition at = point.value_or(Outpost::PlanePosition{});
        const float metersPerPixel = camera.MetersPerPixelAt(at, viewport).value_or(0.0f);
        spans[row] = metersPerPixel;
        const Outpost::PlanePosition across{.xMeters = at.xMeters + (right.x * 3.0f * metersPerPixel),
                                            .zMeters = at.zMeters + (right.z * 3.0f * metersPerPixel)};
        const DirectX::XMFLOAT2 from = camera.PixelOf(at, viewport).value_or(DirectX::XMFLOAT2{});
        const DirectX::XMFLOAT2 to = camera.PixelOf(across, viewport).value_or(DirectX::XMFLOAT2{});
        Assert::AreEqual(3.0f, std::hypot(to.x - from.x, to.y - from.y), 0.01f);
      }
      Assert::IsTrue(spans[0] > spans[1] && spans[1] > spans[2], L"a pixel spans more of the far ground");
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

  TEST_METHOD(RejectsAMinimumWiderThanTheMaximum)
  {
    ExpectRejected(R"("minimumViewWidthMeters": 150)", R"("minimumViewWidthMeters": 4000)");
  }

  // Middle-drag holds the ground under the cursor (ADR-012), so the view moves the other way from the mouse. Letting go
  // ends the drag, and the next press starts a new one where the cursor is, without a jump.
  TEST_METHOD(MiddleDragHoldsTheGroundUnderTheCursor)
  {
    constexpr Outpost::Viewport VIEWPORT{.widthPixels = 1920, .heightPixels = 1080};
    Outpost::Camera camera(RepositorySettings());
    Neuron::InputState input;
    input.active = true;
    input.keysDown.set(VK_MBUTTON);
    input.cursorXPixels = 960;
    input.cursorYPixels = 540;
    camera.Update(input, 0.0f, VIEWPORT.widthPixels, VIEWPORT.heightPixels);
    Assert::AreEqual(0.0f, camera.Focus().x, L"a press alone does not pan");
    Assert::AreEqual(0.0f, camera.Focus().y);

    // Across the middle of the screen a pixel spans the same ground everywhere, so the ground stays exactly under the cursor.
    const std::optional<Outpost::PlanePosition> held = camera.GroundPointAtPixel(960.0f, 540.0f, VIEWPORT);
    input.cursorXPixels = 1160;
    camera.Update(input, 0.0f, VIEWPORT.widthPixels, VIEWPORT.heightPixels);
    const std::optional<Outpost::PlanePosition> under = camera.GroundPointAtPixel(1160.0f, 540.0f, VIEWPORT);
    // Assert::Fail does not return, so both are there below it.
    if (!held.has_value() || !under.has_value())
      Assert::Fail(L"The middle of the screen does not meet the ground.");
    Assert::AreEqual(held->xMeters, under->xMeters, TOLERANCE_METERS);
    Assert::AreEqual(held->zMeters, under->zMeters, TOLERANCE_METERS);
    Assert::IsTrue(camera.Focus().x < 0.0f, L"the view moved the other way from the mouse");

    // Up the screen the focus moves by the same meters a pixel spans at the focus, the other way from the mouse.
    Outpost::Camera expected = camera;
    expected.Pan(0.0f, -100.0f * camera.ViewWidthMeters() / static_cast<float>(VIEWPORT.widthPixels));
    input.cursorYPixels = 440;
    camera.Update(input, 0.0f, VIEWPORT.widthPixels, VIEWPORT.heightPixels);
    Assert::AreEqual(expected.Focus().x, camera.Focus().x, TOLERANCE_METERS);
    Assert::AreEqual(expected.Focus().y, camera.Focus().y, TOLERANCE_METERS);

    const DirectX::XMFLOAT2 dropped = camera.Focus();
    input.keysDown.reset(VK_MBUTTON);
    input.cursorXPixels = 100;
    input.cursorYPixels = 100;
    camera.Update(input, 0.0f, VIEWPORT.widthPixels, VIEWPORT.heightPixels);
    input.keysDown.set(VK_MBUTTON);
    camera.Update(input, 0.0f, VIEWPORT.widthPixels, VIEWPORT.heightPixels);
    Assert::AreEqual(dropped.x, camera.Focus().x);
    Assert::AreEqual(dropped.y, camera.Focus().y);
  }

  // Each edge of the screen pans toward itself, within edgeScrollMarginPixels of it and no further in, and a corner pans
  // both ways at once.
  TEST_METHOD(EdgeScrollsTowardEachEdge)
  {
    struct Edge
    {
      std::int32_t xPixels;
      std::int32_t yPixels;
      float right;
      float forward;
    };
    const Outpost::CameraSettings settings = RepositorySettings();
    Assert::AreEqual(4, settings.edgeScrollMarginPixels);
    constexpr float ELAPSED_SECONDS = 0.5f;
    const float panMeters = settings.defaultViewWidthMeters * settings.panViewWidthsPerSecond * ELAPSED_SECONDS;
    for (const Edge& edge : {Edge{0, 540, -1.0f, 0.0f}, Edge{4, 540, -1.0f, 0.0f}, Edge{5, 540, 0.0f, 0.0f}, Edge{1919, 540, 1.0f, 0.0f},
                             Edge{1915, 540, 1.0f, 0.0f}, Edge{1914, 540, 0.0f, 0.0f}, Edge{960, 0, 0.0f, 1.0f}, Edge{960, 4, 0.0f, 1.0f},
                             Edge{960, 5, 0.0f, 0.0f}, Edge{960, 1079, 0.0f, -1.0f}, Edge{960, 1075, 0.0f, -1.0f},
                             Edge{960, 1074, 0.0f, 0.0f}, Edge{0, 0, -1.0f, 1.0f}, Edge{1919, 1079, 1.0f, -1.0f}})
    {
      Outpost::Camera camera(settings);
      Outpost::Camera expected(settings);
      expected.Pan(edge.right * panMeters, edge.forward * panMeters);
      Neuron::InputState input;
      input.active = true;
      input.cursorClipped = true;
      input.cursorXPixels = edge.xPixels;
      input.cursorYPixels = edge.yPixels;
      camera.Update(input, ELAPSED_SECONDS, 1920, 1080);
      const std::wstring where = std::format(L"at ({}, {})", edge.xPixels, edge.yPixels);
      Assert::AreEqual(expected.Focus().x, camera.Focus().x, TOLERANCE_METERS, where.c_str());
      Assert::AreEqual(expected.Focus().y, camera.Focus().y, TOLERANCE_METERS, where.c_str());
    }
  }

  // A ray that does not come down to the ground meets it nowhere, a point behind the eye shows nowhere, and a viewport
  // with no area has no pixels.
  TEST_METHOD(FindsNothingAboveTheHorizonOrBehindTheEye)
  {
    constexpr Outpost::Viewport VIEWPORT{.widthPixels = 1920, .heightPixels = 1080};
    Outpost::CameraSettings settings = RepositorySettings();
    // Looking 10 degrees down with 30 degrees above the line of sight, the top of the screen looks 20 degrees above the horizon.
    settings.pitchAtMinimumDegrees = 10.0f;
    settings.pitchAtMaximumDegrees = 10.0f;
    settings.verticalFieldOfViewDegrees = 60.0f;
    const Outpost::Camera camera(settings);
    Assert::AreEqual(Radians(10.0f), camera.PitchRadians(), TOLERANCE_RADIANS);
    Assert::IsFalse(camera.GroundPoint(0.0f, 1.0f, WIDE_SCREEN).has_value());
    Assert::IsFalse(camera.GroundPointAtPixel(960.0f, 0.0f, VIEWPORT).has_value());
    Assert::IsTrue(camera.GroundPoint(0.0f, -1.0f, WIDE_SCREEN).has_value());

    // The focus is the origin, so twice as far again from it as the eye stands is behind the eye, on the ground.
    const DirectX::XMFLOAT3 eye = camera.EyePosition(VIEWPORT.AspectRatio());
    const Outpost::PlanePosition behind{.xMeters = 3.0f * eye.x, .zMeters = 3.0f * eye.z};
    Assert::IsFalse(camera.PixelOf(behind, VIEWPORT).has_value());
    Assert::IsFalse(camera.MetersPerPixelAt(behind, VIEWPORT).has_value());
    Assert::IsTrue(camera.PixelOf({}, VIEWPORT).has_value());

    Assert::IsFalse(camera.GroundPointAtPixel(0.0f, 0.0f, {.widthPixels = 0, .heightPixels = 1080}).has_value());
    Assert::IsFalse(camera.GroundPointAtPixel(0.0f, 0.0f, {.widthPixels = 1920, .heightPixels = 0}).has_value());
    Assert::IsFalse(camera.MetersPerPixelAt({}, {.widthPixels = 1920, .heightPixels = 0}).has_value());
  }
};
} // namespace GameAppTests