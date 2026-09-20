#include "pch.h"

#include "Selection.h"

#include <array>
#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Design/Interface.md §5 as cases. The camera is the orthographic one Tests/ReplicaTests's picking
// suite uses, copied rather than shared because a fixture two suites edit is a fixture neither
// owns; what matters here is only that a world point has a screen point nobody has to guess at.

namespace ReplicaTests
{

namespace
{

/// world (x, y, z) -> clip (x/100, y/100, z/200 + 1/2), and the depth is reversed (ADR-005). So a
/// world point at (0, 0, 0) is at screen (100, 100), and one world unit is one screen pixel: screen
/// x is world x plus 100, and screen y is 100 MINUS world y, because the screen counts downward.
[[nodiscard]] Outpost::PickCamera Camera()
{
  Outpost::PickCamera camera{};
  camera.frameWidth = 200;
  camera.frameHeight = 200;
  camera.viewProjection.m = {0.01f, 0.0f,  0.0f,   0.0f, //
                             0.0f,  0.01f, 0.0f,   0.0f, //
                             0.0f,  0.0f,  0.005f, 0.0f, //
                             0.0f,  0.0f,  0.5f,   1.0f};
  camera.inverseViewProjection.m = {100.0f, 0.0f,   0.0f,    0.0f, //
                                    0.0f,   100.0f, 0.0f,    0.0f, //
                                    0.0f,   0.0f,   200.0f,  0.0f, //
                                    0.0f,   0.0f,   -100.0f, 1.0f};
  return camera;
}

[[nodiscard]] Outpost::PickCandidate At(std::uint32_t _id, std::uint8_t _seat, Outpost::ObjectKind _kind, float _x, float _y,
                                        float _radius = 4.0f)
{
  Outpost::PickCandidate candidate{};
  candidate.id = _id;
  candidate.seat = _seat;
  candidate.kind = _kind;
  candidate.x = _x;
  candidate.y = _y;
  candidate.z = 0.0f;
  candidate.radius = _radius;
  return candidate;
}

[[nodiscard]] Outpost::PickCandidate Device(std::uint32_t _id, std::uint8_t _seat, float _x, float _y, float _radius = 4.0f)
{
  return At(_id, _seat, Outpost::ObjectKind::Device, _x, _y, _radius);
}

[[nodiscard]] Outpost::PickCandidate Structure(std::uint32_t _id, std::uint8_t _seat, float _x, float _y)
{
  return At(_id, _seat, Outpost::ObjectKind::Structure, _x, _y);
}

/// The whole gesture in one call: press at one point, move to another, release. `_travel` false
/// releases where it pressed, which is a click.
struct Gesture
{
  Outpost::Selection selection;
  std::vector<Outpost::PickCandidate> candidates;
  std::vector<Outpost::PickBox> blocked;
  std::uint8_t seat = 0;

  [[nodiscard]] Outpost::SelectionFrame Frame(std::int32_t _x, std::int32_t _y, bool _pressed, bool _released, bool _additive) const
  {
    Outpost::SelectionFrame frame{};
    frame.camera = Camera();
    frame.candidates = candidates;
    frame.blocked = blocked;
    frame.ownSeat = seat;
    frame.x = _x;
    frame.y = _y;
    frame.pressed = _pressed;
    frame.released = _released;
    frame.additive = _additive;
    return frame;
  }

  void Click(std::int32_t _x, std::int32_t _y, bool _additive = false)
  {
    selection.Advance(Frame(_x, _y, true, false, _additive));
    selection.Advance(Frame(_x, _y, false, true, _additive));
  }

  void Drag(std::int32_t _x0, std::int32_t _y0, std::int32_t _x1, std::int32_t _y1, bool _additive = false)
  {
    selection.Advance(Frame(_x0, _y0, true, false, _additive));
    selection.Advance(Frame(_x1, _y1, false, false, _additive));
    selection.Advance(Frame(_x1, _y1, false, true, _additive));
  }

  [[nodiscard]] std::vector<std::uint32_t> Ids() const
  {
    const std::span<const std::uint32_t> ids = selection.Ids();
    return {ids.begin(), ids.end()};
  }
};

void AssertIds(const std::vector<std::uint32_t>& _expected, const std::vector<std::uint32_t>& _actual, const wchar_t* _message)
{
  Assert::AreEqual(_expected.size(), _actual.size(), _message);
  for (std::size_t index = 0; index < _expected.size() && index < _actual.size(); ++index)
  {
    Assert::AreEqual(_expected[index], _actual[index], _message);
  }
}

} // namespace

TEST_CLASS(SelectionTests)
{
public:
  TEST_METHOD(AClickTakesTheThingUnderIt)
  {
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f)};
    gesture.Click(100, 100);
    AssertIds({7}, gesture.Ids(), L"the device under the cursor is selected");
  }

  TEST_METHOD(AClickOnNothingClearsTheSelection)
  {
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f)};
    gesture.Click(100, 100);
    gesture.Click(190, 190);
    AssertIds({}, gesture.Ids(), L"a click on open ground selects nothing and keeps nothing");
  }

  /// A commander who missed while adding to a group meant to add nothing, not to lose the group.
  TEST_METHOD(AnEmptyClickWithShiftKeepsWhatWasSelected)
  {
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f)};
    gesture.Click(100, 100);
    gesture.Click(190, 190, true);
    AssertIds({7}, gesture.Ids(), L"Shift over open ground is not a clear");
  }

  /// §5: "a rectangle never selects structures and never selects another commander's anything".
  TEST_METHOD(ARectangleTakesThisCommandersDevicesAndNothingElse)
  {
    Gesture gesture;
    gesture.candidates = {Device(1, 0, -20.0f, 20.0f), Device(2, 0, 20.0f, -20.0f), Device(3, 1, 0.0f, 0.0f),
                          Structure(4, 0, 10.0f, 10.0f)};
    gesture.Drag(50, 50, 150, 150);
    AssertIds({1, 2}, gesture.Ids(), L"both of this seat's devices, neither the enemy's nor the structure");
  }

  /// §5 again, from the other side: a CLICK may take an enemy, because inspection is a real thing a
  /// commander does and refusing it would leave him unable to look at what is shooting at him.
  TEST_METHOD(AClickMayTakeAnEnemyAsAnInspection)
  {
    Gesture gesture;
    gesture.candidates = {Device(3, 1, 0.0f, 0.0f)};
    gesture.Click(100, 100);
    AssertIds({3}, gesture.Ids(), L"a visible enemy is selected by a click");
  }

  /// A hand resting on a mouse moves a pixel or two while it clicks. Under the threshold the
  /// gesture is the click the commander meant and not a rectangle that takes something else.
  ///
  /// THE CASE HAS TO TELL THE TWO APART AND THE FIRST WRITING OF IT DID NOT. It put two devices a
  /// pixel apart and expected the nearer; both were inside the ray's sphere at equal depth, so
  /// either answer was correct and the harness said so. What discriminates is a device whose CENTRE
  /// is outside the tiny rectangle but whose RADIUS the ray still enters: as a click it is
  /// selected, as a rectangle it is not, and no tie-break can confuse the two.
  TEST_METHOD(ADragUnderTheThresholdIsAClick)
  {
    Gesture gesture;
    gesture.candidates = {Device(1, 0, 0.0f, 0.0f, 8.0f)};
    gesture.Drag(103, 100, 105, 100);
    AssertIds({1}, gesture.Ids(), L"the ray entered its radius, although its centre is outside the drag");
  }

  TEST_METHOD(ADragAtTheThresholdIsARectangle)
  {
    Gesture gesture;
    gesture.candidates = {Device(1, 0, 0.0f, 0.0f), Device(2, 0, 1.0f, 1.0f)};
    gesture.Drag(98, 96, 98 + Outpost::DRAG_THRESHOLD_PIXELS, 104);
    AssertIds({1, 2}, gesture.Ids(), L"the rectangle takes both");
  }

  /// §5: "a ray whose origin lands inside any panel rectangle of §2 is refused before it is cast".
  /// A click the interface has already answered must not reach through it into the world.
  TEST_METHOD(APressInsideAPanelNeverReachesTheWorld)
  {
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f)};
    gesture.blocked = {Outpost::PickBox{90, 90, 110, 110}};
    gesture.Click(100, 100);
    AssertIds({}, gesture.Ids(), L"the press was the panel's and selected nothing");
  }

  TEST_METHOD(ShiftAddsAndTheResultIsAscendingAndWithoutRepeats)
  {
    Gesture gesture;
    gesture.candidates = {Device(9, 0, -20.0f, 0.0f), Device(4, 0, 20.0f, 0.0f)};
    gesture.Click(80, 100);        // id 9
    gesture.Click(120, 100, true); // id 4
    gesture.Click(120, 100, true); // the same one again
    AssertIds({4, 9}, gesture.Ids(), L"ascending, and the repeat does not appear twice");
  }

  /// A selected device that died stays selected otherwise, and every order given to it afterwards
  /// is refused with NotOwned for an object that is not there - which reads as the game ignoring
  /// the commander rather than as the unit having died.
  TEST_METHOD(RetainDropsWhatTheReplicaNoLongerHolds)
  {
    Gesture gesture;
    gesture.candidates = {Device(1, 0, -20.0f, 0.0f), Device(2, 0, 20.0f, 0.0f)};
    gesture.Drag(50, 50, 150, 150);
    AssertIds({1, 2}, gesture.Ids(), L"both are selected");

    const std::vector<Outpost::PickCandidate> survivors = {Device(2, 0, 20.0f, 0.0f)};
    gesture.selection.Retain(survivors);
    AssertIds({2}, gesture.Ids(), L"the one that died is forgotten");
  }

  /// An ordinary click must never flash a rubber band.
  TEST_METHOD(TheBandIsEmptyUntilTheThresholdIsCrossed)
  {
    Gesture gesture;
    gesture.selection.Advance(gesture.Frame(100, 100, true, false, false));
    Assert::IsFalse(gesture.selection.Dragging(), L"a press alone is not a drag");
    Assert::IsTrue(gesture.selection.Band().Empty(), L"and it draws nothing");

    gesture.selection.Advance(gesture.Frame(100 + Outpost::DRAG_THRESHOLD_PIXELS, 100, false, false, false));
    Assert::IsTrue(gesture.selection.Dragging(), L"past the threshold it is a drag");
    Assert::IsFalse(gesture.selection.Band().Empty(), L"and there is a rectangle to draw");

    gesture.selection.Advance(gesture.Frame(100 + Outpost::DRAG_THRESHOLD_PIXELS, 100, false, true, false));
    Assert::IsFalse(gesture.selection.Dragging(), L"the release ends it");
  }

  TEST_METHOD(ClearDropsEverythingIncludingADragInFlight)
  {
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f)};
    gesture.Click(100, 100);
    gesture.selection.Advance(gesture.Frame(50, 50, true, false, false));
    gesture.selection.Clear();
    AssertIds({}, gesture.Ids(), L"nothing is selected");
    Assert::IsFalse(gesture.selection.Dragging(), L"and no drag is in flight");
  }

  // ── Hold: a selection made somewhere other than the world (Design/Interface.md §8) ──────────

  TEST_METHOD(HoldReplacesTheSelectionOutright)
  {
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f), Device(9, 0, 100.0f, 0.0f)};
    gesture.Click(100, 100);
    const std::array<std::uint32_t, 1> one = {9};
    gesture.selection.Hold(one);
    AssertIds({9}, gesture.Ids(), L"what was held, and not what was clicked");
  }

  TEST_METHOD(HoldSortsAndDropsRepeats)
  {
    // Ids() promises ascending and without repeats, and Match::Select, the render view's selection
    // flag and picking's answers are all compared against that order.
    Gesture gesture;
    const std::array<std::uint32_t, 5> given = {9, 3, 9, 1, 3};
    gesture.selection.Hold(given);
    AssertIds({1, 3, 9}, gesture.Ids(), L"ascending, each once");
  }

  TEST_METHOD(HoldEndsADragInFlight)
  {
    // The press it came from was consumed by a panel and never reached the world, so a rubber band
    // left half-drawn from before would stay on the screen until the next world click.
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f)};
    gesture.selection.Advance(gesture.Frame(100, 100, true, false, false));
    gesture.selection.Advance(gesture.Frame(100 + Outpost::DRAG_THRESHOLD_PIXELS, 100, false, false, false));
    Assert::IsTrue(gesture.selection.Dragging(), L"a drag is in flight");
    const std::array<std::uint32_t, 1> one = {7};
    gesture.selection.Hold(one);
    Assert::IsFalse(gesture.selection.Dragging(), L"and the hold ended it");
    Assert::IsTrue(gesture.selection.Band().Empty(), L"with nothing left to draw");
  }

  TEST_METHOD(HoldForgetsAPressThatNeverReachedTheWorld)
  {
    // The press was consumed by a panel, so the release that follows it - wherever it lands - is
    // the end of a gesture the world never saw. A press left armed would have that release replace
    // the selection the portrait had just narrowed.
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f), Device(9, 0, 100.0f, 0.0f)};
    gesture.selection.Advance(gesture.Frame(100, 100, true, false, false));
    const std::array<std::uint32_t, 1> one = {9};
    gesture.selection.Hold(one);
    gesture.selection.Advance(gesture.Frame(100, 100, false, true, false));
    AssertIds({9}, gesture.Ids(), L"still what was held");
  }

  TEST_METHOD(HoldOfNothingSelectsNothing)
  {
    Gesture gesture;
    gesture.candidates = {Device(7, 0, 0.0f, 0.0f)};
    gesture.Click(100, 100);
    gesture.selection.Hold({});
    AssertIds({}, gesture.Ids(), L"an empty hold is an empty selection, not a selection kept");
  }
};

} // namespace ReplicaTests
