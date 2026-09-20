#include "pch.h"

#include "CapturePath.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Where the capture's camera looks (m1-vertical-slice/G2). Ninety BMPs of a scripted match are the
// only look anybody gets at this game, and a camera pointed four hundred units off shows an empty
// hillside while the match is decided behind it - which reads exactly like a game where nothing
// happens. So the script, the pose it gives and the fighting it follows are asserted here rather
// than looked at afterwards.
namespace ClientTests
{

namespace
{

constexpr float TOLERANCE = 0.01f;

void AssertNear(float _expected, float _actual, const wchar_t* _what)
{
  Assert::IsTrue(std::fabs(_expected - _actual) < TOLERANCE, _what);
}

/// Two vantages a thousand ticks apart, which is enough to say everything about the interpolation.
[[nodiscard]] std::array<Neuron::CaptureVantage, 2> Pair()
{
  Neuron::CaptureVantage first{};
  first.tick = 1000;
  first.aimX = 100.0f;
  first.aimZ = 200.0f;
  first.bearingRadians = 0.0f;
  first.setbackWorldUnits = 400.0f;
  first.elevationWorldUnits = 300.0f;
  first.follow = false;
  first.fog = Neuron::FogMode::LinearToColor;

  Neuron::CaptureVantage second{};
  second.tick = 2000;
  second.aimX = 1100.0f;
  second.aimZ = 700.0f;
  second.bearingRadians = 2.0f;
  second.setbackWorldUnits = 800.0f;
  second.elevationWorldUnits = 500.0f;
  second.follow = true;
  second.fog = Neuron::FogMode::Desaturation;
  return {first, second};
}

/// One projectile in the air at a point, and the flags that say it is one.
[[nodiscard]] Neuron::RenderInstance Projectile(float _x, float _z)
{
  Neuron::RenderInstance instance{};
  instance.x = _x;
  instance.y = 50.0f;
  instance.z = _z;
  instance.kind = Neuron::RenderInstanceKind::Projectile;
  return instance;
}

[[nodiscard]] Neuron::RenderInstance Device(float _x, float _z)
{
  Neuron::RenderInstance instance{};
  instance.x = _x;
  instance.z = _z;
  instance.kind = Neuron::RenderInstanceKind::Object;
  return instance;
}

} // namespace

TEST_CLASS(CapturePathTests)
{
public:
  TEST_METHOD(AnEmptyScriptGivesAVantageThatIsNotAFault)
  {
    // A capture whose script is empty is a capture nobody wrote a script for, and the answer is a
    // pose at the origin rather than a read past the end of a span.
    const Neuron::CaptureVantage vantage = Neuron::VantageAt({}, 4321);
    AssertNear(0.0f, vantage.aimX, L"aimX");
    AssertNear(0.0f, vantage.aimZ, L"aimZ");
    AssertNear(0.0f, vantage.setbackWorldUnits, L"setback");
  }

  TEST_METHOD(ATickBeforeTheScriptStartsHoldsTheOpeningVantage)
  {
    // NOT AN EXTRAPOLATION BACKWARDS. The first vantage is where the capture opens; a tick before
    // it is a tick the script has not started, and the camera is already in its opening pose.
    const std::array<Neuron::CaptureVantage, 2> script = Pair();
    const Neuron::CaptureVantage vantage = Neuron::VantageAt(script, 0);
    AssertNear(100.0f, vantage.aimX, L"aimX");
    AssertNear(200.0f, vantage.aimZ, L"aimZ");
    AssertNear(400.0f, vantage.setbackWorldUnits, L"setback");
    Assert::IsTrue(vantage.fog == Neuron::FogMode::LinearToColor, L"the opening fog");
  }

  TEST_METHOD(AVantageIsExactOnItsOwnTick)
  {
    const std::array<Neuron::CaptureVantage, 2> script = Pair();
    const Neuron::CaptureVantage first = Neuron::VantageAt(script, 1000);
    AssertNear(100.0f, first.aimX, L"the first vantage's aimX");
    AssertNear(300.0f, first.elevationWorldUnits, L"the first vantage's elevation");
    const Neuron::CaptureVantage second = Neuron::VantageAt(script, 2000);
    AssertNear(1100.0f, second.aimX, L"the second vantage's aimX");
    AssertNear(500.0f, second.elevationWorldUnits, L"the second vantage's elevation");
  }

  TEST_METHOD(BetweenTwoVantagesTheCameraTravelsRatherThanCuts)
  {
    // Half way between the two, in every field that is a number. A capture that cut from one
    // vantage to the next would be ninety stills of four places and nothing in between; this is
    // what makes the frames a pass over the match.
    const std::array<Neuron::CaptureVantage, 2> script = Pair();
    const Neuron::CaptureVantage middle = Neuron::VantageAt(script, 1500);
    AssertNear(600.0f, middle.aimX, L"aimX");
    AssertNear(450.0f, middle.aimZ, L"aimZ");
    AssertNear(1.0f, middle.bearingRadians, L"bearing");
    AssertNear(600.0f, middle.setbackWorldUnits, L"setback");
    AssertNear(400.0f, middle.elevationWorldUnits, L"elevation");

    // A quarter of the way, so that the fraction is the tick's and not a half hard-coded.
    const Neuron::CaptureVantage quarter = Neuron::VantageAt(script, 1250);
    AssertNear(350.0f, quarter.aimX, L"a quarter's aimX");
    AssertNear(325.0f, quarter.aimZ, L"a quarter's aimZ");
  }

  TEST_METHOD(TheFogAndTheFollowAreTheVantagesOwnAndNeverHalfWay)
  {
    // A frame is fogged one way or the other and a vantage either looks at the fighting or does
    // not, so both are held from the vantage in force until the next one arrives. ADR-005 rests
    // on a pair of frames under the two modes; a fog that faded between them would compare
    // nothing.
    const std::array<Neuron::CaptureVantage, 2> script = Pair();
    const Neuron::CaptureVantage middle = Neuron::VantageAt(script, 1999);
    Assert::IsTrue(middle.fog == Neuron::FogMode::LinearToColor, L"still the first vantage's fog");
    Assert::IsFalse(middle.follow, L"still the first vantage's following");
    const Neuron::CaptureVantage after = Neuron::VantageAt(script, 2000);
    Assert::IsTrue(after.fog == Neuron::FogMode::Desaturation, L"the second vantage's fog");
    Assert::IsTrue(after.follow, L"the second vantage's following");
  }

  TEST_METHOD(PastTheLastVantageTheCameraHoldsItRatherThanFlyingOn)
  {
    // A capture is run for a tick count the script does not have to end exactly on, and a camera
    // that kept travelling past its last vantage would leave the landscape somewhere after tick
    // nine thousand.
    const std::array<Neuron::CaptureVantage, 2> script = Pair();
    const Neuron::CaptureVantage beyond = Neuron::VantageAt(script, 100000);
    AssertNear(1100.0f, beyond.aimX, L"aimX");
    AssertNear(700.0f, beyond.aimZ, L"aimZ");
    AssertNear(800.0f, beyond.setbackWorldUnits, L"setback");
  }

  TEST_METHOD(AScriptOfOneVantageIsThatVantageAtEveryTick)
  {
    const std::array<Neuron::CaptureVantage, 1> script = {Pair()[0]};
    for (const std::uint32_t tick : {0u, 1000u, 9000u})
    {
      const Neuron::CaptureVantage vantage = Neuron::VantageAt(script, tick);
      AssertNear(100.0f, vantage.aimX, L"aimX");
      AssertNear(400.0f, vantage.setbackWorldUnits, L"setback");
    }
  }

  TEST_METHOD(TheEyeSitsRoundTheAimPointAtTheBearingTheScriptAsksFor)
  {
    // Bearing zero is due -Z of the aim, which is the camera looking toward +Z; a quarter turn is
    // due +X of it. The whole of what a vantage means is here, so a capture that framed everything
    // from behind would be this assertion failing rather than ninety frames of the wrong side.
    Neuron::CaptureVantage vantage{};
    vantage.aimX = 1000.0f;
    vantage.aimZ = 2000.0f;
    vantage.setbackWorldUnits = 500.0f;
    vantage.elevationWorldUnits = 300.0f;

    const Neuron::CapturePose south = Neuron::PoseOf(vantage, 40.0f);
    AssertNear(1000.0f, south.eyeX, L"the eye's x due south");
    AssertNear(1500.0f, south.eyeZ, L"the eye's z due south");
    AssertNear(340.0f, south.eyeY, L"the eye's height above the ground");
    AssertNear(1000.0f, south.atX, L"what it looks at, x");
    AssertNear(40.0f, south.atY, L"what it looks at is ON the ground");
    AssertNear(2000.0f, south.atZ, L"what it looks at, z");

    vantage.bearingRadians = 1.57079633f; // a quarter turn
    const Neuron::CapturePose east = Neuron::PoseOf(vantage, 40.0f);
    AssertNear(1500.0f, east.eyeX, L"the eye's x a quarter turn round");
    AssertNear(2000.0f, east.eyeZ, L"the eye's z a quarter turn round");
  }

  TEST_METHOD(TheAimRidesTheGroundRatherThanASeaLevel)
  {
    // The same vantage on a ridge and on a beach: the eye keeps its elevation ABOVE the ground and
    // the aim stays on it. A capture that took a fixed height would be underground on a peak and
    // in orbit over a shore, which is the fault Match's own opening pose already avoids.
    Neuron::CaptureVantage vantage{};
    vantage.elevationWorldUnits = 300.0f;
    const Neuron::CapturePose beach = Neuron::PoseOf(vantage, 0.0f);
    const Neuron::CapturePose ridge = Neuron::PoseOf(vantage, 900.0f);
    AssertNear(300.0f, beach.eyeY, L"over the beach");
    AssertNear(1200.0f, ridge.eyeY, L"over the ridge");
    AssertNear(0.0f, beach.atY, L"the beach's ground");
    AssertNear(900.0f, ridge.atY, L"the ridge's ground");
  }

  TEST_METHOD(NothingInTheAirIsAnsweredWithNoAndNotWithTheOrigin)
  {
    // The difference matters: a capture told the fighting is at (0, 0) points its camera off the
    // corner of the landscape, and every frame of the match is sea.
    const std::array<Neuron::RenderInstance, 2> quiet = {Device(500.0f, 500.0f), Device(600.0f, 600.0f)};
    float x = -1.0f;
    float z = -1.0f;
    Assert::IsFalse(Neuron::ActionCenter(quiet, x, z), L"nothing is in the air");
    AssertNear(-1.0f, x, L"the outputs are untouched");
    AssertNear(-1.0f, z, L"the outputs are untouched");
    Assert::IsFalse(Neuron::ActionCenter({}, x, z), L"an empty view");
  }

  TEST_METHOD(TheActionIsWhereTheShotsAreAndTheRestOfTheViewIsNotCounted)
  {
    // The fighting is two shots at one end of the landscape; the commander's own base, a dozen
    // devices and a wreck are at the other. A mean over everything drawn would point the camera
    // between the two and show neither.
    std::vector<Neuron::RenderInstance> view;
    view.push_back(Device(0.0f, 0.0f));
    view.push_back(Device(100.0f, 100.0f));
    Neuron::RenderInstance wreck{};
    wreck.x = 200.0f;
    wreck.z = 200.0f;
    wreck.kind = Neuron::RenderInstanceKind::Wreck;
    view.push_back(wreck);
    view.push_back(Projectile(3000.0f, 5000.0f));
    view.push_back(Projectile(3200.0f, 5200.0f));

    float x = 0.0f;
    float z = 0.0f;
    Assert::IsTrue(Neuron::ActionCenter(view, x, z), L"two shots are in the air");
    AssertNear(3100.0f, x, L"the middle of the fighting, x");
    AssertNear(5100.0f, z, L"the middle of the fighting, z");
  }

  TEST_METHOD(OneShotIsEnoughToLookAt)
  {
    const std::array<Neuron::RenderInstance, 2> view = {Device(0.0f, 0.0f), Projectile(1234.0f, 5678.0f)};
    float x = 0.0f;
    float z = 0.0f;
    Assert::IsTrue(Neuron::ActionCenter(view, x, z), L"one shot is in the air");
    AssertNear(1234.0f, x, L"x");
    AssertNear(5678.0f, z, L"z");
  }
};

} // namespace ClientTests
