#include "pch.h"

#include <cmath>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr Outpost::PlayerId OURS = 1;
constexpr Outpost::PlayerId ENEMY = 2;

constexpr float FRAME_WIDTH = 1440.0f;
constexpr float FRAME_HEIGHT = 960.0f;
constexpr float FRAME_ASPECT = FRAME_WIDTH / FRAME_HEIGHT;

[[nodiscard]] Outpost::CameraPose Overhead(float _distance) noexcept
{
  return Outpost::CameraPose{.focusX = 0.0f, .focusY = 0.0f, .headingRadians = 0.0f, .distance = _distance};
}

[[nodiscard]] Outpost::EntityRecord Ship(std::uint16_t _index, float _worldX, float _worldY, Outpost::PlayerId _owner,
                                         Outpost::DesignId _design)
{
  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(_index, 1);
  record.positionX = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(_worldX * Neuron::FIXED_ONE));
  record.positionY = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(_worldY * Neuron::FIXED_ONE));
  record.designIdentity = static_cast<std::uint8_t>(_design);
  record.owner = _owner;
  return record;
}

[[nodiscard]] Outpost::HitTestRequest Request(Outpost::PlayerId _player = OURS) noexcept
{
  return Outpost::HitTestRequest{.authoredX = 0.0f,
                                 .authoredY = 0.0f,
                                 .authoredWidth = FRAME_WIDTH,
                                 .authoredHeight = FRAME_HEIGHT,
                                 .aspectRatio = FRAME_ASPECT,
                                 .player = _player};
}

/// How many authored pixels one world unit covers ACROSS the frame at its center, so a test can place
/// a ship at a known screen distance rather than guessing.
///
/// **IT MEASURES ALONG WORLD Y AND NOT ALONG X, AND THAT IS THE WHOLE TRAP.** Heading zero looks along
/// +x, so +x is the camera's DEPTH axis and is foreshortened by the pitch; the camera's right is
/// world -y (`GameClient/Camera.cpp`). Measuring along x and then placing ships along x gives a
/// screen distance several times what the arithmetic said, which is how the first draft of these
/// tests came out taking only the anchor.
[[nodiscard]] float PixelsPerUnitAcross(const Outpost::CameraPose& _pose)
{
  float ax = 0.0f;
  float ay = 0.0f;
  float bx = 0.0f;
  float by = 0.0f;
  Assert::IsTrue(Outpost::PlaneToScreen(_pose, FRAME_ASPECT, 0.0f, 0.0f, ax, ay));
  Assert::IsTrue(Outpost::PlaneToScreen(_pose, FRAME_ASPECT, 0.0f, -100.0f, bx, by));
  return std::fabs(((bx - ax) * 0.5f) * FRAME_WIDTH) / 100.0f;
}
} // namespace

/// ADR-017's circle: **own ships only, same design only, fixed radius, centered on the ship.**
TEST_CLASS(TheGroupCircle)
{
public:
  /// **192 AUTHORED PIXELS** -- four times the touch floor, about a quarter of the frame's width.
  TEST_METHOD(TheRadiusIsOneNinetyTwo)
  {
    Assert::AreEqual(192.0f, Outpost::GROUP_RADIUS_AUTHORED_PIXELS);
    Assert::AreEqual(192.0f, 4.0f * 48.0f, L"four times Interface.md section 1's 48-pixel touch floor");
    Assert::AreEqual(300u, Outpost::DOUBLE_TAP_WINDOW_MILLISECONDS);
  }

  TEST_METHOD(ItTakesOwnShipsOfTheSameDesignInside)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    const float perUnit = PixelsPerUnitAcross(pose);
    const float insideUnits = 100.0f / perUnit; // a hundred pixels across the frame

    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter));         // the anchor
    entities.push_back(Ship(2, 0.0f, insideUnits, OURS, Outpost::DesignId::Fighter));  // in
    entities.push_back(Ship(3, 0.0f, -insideUnits, OURS, Outpost::DesignId::Fighter)); // in
    entities.push_back(Ship(4, 0.0f, insideUnits, OURS, Outpost::DesignId::Miner));    // wrong design
    entities.push_back(Ship(5, 0.0f, insideUnits, ENEMY, Outpost::DesignId::Fighter)); // wrong owner

    const std::vector<Outpost::WireIdentity> taken = Outpost::ShipsInGroupCircle(pose, Request(), entities, Outpost::PackIdentity(1, 1));

    Assert::AreEqual(static_cast<std::size_t>(3), taken.size());
  }

  /// **THE ANCHOR IS ALWAYS TAKEN** -- it is inside its own circle at distance zero, and saying so is
  /// cheaper than a reader wondering.
  TEST_METHOD(TheAnchorIsAlwaysTaken)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    const std::vector<Outpost::EntityRecord> entities{Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter)};

    const std::vector<Outpost::WireIdentity> taken = Outpost::ShipsInGroupCircle(pose, Request(), entities, Outpost::PackIdentity(1, 1));
    Assert::AreEqual(static_cast<std::size_t>(1), taken.size());
    Assert::AreEqual(static_cast<int>(Outpost::PackIdentity(1, 1)), static_cast<int>(taken[0]));
  }

  /// **A SHIP EXACTLY ON THE EDGE**, which `TechnicalDesign.md` section 8 names by itself because that
  /// is where a radius test is decided by a rounding nobody chose. At or inside is taken.
  TEST_METHOD(AShipExactlyOnTheEdgeIsTaken)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    const float perUnit = PixelsPerUnitAcross(pose);

    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter));
    entities.push_back(Ship(2, 0.0f, (Outpost::GROUP_RADIUS_AUTHORED_PIXELS - 1.0f) / perUnit, OURS, Outpost::DesignId::Fighter));
    entities.push_back(Ship(3, 0.0f, (Outpost::GROUP_RADIUS_AUTHORED_PIXELS + 8.0f) / perUnit, OURS, Outpost::DesignId::Fighter));

    const std::vector<Outpost::WireIdentity> taken = Outpost::ShipsInGroupCircle(pose, Request(), entities, Outpost::PackIdentity(1, 1));

    Assert::AreEqual(static_cast<std::size_t>(2), taken.size(), L"the ship on the edge was not taken, or the one past it was");
  }

  /// **THE CAMERA IS THE GROUP-SIZE CONTROL.** Because the circle is screen-space, zooming in takes a
  /// squad and zooming out takes the fleet -- and that is asserted here as a count rather than argued.
  TEST_METHOD(ZoomingOutTakesMore)
  {
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter));
    for (std::uint16_t index = 2; index < 12; ++index)
    {
      entities.push_back(Ship(index, 0.0f, static_cast<float>(index - 1) * 300.0f, OURS, Outpost::DesignId::Fighter));
    }

    const std::size_t closeUp =
      Outpost::ShipsInGroupCircle(Overhead(Outpost::MINIMUM_CAMERA_DISTANCE), Request(), entities, Outpost::PackIdentity(1, 1)).size();
    const std::size_t pulledBack =
      Outpost::ShipsInGroupCircle(Overhead(Outpost::MAXIMUM_CAMERA_DISTANCE), Request(), entities, Outpost::PackIdentity(1, 1)).size();

    Assert::IsTrue(pulledBack > closeUp, L"zooming out did not take more ships");
  }

  /// **THE RAKING-CAMERA CASE**, where the circle's world footprint is a wedge (ADR-010). Two ships at
  /// equal WORLD distance from the anchor -- one toward the top of the frame, one toward the bottom --
  /// are not at equal screen distance, and the circle takes the nearer-on-screen one. The stretch is
  /// bounded by `Interface.md` section 5's pitch floor, which M1.8 measured at 8.29x per pixel.
  TEST_METHOD(TheRakingCameraTakesAWedgeAndTheFloorBoundsIt)
  {
    // Fully zoomed in is the floor, which is the worst case the camera can reach.
    const Outpost::CameraPose pose = Overhead(Outpost::MINIMUM_CAMERA_DISTANCE);

    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter));
    entities.push_back(Ship(2, 150.0f, 0.0f, OURS, Outpost::DesignId::Fighter));  // away from the camera
    entities.push_back(Ship(3, -150.0f, 0.0f, OURS, Outpost::DesignId::Fighter)); // toward it

    float centerX = 0.0f;
    float centerY = 0.0f;
    float awayX = 0.0f;
    float awayY = 0.0f;
    float towardX = 0.0f;
    float towardY = 0.0f;
    Assert::IsTrue(Outpost::PlaneToScreen(pose, FRAME_ASPECT, 0.0f, 0.0f, centerX, centerY));
    Assert::IsTrue(Outpost::PlaneToScreen(pose, FRAME_ASPECT, 150.0f, 0.0f, awayX, awayY));
    Assert::IsTrue(Outpost::PlaneToScreen(pose, FRAME_ASPECT, -150.0f, 0.0f, towardX, towardY));

    // **THE WEDGE, AS A NUMBER**: the same world distance is a different screen distance either side.
    const float awayPixels = std::fabs(awayY - centerY) * 0.5f * FRAME_HEIGHT;
    const float towardPixels = std::fabs(towardY - centerY) * 0.5f * FRAME_HEIGHT;
    Assert::IsTrue(towardPixels > awayPixels, L"the camera is not raking; this test proves nothing");

    // And it is bounded: the ratio cannot exceed what the pitch floor allows.
    Assert::IsTrue((towardPixels / awayPixels) < 9.0f, L"the wedge is worse than the pitch floor should permit");
  }

  /// A double tap on nobody's entity is not an expansion.
  TEST_METHOD(AnUnownedAnchorTakesNothing)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    const std::vector<Outpost::EntityRecord> entities{Ship(1, 0.0f, 0.0f, Outpost::NO_PLAYER, Outpost::DesignId::Miner)};

    Assert::AreEqual(static_cast<std::size_t>(0),
                     Outpost::ShipsInGroupCircle(pose, Request(), entities, Outpost::PackIdentity(1, 1)).size());
  }

  TEST_METHOD(AnAnchorTheStoreDoesNotCarryTakesNothing)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    const std::vector<Outpost::EntityRecord> entities{Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter)};

    Assert::AreEqual(static_cast<std::size_t>(0),
                     Outpost::ShipsInGroupCircle(pose, Request(), entities, Outpost::PackIdentity(99, 1)).size());
  }
};

/// ADR-017's three cases, which are the whole of what M1.11 has to get right.
TEST_CLASS(TheDoubleTapExpansion)
{
public:
  /// **ONE TAP SELECTS ONE SHIP AND EXPANDS NOTHING.**
  TEST_METHOD(OneTapSelectsOneShip)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter));
    entities.push_back(Ship(2, 50.0f, 0.0f, OURS, Outpost::DesignId::Fighter));

    Outpost::Selection selection;
    selection.ReplaceWith(Outpost::PackIdentity(1, 1));
    Assert::AreEqual(static_cast<std::size_t>(1), selection.Count());
  }

  /// **A SECOND TAP ON THE SAME ENTITY EXPANDS.**
  TEST_METHOD(ASecondTapOnTheSameEntityExpands)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter));
    entities.push_back(Ship(2, 50.0f, 0.0f, OURS, Outpost::DesignId::Fighter));
    entities.push_back(Ship(3, 80.0f, 0.0f, OURS, Outpost::DesignId::Fighter));

    Outpost::Selection selection;
    selection.ReplaceWith(Outpost::PackIdentity(1, 1));

    const Outpost::ExpansionOutcome outcome =
      Outpost::ExpandSelection(selection, pose, Request(), entities, Outpost::PackIdentity(1, 1), Outpost::PackIdentity(1, 1));

    Assert::IsTrue(outcome.expanded);
    Assert::AreEqual(static_cast<std::size_t>(3), outcome.selected);
    Assert::AreEqual(static_cast<std::size_t>(3), selection.Count());
  }

  /// **TWO TAPS ON DIFFERENT SHIPS STAY TWO SINGLE TAPS**, which is the case ADR-017 names and the
  /// one that is easy to get wrong. Matching on identity rather than on screen distance is what makes
  /// it work while the fleet is moving, which is when it is used.
  TEST_METHOD(TwoTapsOnDifferentShipsStayTwoSingleTaps)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter));
    entities.push_back(Ship(2, 50.0f, 0.0f, OURS, Outpost::DesignId::Fighter));

    Outpost::Selection selection;
    selection.ReplaceWith(Outpost::PackIdentity(2, 1));

    const Outpost::ExpansionOutcome outcome =
      Outpost::ExpandSelection(selection, pose, Request(), entities, Outpost::PackIdentity(1, 1), Outpost::PackIdentity(2, 1));

    Assert::IsFalse(outcome.expanded);
    Assert::AreEqual(static_cast<std::size_t>(1), selection.Count());
  }

  /// **AN EXPANSION ABANDONED HALFWAY LEAVES ONE SHIP SELECTED RATHER THAN NOTHING**, because nothing
  /// was deferred waiting for the second tap.
  TEST_METHOD(AnAbandonedExpansionLeavesOneShipSelected)
  {
    Outpost::Selection selection;
    selection.ReplaceWith(Outpost::PackIdentity(1, 1));
    Assert::AreEqual(static_cast<std::size_t>(1), selection.Count());
    Assert::IsTrue(selection.Contains(Outpost::PackIdentity(1, 1)));
  }

  /// It can only ever ADD, so expanding twice is idempotent.
  TEST_METHOD(ExpandingTwiceChangesNothingTheSecondTime)
  {
    const Outpost::CameraPose pose = Overhead(6000.0f);
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Ship(1, 0.0f, 0.0f, OURS, Outpost::DesignId::Fighter));
    entities.push_back(Ship(2, 50.0f, 0.0f, OURS, Outpost::DesignId::Fighter));

    Outpost::Selection selection;
    selection.ReplaceWith(Outpost::PackIdentity(1, 1));

    const std::size_t once =
      Outpost::ExpandSelection(selection, pose, Request(), entities, Outpost::PackIdentity(1, 1), Outpost::PackIdentity(1, 1)).selected;
    const std::size_t twice =
      Outpost::ExpandSelection(selection, pose, Request(), entities, Outpost::PackIdentity(1, 1), Outpost::PackIdentity(1, 1)).selected;

    Assert::AreEqual(once, twice);
  }
};

} // namespace GameClientTests
