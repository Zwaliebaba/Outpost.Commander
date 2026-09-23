#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

constexpr float AUTHORED_WIDTH = 1440.0f;
constexpr float AUTHORED_HEIGHT = 960.0f;
constexpr float ASPECT = AUTHORED_WIDTH / AUTHORED_HEIGHT;

[[nodiscard]] Outpost::CameraPose TopDown(float _distance = 6000.0f) noexcept
{
  return Outpost::CameraPose{.focusX = 0.0f, .focusY = 0.0f, .headingRadians = 0.0f, .distance = _distance};
}

[[nodiscard]] Outpost::EntityRecord Record(std::uint16_t _index, float _worldX, float _worldY, Outpost::PlayerId _owner,
                                           Outpost::DesignId _design)
{
  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(_index, 1);
  record.positionX = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(_worldX * Neuron::FIXED_ONE));
  record.positionY = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(_worldY * Neuron::FIXED_ONE));
  record.designIdentity = static_cast<std::uint8_t>(_design);
  record.owner = _owner;
  record.hullPercentRemaining = 100;
  return record;
}

/// Where a world point lands in authored pixels, so a test can aim a tap at a ship rather than
/// guessing.
[[nodiscard]] bool AuthoredOf(const Outpost::CameraPose& _pose, float _worldX, float _worldY, float& _outX, float& _outY) noexcept
{
  float screenX = 0.0f;
  float screenY = 0.0f;
  if (!Outpost::PlaneToScreen(_pose, ASPECT, _worldX, _worldY, screenX, screenY))
  {
    return false;
  }
  _outX = ((screenX + 1.0f) * 0.5f) * AUTHORED_WIDTH;
  _outY = ((1.0f - screenY) * 0.5f) * AUTHORED_HEIGHT;
  return true;
}

[[nodiscard]] Outpost::HitTestRequest At(float _x, float _y, Outpost::PlayerId _player = MINE) noexcept
{
  return Outpost::HitTestRequest{.authoredX = _x,
                                 .authoredY = _y,
                                 .authoredWidth = AUTHORED_WIDTH,
                                 .authoredHeight = AUTHORED_HEIGHT,
                                 .aspectRatio = ASPECT,
                                 .player = _player};
}
} // namespace

/// `Interface.md` section 4: **a tap's meaning comes from what is under it**, within section 1's
/// 24-pixel pick radius.
TEST_CLASS(TheHitTest)
{
public:
  TEST_METHOD(ATapOnAShipFindsIt)
  {
    const Outpost::CameraPose pose = TopDown();
    const std::vector<Outpost::EntityRecord> entities{Record(3, 400.0f, 300.0f, MINE, Outpost::DesignId::Fighter)};

    float x = 0.0f;
    float y = 0.0f;
    Assert::IsTrue(AuthoredOf(pose, 400.0f, 300.0f, x, y));

    const std::vector<Outpost::PickCandidate> found = Outpost::CandidatesUnderTap(pose, At(x, y), entities);
    Assert::AreEqual(static_cast<std::size_t>(1), found.size());
    Assert::AreEqual(static_cast<int>(Outpost::PackIdentity(3, 1)), static_cast<int>(found[0].identity));
    Assert::IsTrue(found[0].screenDistanceAuthoredPixels < 1.0f);
  }

  /// **THE RADIUS IS TWENTY-FOUR AUTHORED PIXELS**, which section 1 justifies by keeping a four-pixel
  /// ship at tactical zoom hittable. A tap just outside it finds nothing.
  TEST_METHOD(TheRadiusIsTwentyFourPixels)
  {
    Assert::AreEqual(24.0f, Neuron::PICK_RADIUS_AUTHORED_PIXELS);

    const Outpost::CameraPose pose = TopDown();
    const std::vector<Outpost::EntityRecord> entities{Record(3, 0.0f, 0.0f, MINE, Outpost::DesignId::Fighter)};

    float x = 0.0f;
    float y = 0.0f;
    Assert::IsTrue(AuthoredOf(pose, 0.0f, 0.0f, x, y));

    Assert::AreEqual(static_cast<std::size_t>(1), Outpost::CandidatesUnderTap(pose, At(x + 23.0f, y), entities).size());
    Assert::AreEqual(static_cast<std::size_t>(0), Outpost::CandidatesUnderTap(pose, At(x + 25.0f, y), entities).size());
  }

  /// **AT SEVERAL CAMERA PITCHES**, which is the case M1.10 names: the radius is on the screen and the
  /// camera rakes, so the world distance it covers changes and the screen distance must not.
  TEST_METHOD(TheRadiusHoldsAtEveryPitch)
  {
    for (const float distance : {Outpost::MINIMUM_CAMERA_DISTANCE, 4000.0f, 12000.0f, Outpost::MAXIMUM_CAMERA_DISTANCE})
    {
      const Outpost::CameraPose pose = TopDown(distance);
      const std::vector<Outpost::EntityRecord> entities{Record(3, 0.0f, 0.0f, MINE, Outpost::DesignId::Fighter)};

      float x = 0.0f;
      float y = 0.0f;
      Assert::IsTrue(AuthoredOf(pose, 0.0f, 0.0f, x, y));

      Assert::AreEqual(static_cast<std::size_t>(1), Outpost::CandidatesUnderTap(pose, At(x + 20.0f, y), entities).size(),
                       L"a ship inside the radius was missed at some pitch");
      Assert::AreEqual(static_cast<std::size_t>(0), Outpost::CandidatesUnderTap(pose, At(x + 30.0f, y), entities).size(),
                       L"a ship outside the radius was taken at some pitch");
    }
  }

  /// **THE TIER ORDER, AGAINST OVERLAPPING CANDIDATES OF DIFFERENT KINDS.** Own ship beats own
  /// structure beats hostile beats asteroid, and the tier wins across a tier even when something in a
  /// lower one is nearer.
  TEST_METHOD(TheTierOrderBeatsDistance)
  {
    const Outpost::CameraPose pose = TopDown();

    float x = 0.0f;
    float y = 0.0f;
    Assert::IsTrue(AuthoredOf(pose, 0.0f, 0.0f, x, y));

    // Four things within the radius, the own ship furthest away of the four.
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(1, 0.0f, 0.0f, NO_OWNER, Outpost::DesignId::Miner));
    entities.push_back(Record(2, 6.0f, 0.0f, THEIRS, Outpost::DesignId::Fighter));
    entities.push_back(Record(3, 12.0f, 0.0f, MINE, Outpost::DesignId::Station));
    entities.push_back(Record(4, 18.0f, 0.0f, MINE, Outpost::DesignId::Fighter));

    const std::vector<Outpost::PickCandidate> found = Outpost::CandidatesUnderTap(pose, At(x, y), entities);
    Assert::AreEqual(static_cast<std::size_t>(4), found.size());

    Outpost::PickCandidate hit;
    Assert::IsTrue(Outpost::ResolvePick(found, hit));
    Assert::IsTrue(hit.tier == Outpost::PickTier::OwnShip, L"a nearer thing in a lower tier won");
    Assert::AreEqual(static_cast<int>(Outpost::PackIdentity(4, 1)), static_cast<int>(hit.identity));
  }

  /// And within a tier, nearest wins -- which is the other half of the rule.
  TEST_METHOD(NearestWinsInsideATier)
  {
    const Outpost::CameraPose pose = TopDown();
    float x = 0.0f;
    float y = 0.0f;
    Assert::IsTrue(AuthoredOf(pose, 0.0f, 0.0f, x, y));

    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(7, 18.0f, 0.0f, MINE, Outpost::DesignId::Fighter));
    entities.push_back(Record(8, 4.0f, 0.0f, MINE, Outpost::DesignId::Fighter));

    Outpost::PickCandidate hit;
    Assert::IsTrue(Outpost::ResolvePick(Outpost::CandidatesUnderTap(pose, At(x, y), entities), hit));
    Assert::AreEqual(static_cast<int>(Outpost::PackIdentity(8, 1)), static_cast<int>(hit.identity));
  }

  /// **A TAP THAT LANDS ON TWO OVERLAPPING SHIPS RESOLVES THE SAME WAY TWICE**, which is M1.10's exit
  /// criterion and is the thing a scan over a container in an unstable order would break.
  TEST_METHOD(TwoOverlappingShipsResolveTheSameWayTwice)
  {
    const Outpost::CameraPose pose = TopDown();
    float x = 0.0f;
    float y = 0.0f;
    Assert::IsTrue(AuthoredOf(pose, 0.0f, 0.0f, x, y));

    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(11, 2.0f, 0.0f, MINE, Outpost::DesignId::Fighter));
    entities.push_back(Record(12, 2.0f, 0.0f, MINE, Outpost::DesignId::Fighter));

    Outpost::PickCandidate first;
    Outpost::PickCandidate second;
    Assert::IsTrue(Outpost::ResolvePick(Outpost::CandidatesUnderTap(pose, At(x, y), entities), first));
    Assert::IsTrue(Outpost::ResolvePick(Outpost::CandidatesUnderTap(pose, At(x, y), entities), second));
    Assert::AreEqual(static_cast<int>(first.identity), static_cast<int>(second.identity));
  }

  /// Ownership and design are all a snapshot carries, and they are all the tier needs.
  TEST_METHOD(TheTierComesFromOwnershipAndDesign)
  {
    Assert::IsTrue(Outpost::TierOf(Record(1, 0, 0, MINE, Outpost::DesignId::Fighter), MINE) == Outpost::PickTier::OwnShip);
    Assert::IsTrue(Outpost::TierOf(Record(1, 0, 0, MINE, Outpost::DesignId::Station), MINE) == Outpost::PickTier::OwnStructure);
    Assert::IsTrue(Outpost::TierOf(Record(1, 0, 0, THEIRS, Outpost::DesignId::Fighter), MINE) == Outpost::PickTier::Hostile);
    Assert::IsTrue(Outpost::TierOf(Record(1, 0, 0, THEIRS, Outpost::DesignId::Station), MINE) == Outpost::PickTier::Hostile);
    Assert::IsTrue(Outpost::TierOf(Record(1, 0, 0, NO_OWNER, Outpost::DesignId::Miner), MINE) == Outpost::PickTier::Asteroid);
  }

  /// **BEFORE THE JOIN IS ANSWERED NOTHING IS OWN** (ADR-013), so everything owned is hostile rather
  /// than everything being the player's.
  TEST_METHOD(AnUnseatedClientOwnsNothing)
  {
    Assert::IsTrue(Outpost::TierOf(Record(1, 0, 0, MINE, Outpost::DesignId::Fighter), Outpost::NO_PLAYER) == Outpost::PickTier::Hostile);
  }

private:
  static constexpr Outpost::PlayerId NO_OWNER = Outpost::NO_PLAYER;
};

/// `Interface.md` section 4's table, walked row by row.
TEST_CLASS(TheVerbTable)
{
public:
  TEST_METHOD(EmptySpaceWithASelectionMoves)
  {
    Outpost::TapOutcome outcome;
    outcome.action = Outpost::TapAction::MoveTo;
    Assert::IsTrue(Outpost::VerbForPick(outcome, true) == Outpost::OrderVerb::MoveTo);
  }

  /// **A TAP ON EMPTY SPACE WITH NOTHING SELECTED DOES NOTHING**, which section 4 states explicitly
  /// because it is a decision rather than a gap.
  TEST_METHOD(EmptySpaceWithNothingSelectedDoesNothing)
  {
    Outpost::TapOutcome outcome;
    outcome.action = Outpost::TapAction::Nothing;
    Assert::IsTrue(Outpost::VerbForPick(outcome, false) == Outpost::OrderVerb::None);

    outcome.action = Outpost::TapAction::MoveTo;
    Assert::IsTrue(Outpost::VerbForPick(outcome, false) == Outpost::OrderVerb::None);
  }

  TEST_METHOD(EachTierProducesItsRowsVerb)
  {
    Outpost::TapOutcome outcome;
    outcome.action = Outpost::TapAction::Occupied;

    outcome.hit.tier = Outpost::PickTier::OwnShip;
    Assert::IsTrue(Outpost::VerbForPick(outcome, true) == Outpost::OrderVerb::Select);
    Assert::IsTrue(Outpost::VerbForPick(outcome, false) == Outpost::OrderVerb::Select, L"selecting needs nothing selected first");

    outcome.hit.tier = Outpost::PickTier::OwnStructure;
    Assert::IsTrue(Outpost::VerbForPick(outcome, true) == Outpost::OrderVerb::OpenBuildPanel);

    outcome.hit.tier = Outpost::PickTier::Hostile;
    Assert::IsTrue(Outpost::VerbForPick(outcome, true) == Outpost::OrderVerb::Attack);
    Assert::IsTrue(Outpost::VerbForPick(outcome, false) == Outpost::OrderVerb::None, L"an attack needs something to attack with");

    outcome.hit.tier = Outpost::PickTier::Asteroid;
    Assert::IsTrue(Outpost::VerbForPick(outcome, true) == Outpost::OrderVerb::Mine);
    Assert::IsTrue(Outpost::VerbForPick(outcome, false) == Outpost::OrderVerb::None);
  }
};

/// The selection itself.
TEST_CLASS(TheSelection)
{
public:
  /// A tap on your own ship replaces the selection, and it fires now rather than waiting to see
  /// whether a second tap arrives (ADR-017).
  TEST_METHOD(ATapOnYourOwnShipReplacesTheSelection)
  {
    const Outpost::CameraPose pose = TopDown();
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(5, 0.0f, 0.0f, MINE, Outpost::DesignId::Fighter));
    entities.push_back(Record(6, 3000.0f, 0.0f, MINE, Outpost::DesignId::Fighter));

    Outpost::Selection selection;
    selection.Add(Outpost::PackIdentity(6, 1));

    float x = 0.0f;
    float y = 0.0f;
    Assert::IsTrue(AuthoredOf(pose, 0.0f, 0.0f, x, y));

    const Outpost::SelectionOutcome outcome = selection.Tap(pose, At(x, y), entities);
    Assert::IsTrue(outcome.verb == Outpost::OrderVerb::Select);
    Assert::IsTrue(outcome.selectionChanged);
    Assert::AreEqual(static_cast<std::size_t>(1), selection.Count());
    Assert::IsTrue(selection.Contains(Outpost::PackIdentity(5, 1)));
    Assert::IsFalse(selection.Contains(Outpost::PackIdentity(6, 1)), L"the old selection survived a replacement");
  }

  /// A tap on your own station opens the build panel **and leaves the selection alone**, which is the
  /// one row of the table that is about the interface rather than about the world.
  TEST_METHOD(ATapOnYourStationLeavesTheSelectionAlone)
  {
    const Outpost::CameraPose pose = TopDown();
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(9, 0.0f, 0.0f, MINE, Outpost::DesignId::Station));

    Outpost::Selection selection;
    selection.Add(Outpost::PackIdentity(77, 1));

    float x = 0.0f;
    float y = 0.0f;
    Assert::IsTrue(AuthoredOf(pose, 0.0f, 0.0f, x, y));

    const Outpost::SelectionOutcome outcome = selection.Tap(pose, At(x, y), entities);
    Assert::IsTrue(outcome.verb == Outpost::OrderVerb::OpenBuildPanel);
    Assert::IsFalse(outcome.selectionChanged);
    Assert::IsTrue(selection.Contains(Outpost::PackIdentity(77, 1)));
  }

  /// **A SELECTED SHIP THAT DIED IS DROPPED**, and a slot reused since is dropped too -- the whole
  /// packed identity is compared, so the selection cannot silently become somebody else's ship.
  TEST_METHOD(TheSelectionDropsWhatTheStoreNoLongerCarries)
  {
    Outpost::Selection selection;
    selection.Add(Outpost::PackIdentity(1, 1));
    selection.Add(Outpost::PackIdentity(2, 1));
    selection.Add(Outpost::PackIdentity(3, 1));

    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(1, 0.0f, 0.0f, MINE, Outpost::DesignId::Fighter));

    // Slot 2 was reused: same index, a new generation.
    Outpost::EntityRecord reused = Record(2, 0.0f, 0.0f, MINE, Outpost::DesignId::Fighter);
    reused.identity = Outpost::PackIdentity(2, 2);
    entities.push_back(reused);

    Assert::AreEqual(static_cast<std::size_t>(2), selection.RetainLiving(entities));
    Assert::AreEqual(static_cast<std::size_t>(1), selection.Count());
    Assert::IsTrue(selection.Contains(Outpost::PackIdentity(1, 1)));
    Assert::IsFalse(selection.Contains(Outpost::PackIdentity(2, 1)), L"a reused slot stayed selected");
  }

  /// The clear target is the only way to deselect, because a tap on empty space is already an order.
  TEST_METHOD(ClearingEmptiesIt)
  {
    Outpost::Selection selection;
    selection.Add(1);
    selection.Add(2);
    selection.Clear();
    Assert::IsTrue(selection.IsEmpty());
  }

  TEST_METHOD(AddingDoesNotDuplicate)
  {
    Outpost::Selection selection;
    selection.Add(5);
    selection.Add(5);
    Assert::AreEqual(static_cast<std::size_t>(1), selection.Count());
  }
};

} // namespace GameClientTests
