#include "pch.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
/// The target device (`Interface.md` section 1): a 2880 x 1920 panel reported as 1440 x 960
/// device-independent pixels at 200%.
inline constexpr std::int32_t PANEL_WIDTH = 2880;
inline constexpr std::int32_t PANEL_HEIGHT = 1920;
inline constexpr float PANEL_RAW_PIXELS_PER_VIEW_PIXEL = 2.0f;

[[nodiscard]] Neuron::AuthoredSpace PanelSpace() noexcept
{
  return Neuron::AuthoredSpace{.interfaceFit = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT),
                               .rawPixelsPerViewPixel = PANEL_RAW_PIXELS_PER_VIEW_PIXEL};
}

/// One manipulation update, in authored pixels and degrees. The seam is what turns a `PointerPoint`
/// into one of these, and nothing in this file has ever seen one.
[[nodiscard]] Neuron::InputEvent Update(float _translationX, float _translationY, float _scale, float _rotationDegrees,
                                        std::uint32_t _contactCount = 2) noexcept
{
  return Neuron::InputEvent{.kind = Neuron::InputEventKind::ManipulationUpdated,
                            .contactCount = _contactCount,
                            .translationXAuthoredPixels = _translationX,
                            .translationYAuthoredPixels = _translationY,
                            .scale = _scale,
                            .rotationDegrees = _rotationDegrees};
}

[[nodiscard]] Neuron::InputEvent Started(std::uint32_t _contactCount) noexcept
{
  return Neuron::InputEvent{.kind = Neuron::InputEventKind::ManipulationStarted, .contactCount = _contactCount};
}
} // namespace

TEST_CLASS(TheGestureConstants)
{
public:
  TEST_METHOD(TheyAreTheOnesInterfaceStates)
  {
    // `Interface.md` section 1's table and section 5's two deadzones, asserted so that moving one
    // is a decision somebody took rather than a line that drifted. R21 requires the arithmetic
    // under a gesture to have a suite over it, and a number nobody wrote down is a number no suite
    // can pin.
    Assert::AreEqual(16.0f, Neuron::TAP_SLOP_AUTHORED_PIXELS);
    Assert::AreEqual(78.0f, Neuron::CONTACT_REJECTION_AUTHORED_PIXELS);
    Assert::AreEqual(8.0f, Neuron::ROTATION_DEADZONE_DEGREES);
    Assert::AreEqual(0.02f, Neuron::SCALE_DEADZONE_FRACTION);
  }

  TEST_METHOD(OnlyTouchGetsIn)
  {
    // R21, and the whole of it: mouse and pen have no compatibility path. `GestureSeam.cpp`
    // static_asserts that this integer is still the projection's `PointerDeviceType::Touch`, at
    // the single site that performs the drop -- which is the half of this rule a desktop suite
    // cannot reach.
    Assert::IsTrue(Neuron::IsTouchPointer(Neuron::POINTER_DEVICE_TYPE_TOUCH));
    Assert::IsFalse(Neuron::IsTouchPointer(1), L"pen");
    Assert::IsFalse(Neuron::IsTouchPointer(2), L"mouse");
  }

  TEST_METHOD(APalmIsNotAFingertip)
  {
    // `Interface.md` section 2: EXCEEDING 78 authored pixels in either dimension is a palm, so a
    // contact exactly 78 across is still a finger. Either dimension, not both -- a hand landing on
    // its edge is long and narrow.
    Assert::IsTrue(Neuron::IsFingertipContact(40.0f, 52.0f), L"an ordinary fingertip");
    Assert::IsTrue(Neuron::IsFingertipContact(78.0f, 78.0f), L"exactly at the threshold is still a finger");
    Assert::IsFalse(Neuron::IsFingertipContact(78.5f, 20.0f), L"wide and short is a palm edge");
    Assert::IsFalse(Neuron::IsFingertipContact(20.0f, 78.5f), L"tall and narrow is too");
  }

  TEST_METHOD(AContactThatReportsNoSizeIsAFingertip)
  {
    // A digitizer that gives an empty ContactRect must not make the game unplayable, which is what
    // treating "unknown" as a palm would do.
    Assert::IsTrue(Neuron::IsFingertipContact(0.0f, 0.0f));
  }
};

TEST_CLASS(TheAuthoredConversion)
{
public:
  TEST_METHOD(ADisplayScaleThatIsIgnoredHalvesEveryTap)
  {
    // THE TRAP R18 NAMES, from the input side. A CoreWindow reports a POSITION IN
    // DEVICE-INDEPENDENT PIXELS; everything downstream of the swap chain is physical. Drop the
    // conversion on a 200% display and every tap lands at half the distance from the top left
    // corner -- and nothing fails, which is exactly why this is asserted rather than trusted.
    const Neuron::AuthoredPoint correct = Neuron::AuthoredFromDips(PanelSpace(), 720.0f, 480.0f);
    Assert::AreEqual(720.0f, correct.xPixels, 0.01f);
    Assert::AreEqual(480.0f, correct.yPixels, 0.01f);

    const Neuron::AuthoredSpace withoutTheScale{.interfaceFit = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT),
                                                .rawPixelsPerViewPixel = 1.0f};
    const Neuron::AuthoredPoint wrong = Neuron::AuthoredFromDips(withoutTheScale, 720.0f, 480.0f);
    Assert::AreEqual(360.0f, wrong.xPixels, 0.01f);
    Assert::AreEqual(240.0f, wrong.yPixels, 0.01f);
  }

  TEST_METHOD(OnTheTargetDeviceAuthoredAndDipsCoincideAndThatIsACoincidence)
  {
    // 1440 authored across a panel reported as 1440 DIPs, so the two numbers agree ON THIS DEVICE
    // and only here. ADR-007 corrected a version of this design that called that correspondence
    // "not a coincidence"; dressing a free choice as a natural consequence invites somebody to
    // "fix" the conversion away, and the test below is what would catch them.
    const Neuron::AuthoredPoint corner = Neuron::AuthoredFromDips(PanelSpace(), 1440.0f, 960.0f);
    Assert::AreEqual(1440.0f, corner.xPixels, 0.01f);
    Assert::AreEqual(960.0f, corner.yPixels, 0.01f);
  }

  TEST_METHOD(APillarboxedWindowPutsTheFramesCornersWhereTheyBelong)
  {
    // The development window: a 3:2 interface in a 16:9 back buffer leaves 150 pixels of bar down
    // each side, so authored 0,0 is at physical 150,0 rather than at the window's corner. A tap in
    // the bar is outside the authored frame and comes back negative, which is the honest answer.
    const Neuron::AuthoredSpace space{.interfaceFit = Neuron::ComputeInterfaceFit(1920, 1080), .rawPixelsPerViewPixel = 1.0f};
    const Neuron::AuthoredPoint topLeft = Neuron::AuthoredFromDips(space, 150.0f, 0.0f);
    Assert::AreEqual(0.0f, topLeft.xPixels, 0.01f);
    Assert::AreEqual(0.0f, topLeft.yPixels, 0.01f);

    const Neuron::AuthoredPoint bottomRight = Neuron::AuthoredFromDips(space, 1770.0f, 1080.0f);
    Assert::AreEqual(1440.0f, bottomRight.xPixels, 0.01f);
    Assert::AreEqual(960.0f, bottomRight.yPixels, 0.01f);

    const Neuron::AuthoredPoint inTheBar = Neuron::AuthoredFromDips(space, 20.0f, 540.0f);
    Assert::IsTrue(inTheBar.xPixels < 0.0f, L"a tap in the letterbox bar is outside the frame");
  }

  TEST_METHOD(ItIsTheInverseOfWhatM0Point17Draws)
  {
    // The two halves of one transform, composed: M0.17 places an authored rectangle on the glass
    // and this takes a point on the glass back to authored coordinates. If they ever disagree, a
    // player taps a button and misses it by the difference.
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT);
    const Neuron::AuthoredRect target{.left = 16, .top = 896, .right = 64, .bottom = 944};
    const Neuron::PhysicalRect placed = Neuron::MapAuthoredRect(fit, target);

    const Neuron::AuthoredPoint back = Neuron::AuthoredFromPhysical(fit, static_cast<float>(placed.left), static_cast<float>(placed.top));
    Assert::AreEqual(static_cast<float>(target.left), back.xPixels, 0.01f);
    Assert::AreEqual(static_cast<float>(target.top), back.yPixels, 0.01f);

    // And a length, which is what palm rejection compares: 48 authored is 96 physical either way.
    Assert::AreEqual(48.0f, Neuron::AuthoredLengthFromPhysical(fit, 96.0f), 0.01f);
  }

  TEST_METHOD(ADegenerateFitIsZeroRatherThanInfinite)
  {
    const Neuron::AuthoredSpace space{.interfaceFit = Neuron::ComputeInterfaceFit(0, 0), .rawPixelsPerViewPixel = 2.0f};
    const Neuron::AuthoredPoint at = Neuron::AuthoredFromDips(space, 100.0f, 100.0f);
    Assert::AreEqual(0.0f, at.xPixels);
    Assert::AreEqual(0.0f, at.yPixels);
    Assert::AreEqual(0.0f, Neuron::AuthoredLengthFromDips(space, 40.0f));
  }
};

TEST_CLASS(TheTapSlop)
{
public:
  TEST_METHOD(ItIsPinnedEitherSideOfSixteen)
  {
    // `Interface.md` section 3: the travel that separates a tap from a pan, and the asymmetry the
    // whole design rests on -- an accidental pan is free, an accidental move order costs a fleet.
    // EXCEED, so a travel of exactly 16 is still a tap.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(1)));

    Assert::IsFalse(Neuron::ApplyGate(gate, Update(15.9f, 0.0f, 1.0f, 0.0f, 1)).isPanning, L"under the slop");
    Assert::IsFalse(Neuron::ApplyGate(gate, Update(16.0f, 0.0f, 1.0f, 0.0f, 1)).isPanning, L"exactly at it");
    Assert::IsTrue(Neuron::ApplyGate(gate, Update(16.1f, 0.0f, 1.0f, 0.0f, 1)).isPanning, L"past it");
  }

  TEST_METHOD(ItIsARadiusRatherThanTwoAxes)
  {
    // A diagonal drag of 12 by 12 is 16.97 of travel, so it pans. Comparing each axis separately
    // would let a finger travel 22 pixels diagonally and still call it a tap.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(1)));
    Assert::IsTrue(Neuron::ApplyGate(gate, Update(12.0f, 12.0f, 1.0f, 0.0f, 1)).isPanning);
  }

  TEST_METHOD(ThePanBeginsAtTheCrossingAndNotAtThePress)
  {
    // THE HALF OF THIS RULE THAT IS EASY TO LEAVE OUT, and the one `Interface.md` section 3 states
    // explicitly: the pan begins at the point the threshold was crossed, so nothing jumps when it
    // engages. Without it the camera lurches by the whole slop the instant the finger passes it.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(1)));

    const Neuron::GatedManipulation crossing = Neuron::ApplyGate(gate, Update(20.0f, 0.0f, 1.0f, 0.0f, 1));
    Assert::IsTrue(crossing.isPanning);
    Assert::AreEqual(0.0f, crossing.panXAuthoredPixels, 0.001f, L"the update that engages the pan moves nothing");
    Assert::AreEqual(0.0f, crossing.panYAuthoredPixels, 0.001f);

    const Neuron::GatedManipulation after = Neuron::ApplyGate(gate, Update(25.0f, 3.0f, 1.0f, 0.0f, 1));
    Assert::AreEqual(5.0f, after.panXAuthoredPixels, 0.001f);
    Assert::AreEqual(3.0f, after.panYAuthoredPixels, 0.001f);
  }

  TEST_METHOD(AnEngagedPanNeverBecomesATapAgain)
  {
    // A finger that wanders out past the slop and comes back is panning, not tapping. The
    // alternative is a move order issued by a drag that happened to end where it started.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(1)));
    static_cast<void>(Neuron::ApplyGate(gate, Update(40.0f, 0.0f, 1.0f, 0.0f, 1)));

    const Neuron::GatedManipulation back = Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.0f, 0.0f, 1));
    Assert::IsTrue(back.isPanning);
    Assert::AreEqual(-40.0f, back.panXAuthoredPixels, 0.001f);
  }
};

TEST_CLASS(TheCameraDeadzones)
{
public:
  TEST_METHOD(RotationIsPinnedEitherSideOfEightDegrees)
  {
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(2)));

    Assert::AreEqual(0.0f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.0f, 7.9f)).rotationDegrees, 0.001f);
    Assert::AreEqual(0.0f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.0f, 8.0f)).rotationDegrees, 0.001f);
    Assert::AreEqual(8.1f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.0f, 8.1f)).rotationDegrees, 0.001f);
  }

  TEST_METHOD(TheRotationLatchHoldsWhenTheFingersCrossBack)
  {
    // `Interface.md` section 5: once it engages it stays engaged for the rest of that manipulation.
    // WITHOUT THE LATCH THE CAMERA STUTTERS every time the player crosses back under the
    // threshold mid-gesture, which is worse than having no deadzone at all -- and it is the
    // failure a plain comparison produces, which is why this is a separate test from the one above.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(2)));
    static_cast<void>(Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.0f, 12.0f)));

    Assert::AreEqual(3.0f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.0f, 3.0f)).rotationDegrees, 0.001f);
    Assert::AreEqual(-1.0f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.0f, -1.0f)).rotationDegrees, 0.001f);

    // And the latch is the manipulation's, not the seam's: the next one starts closed.
    Neuron::ManipulationGate second{};
    static_cast<void>(Neuron::ApplyGate(second, Started(2)));
    Assert::AreEqual(0.0f, Neuron::ApplyGate(second, Update(0.0f, 0.0f, 1.0f, 3.0f)).rotationDegrees, 0.001f);
  }

  TEST_METHOD(TheSignOfARotationSurvivesTheDeadzone)
  {
    // R21 names this as a thing a package can hide and a test cannot. The recognizer reports
    // degrees with CLOCKWISE POSITIVE, and the gate passes that through rather than converting it:
    // a camera that orbits the wrong way is a one-character defect nobody can see in a diff.
    Neuron::ManipulationGate clockwise{};
    static_cast<void>(Neuron::ApplyGate(clockwise, Started(2)));
    Assert::IsTrue(Neuron::ApplyGate(clockwise, Update(0.0f, 0.0f, 1.0f, 30.0f)).rotationDegrees > 0.0f);

    Neuron::ManipulationGate counterClockwise{};
    static_cast<void>(Neuron::ApplyGate(counterClockwise, Started(2)));
    Assert::IsTrue(Neuron::ApplyGate(counterClockwise, Update(0.0f, 0.0f, 1.0f, -30.0f)).rotationDegrees < 0.0f);
  }

  TEST_METHOD(TwoPerCentOfScaleIsIgnored)
  {
    // The mirror-image reason: fingers that rotate also change separation slightly, and pitch is
    // coupled to zoom, so without this a pure orbit silently re-pitches the camera.
    //
    // EITHER SIDE OF TWO PER CENT RATHER THAN ON IT. The constant itself is pinned exactly above;
    // a scale of exactly 1.02f is a knife edge in binary -- the nearest float to it is 1.0199999809
    // and therefore just inside -- and a test that turns on which way that rounds pins the
    // floating-point representation rather than the rule.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(2)));

    Assert::AreEqual(1.0f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.01f, 0.0f)).scale, 0.0001f);
    Assert::AreEqual(1.0f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 0.99f, 0.0f)).scale, 0.0001f);
    Assert::AreEqual(1.03f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.03f, 0.0f)).scale, 0.0001f);
    Assert::AreEqual(0.97f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 0.97f, 0.0f)).scale, 0.0001f);
  }

  TEST_METHOD(TheSignOfAPinchSurvivesTheDeadzone)
  {
    // The other half of what R21 names. A scale BELOW one is fingers coming together; ABOVE one is
    // fingers moving apart. The gate reports the recognizer's multiplier unchanged, and what that
    // does to the camera's distance is ADR-018's solve at M1.8.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(2)));
    Assert::IsTrue(Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 0.8f, 0.0f)).scale < 1.0f, L"a pinch");
    Assert::IsTrue(Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.25f, 0.0f)).scale > 1.0f, L"a spread");
  }

  TEST_METHOD(TheScaleDeadzoneDoesNotLatch)
  {
    // And that asymmetry against rotation is deliberate: ADR-018 counts three constants here -- the
    // eight degrees, its latch, and this two per cent -- and gives zoom no fourth.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(2)));
    static_cast<void>(Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.25f, 0.0f)));
    Assert::AreEqual(1.0f, Neuron::ApplyGate(gate, Update(0.0f, 0.0f, 1.01f, 0.0f)).scale, 0.0001f);
  }
};

TEST_CLASS(TheContactCountLatch)
{
public:
  TEST_METHOD(AThirdFingerDoesNotChangeWhatTheDragIsDoing)
  {
    // `Interface.md` section 2, and the reason it is a rule: a manipulation means two different
    // things depending on how many fingers began it, and a thumb brushing the glass mid-drag must
    // not turn a pan into an orbit.
    Neuron::ManipulationGate gate{};
    Assert::AreEqual(2u, Neuron::ApplyGate(gate, Started(2)).contactCount);
    Assert::AreEqual(2u, Neuron::ApplyGate(gate, Update(10.0f, 0.0f, 1.0f, 0.0f, 3)).contactCount);

    const Neuron::InputEvent completed{
      .kind = Neuron::InputEventKind::ManipulationCompleted, .contactCount = 1, .translationXAuthoredPixels = 10.0f};
    Assert::AreEqual(2u, Neuron::ApplyGate(gate, completed).contactCount);
  }

  TEST_METHOD(EachManipulationLatchesItsOwn)
  {
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(2)));
    static_cast<void>(
      Neuron::ApplyGate(gate, Neuron::InputEvent{.kind = Neuron::InputEventKind::ManipulationCompleted, .contactCount = 2}));
    Assert::AreEqual(1u, Neuron::ApplyGate(gate, Started(1)).contactCount);
  }

  TEST_METHOD(AnUpdateOutsideAManipulationIsInert)
  {
    // A dropped or reordered event must not start a manipulation halfway through, which would
    // latch whatever count happened to be on it and pan from an origin nobody chose.
    Neuron::ManipulationGate gate{};
    const Neuron::GatedManipulation orphan = Neuron::ApplyGate(gate, Update(400.0f, 0.0f, 2.0f, 45.0f));
    Assert::IsFalse(orphan.isPanning);
    Assert::AreEqual(0u, orphan.contactCount);
    Assert::AreEqual(1.0f, orphan.scale, 0.0001f);
    Assert::AreEqual(0.0f, orphan.rotationDegrees, 0.0001f);
  }

  TEST_METHOD(ATapAndAHoldCarryNoManipulation)
  {
    // A tap's meaning is what is under it (`Interface.md` section 4) and a hold's is a recenter
    // (ADR-018). Neither is the camera's, and neither reaches this function for anything.
    Neuron::ManipulationGate gate{};
    static_cast<void>(Neuron::ApplyGate(gate, Started(1)));
    static_cast<void>(Neuron::ApplyGate(gate, Update(40.0f, 0.0f, 1.0f, 0.0f, 1)));

    for (const Neuron::InputEventKind kind : {Neuron::InputEventKind::Tapped, Neuron::InputEventKind::Holding})
    {
      const Neuron::GatedManipulation gated =
        Neuron::ApplyGate(gate, Neuron::InputEvent{.kind = kind, .contactCount = 1, .tapCount = 2, .xAuthoredPixels = 400.0f});
      Assert::IsFalse(gated.isPanning);
      Assert::AreEqual(0u, gated.contactCount);
    }

    // And the manipulation they interleaved with is untouched: the gate is still engaged.
    Assert::IsTrue(Neuron::ApplyGate(gate, Update(45.0f, 0.0f, 1.0f, 0.0f, 1)).isPanning);
  }
};

} // namespace NeuronClientTests
