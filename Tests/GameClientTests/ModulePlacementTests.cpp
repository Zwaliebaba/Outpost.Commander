#include "pch.h"

#include <cmath>
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

/// Close enough that a tap 300 units from the station is well clear of its pick radius.
[[nodiscard]] Outpost::CameraPose Overhead() noexcept
{
  return Outpost::CameraPose{.focusX = 0.0f, .focusY = 0.0f, .headingRadians = 0.0f, .distance = 2000.0f};
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

/// A tap aimed at a world point.
[[nodiscard]] Outpost::HitTestRequest TapOn(const Outpost::CameraPose& _pose, float _worldX, float _worldY)
{
  float screenX = 0.0f;
  float screenY = 0.0f;
  static_cast<void>(Outpost::PlaneToScreen(_pose, ASPECT, _worldX, _worldY, screenX, screenY));
  return Outpost::HitTestRequest{.authoredX = ((screenX + 1.0f) * 0.5f) * AUTHORED_WIDTH,
                                 .authoredY = ((1.0f - screenY) * 0.5f) * AUTHORED_HEIGHT,
                                 .authoredWidth = AUTHORED_WIDTH,
                                 .authoredHeight = AUTHORED_HEIGHT,
                                 .aspectRatio = ASPECT,
                                 .player = MINE};
}

/// A station at the origin, which is where every test here builds.
[[nodiscard]] std::vector<Outpost::EntityRecord> Base()
{
  return {Record(1, 0.0f, 0.0f, MINE, Outpost::DesignId::Station)};
}
} // namespace

/// M2.11, `Interface.md` section 6: **with a module armed, a tap inside the radius places it**, and a tap outside
/// it, on the station, or on another module does nothing.
TEST_CLASS(ThePlacementTap)
{
public:
  TEST_METHOD(ATapInsideTheRadiusPlaces)
  {
    const Outpost::CameraPose pose = Overhead();
    const Outpost::PlacementOutcome outcome =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, 300.0f, 0.0f), Base(), {}, Outpost::DesignId::ModuleShipyardL1);
    Assert::IsTrue(outcome.action == Outpost::PlacementAction::Place);

    // On the wire's grid already, and within a quarter unit of where the finger was.
    Assert::AreEqual(outcome.site.x, Outpost::DequantizePosition(Outpost::QuantizePosition(outcome.site.x)));
    Assert::IsTrue(std::abs(outcome.site.x - (300 * Neuron::FIXED_ONE)) < (2 * Neuron::FIXED_ONE));
    Assert::IsTrue(std::abs(outcome.site.y) < (2 * Neuron::FIXED_ONE));
  }

  TEST_METHOD(ATapOutsideTheRadiusDoesNothing)
  {
    const Outpost::CameraPose pose = Overhead();
    const Outpost::PlacementOutcome outcome =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, 450.0f, 0.0f), Base(), {}, Outpost::DesignId::ModuleShipyardL1);
    Assert::IsTrue(outcome.action == Outpost::PlacementAction::Nothing);
    Assert::IsTrue(outcome.fault == Outpost::ModuleSiteFault::OutsideRadius);
  }

  /// **ON THE STATION IS NOTHING** -- whether the tap lands on its pick radius or only on its footprint.
  TEST_METHOD(ATapOnTheStationDoesNothing)
  {
    const Outpost::CameraPose pose = Overhead();
    const Outpost::PlacementOutcome onIt =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, 0.0f, 0.0f), Base(), {}, Outpost::DesignId::ModuleShipyardL1);
    Assert::IsTrue(onIt.action == Outpost::PlacementAction::Nothing);

    const Outpost::PlacementOutcome onItsHull =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, 120.0f, 0.0f), Base(), {}, Outpost::DesignId::ModuleShipyardL1);
    Assert::IsTrue(onItsHull.action == Outpost::PlacementAction::Nothing);
  }

  TEST_METHOD(ATapOnAnotherModuleDoesNothing)
  {
    const Outpost::CameraPose pose = Overhead();
    std::vector<Outpost::EntityRecord> entities = Base();
    entities.push_back(Record(2, 300.0f, 0.0f, MINE, Outpost::DesignId::ModuleOreProcessorL1));

    const Outpost::PlacementOutcome outcome =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, 300.0f, 0.0f), entities, {}, Outpost::DesignId::ModuleShipyardL1);
    Assert::IsTrue(outcome.action == Outpost::PlacementAction::Nothing);
  }

  /// **ANOTHER PLAYER'S MODULE DOES NOT COUNT AGAINST YOURS**, as at the host (`BuildTests`).
  TEST_METHOD(OnlyYourOwnModulesAreInThePreview)
  {
    std::vector<Outpost::EntityRecord> entities = Base();
    entities.push_back(Record(2, 300.0f, 0.0f, THEIRS, Outpost::DesignId::ModuleOreProcessorL1));
    entities.push_back(Record(3, -300.0f, 0.0f, MINE, Outpost::DesignId::ModuleShipyardL1));

    const std::vector<Outpost::PlacedModule> modules = Outpost::OwnModules(entities, MINE);
    Assert::AreEqual(std::size_t{1}, modules.size());
    Assert::AreEqual(Outpost::PackIdentity(3, 1), modules[0].identity);
    Assert::AreEqual(-300 * Neuron::FIXED_ONE, modules[0].position.x);
  }

  /// **A TAP ON ONE OF YOUR SHIPS IS STILL A SELECTION**, and takes the arming with it.
  TEST_METHOD(ATapOnYourOwnShipFallsThrough)
  {
    const Outpost::CameraPose pose = Overhead();
    std::vector<Outpost::EntityRecord> entities = Base();
    entities.push_back(Record(5, 300.0f, 100.0f, MINE, Outpost::DesignId::Miner));

    const Outpost::PlacementOutcome outcome =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, 300.0f, 100.0f), entities, {}, Outpost::DesignId::ModuleShipyardL1);
    Assert::IsTrue(outcome.action == Outpost::PlacementAction::FallThrough);
  }

  /// **AN ARMED L2 UPGRADES THE L1 IT IS TAPPED ON** (Q54, Q57), and only that kind.
  TEST_METHOD(AnArmedUpgradeTakesTheLevelItUpgrades)
  {
    const Outpost::CameraPose pose = Overhead();
    std::vector<Outpost::EntityRecord> entities = Base();
    entities.push_back(Record(2, 300.0f, 0.0f, MINE, Outpost::DesignId::ModuleShipyardL1));
    entities.push_back(Record(3, -300.0f, 0.0f, MINE, Outpost::DesignId::ModuleOreProcessorL1));

    const Outpost::PlacementOutcome yard =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, 300.0f, 0.0f), entities, {}, Outpost::DesignId::ModuleShipyardL2);
    Assert::IsTrue(yard.action == Outpost::PlacementAction::Upgrade);
    Assert::AreEqual(Outpost::PackIdentity(2, 1), yard.module);

    const Outpost::PlacementOutcome ore =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, -300.0f, 0.0f), entities, {}, Outpost::DesignId::ModuleShipyardL2);
    Assert::IsTrue(ore.action == Outpost::PlacementAction::Nothing, L"a shipyard upgrade on an ore processor");

    const Outpost::PlacementOutcome empty =
      Outpost::ResolvePlacementTap(pose, TapOn(pose, 0.0f, 300.0f), entities, {}, Outpost::DesignId::ModuleShipyardL2);
    Assert::IsTrue(empty.action == Outpost::PlacementAction::Nothing, L"an L2 is never placed on empty space");
  }

  TEST_METHOD(BothOrdersCarryWhatTheHostReads)
  {
    const Neuron::Vec2 site{.x = 300 * Neuron::FIXED_ONE, .y = -128 * Neuron::FIXED_ONE};
    const Outpost::Command place = Outpost::BuildPlaceModuleCommand(7, site, Outpost::DesignId::ModuleOreProcessorL1);
    Assert::IsTrue(place.type == Outpost::CommandType::PlaceModule);
    Assert::AreEqual(site.x, Outpost::DequantizePosition(place.targetX));
    Assert::AreEqual(site.y, Outpost::DequantizePosition(place.targetY));
    Assert::AreEqual(static_cast<int>(Outpost::DesignId::ModuleOreProcessorL1), static_cast<int>(place.placedDesign));
    Assert::IsTrue(place.selection.empty());

    const Outpost::Command upgrade =
      Outpost::BuildUpgradeModuleCommand(8, Outpost::PackIdentity(40, 3), Outpost::DesignId::ModuleShipyardL2);
    Assert::IsTrue(upgrade.type == Outpost::CommandType::UpgradeModule);
    Assert::AreEqual(Outpost::PackIdentity(40, 3), upgrade.TargetEntity());
    Assert::AreEqual(static_cast<int>(Outpost::DesignId::ModuleShipyardL2), static_cast<int>(upgrade.UpgradeLevel()));
  }
};

/// The arming, and the tap that is not the placement's.
TEST_CLASS(TheArming)
{
public:
  /// **A SECOND TAP ON THE ARMED BUTTON DISARMS IT**; a tap on another moves the arming.
  TEST_METHOD(TheSameButtonTwiceDisarms)
  {
    Outpost::ModuleArming arming;
    Assert::IsFalse(arming.IsArmed());
    arming.Toggle(Outpost::DesignId::ModuleShipyardL1);
    Assert::IsTrue(arming.IsArmed());
    arming.Toggle(Outpost::DesignId::ModuleOreProcessorL1);
    Assert::IsTrue(arming.IsArmed());
    Assert::IsTrue(arming.Armed() == Outpost::DesignId::ModuleOreProcessorL1);
    arming.Toggle(Outpost::DesignId::ModuleOreProcessorL1);
    Assert::IsFalse(arming.IsArmed());
  }

  /// **A TAP ON YOUR OWN MODULE HAS NO VERB** (Q57): picked as a structure, and then nothing -- the selection is
  /// unchanged and the build panel is not toggled.
  TEST_METHOD(AnUnarmedTapOnYourModuleDoesNothing)
  {
    const Outpost::CameraPose pose = Overhead();
    std::vector<Outpost::EntityRecord> entities = Base();
    entities.push_back(Record(2, 300.0f, 0.0f, MINE, Outpost::DesignId::ModuleShipyardL1));
    entities.push_back(Record(5, 300.0f, 200.0f, MINE, Outpost::DesignId::Miner));

    Assert::IsTrue(Outpost::TierOf(entities[1], MINE) == Outpost::PickTier::OwnStructure);

    Outpost::Selection selection;
    selection.ReplaceWith(Outpost::PackIdentity(5, 1));
    const Outpost::SelectionOutcome outcome = selection.Tap(pose, TapOn(pose, 300.0f, 0.0f), entities);
    Assert::IsTrue(outcome.verb == Outpost::OrderVerb::None);
    Assert::AreEqual(std::size_t{1}, selection.Count());
    Assert::IsTrue(selection.Contains(Outpost::PackIdentity(5, 1)));

    const Outpost::SelectionOutcome station = selection.Tap(pose, TapOn(pose, 0.0f, 0.0f), entities);
    Assert::IsTrue(station.verb == Outpost::OrderVerb::OpenBuildPanel, L"the station still opens the panel");
  }
};

/// **THE RADIUS, AS DRAWN** (`design_handoff_hud`): 24 dashes of a 48-segment world circle, projected.
TEST_CLASS(ThePlacementRing)
{
public:
  /// **EVERY SQUARE IS ON THE WORLD CIRCLE**: taken back through the camera to the plane, each lands 400 units
  /// from the station -- give or take the chord's sag, r(1 - cos(pi/48)) or about one unit, and a pixel of rounding.
  TEST_METHOD(EverySquareLiesOnTheWorldCircle)
  {
    const Outpost::CameraPose pose = Overhead();
    const Neuron::Vec2 center{.x = 100 * Neuron::FIXED_ONE, .y = -50 * Neuron::FIXED_ONE};
    const std::vector<Outpost::HudRect> squares = Outpost::PlacementRingSquares(pose, ASPECT, AUTHORED_WIDTH, AUTHORED_HEIGHT, center);
    Assert::IsTrue(squares.size() > 24, L"at least a square per dash");

    for (const Outpost::HudRect& square : squares)
    {
      Assert::AreEqual(Outpost::PLACEMENT_RING_STROKE_PIXELS, square.w);
      const float screenX = (((static_cast<float>(square.x) + 1.0f) / AUTHORED_WIDTH) * 2.0f) - 1.0f;
      const float screenY = 1.0f - (((static_cast<float>(square.y) + 1.0f) / AUTHORED_HEIGHT) * 2.0f);
      float worldX = 0.0f;
      float worldY = 0.0f;
      Assert::IsTrue(Outpost::ScreenToPlane(pose, ASPECT, screenX, screenY, worldX, worldY));
      const float dx = worldX - 100.0f;
      const float dy = worldY + 50.0f;
      Assert::AreEqual(400.0, static_cast<double>(std::sqrt((dx * dx) + (dy * dy))), 8.0);
    }
  }

  /// **DASHED: HALF THE CIRCLE IS DRAWN.** Nothing lands in the gaps, which is what keeps the ring from reading
  /// as the selection circle.
  TEST_METHOD(HalfTheSegmentsAreGaps)
  {
    const Outpost::CameraPose pose = Overhead();
    const std::vector<Outpost::HudRect> squares =
      Outpost::PlacementRingSquares(pose, ASPECT, AUTHORED_WIDTH, AUTHORED_HEIGHT, Neuron::Vec2{});

    // The middle of the second segment is a gap.
    const float gapAngle = 6.28318530718f * 1.5f / 48.0f;
    float gapX = 0.0f;
    float gapY = 0.0f;
    static_cast<void>(Outpost::PlaneToScreen(pose, ASPECT, 400.0f * std::cos(gapAngle), 400.0f * std::sin(gapAngle), gapX, gapY));
    const float gx = ((gapX + 1.0f) * 0.5f) * AUTHORED_WIDTH;
    const float gy = ((1.0f - gapY) * 0.5f) * AUTHORED_HEIGHT;
    for (const Outpost::HudRect& square : squares)
    {
      const float dx = (static_cast<float>(square.x) + 1.0f) - gx;
      const float dy = (static_cast<float>(square.y) + 1.0f) - gy;
      Assert::IsTrue(std::sqrt((dx * dx) + (dy * dy)) > 3.0f, L"a square in a gap");
    }
  }

  /// **THE ARMED BUTTON LOOKS ARMED AND THE RING IS DRAWN UNDER THE PANELS** -- by the values the panel emits.
  TEST_METHOD(TheArmedButtonAndTheRingAreEmitted)
  {
    Outpost::HudState state;
    state.player = MINE;
    state.credits = 5000;
    state.buildPanelOpen = true;
    state.link = Outpost::LinkState::Linked;
    state.moduleArmed = true;
    state.armedModule = Outpost::DesignId::ModuleOreProcessorL1;
    state.placementRing = {Outpost::HudRect{700, 400, 2, 2}};
    const Outpost::HudFrame frame = Outpost::BuildHud(state);

    std::size_t ringAt = frame.items.size();
    std::size_t firstPanel = frame.items.size();
    std::size_t topRules = 0;
    for (std::size_t index = 0; index < frame.items.size(); ++index)
    {
      const Outpost::HudItem& item = frame.items[index];
      if ((item.rect.x == 700) && (item.rect.y == 400) && (item.rect.w == 2))
      {
        ringAt = index;
      }
      if ((firstPanel == frame.items.size()) && (item.rect.w >= 100) && (item.rect.h >= 60))
      {
        firstPanel = index;
      }
      // The armed state's 4-pixel rule along the top edge of the ore L1 button.
      if ((item.rect.x == Outpost::BUILD_BUTTON_ORE_L1.x) && (item.rect.y == Outpost::BUILD_BUTTON_ORE_L1.y) && (item.rect.h == 4))
      {
        ++topRules;
      }
    }
    Assert::IsTrue(ringAt < firstPanel, L"the ring is drawn before any panel");
    Assert::AreEqual(std::size_t{1}, topRules);

    for (const Outpost::DesignId design : {Outpost::DesignId::ModuleShipyardL1, Outpost::DesignId::ModuleShipyardL2,
                                           Outpost::DesignId::ModuleOreProcessorL1, Outpost::DesignId::ModuleOreProcessorL2})
    {
      bool found = false;
      for (const Outpost::HudTarget& target : frame.hits.Targets())
      {
        found = found || ((target.action == Outpost::HudAction::ArmModule) && (target.argument == static_cast<std::uint8_t>(design)) &&
                          (target.tier == Outpost::TouchTier::UnderFire));
      }
      Assert::IsTrue(found, L"a module button missing from the hit table");
    }
  }

  /// **AN L2 SHOWS THE DIFFERENCE** (Q54), which is what the host charges.
  TEST_METHOD(AnUpgradeButtonShowsTheDifference)
  {
    Outpost::HudState state;
    state.player = MINE;
    state.credits = 5000;
    state.buildPanelOpen = true;
    state.link = Outpost::LinkState::Linked;
    const Outpost::HudFrame frame = Outpost::BuildHud(state);

    bool yard = false;
    bool ore = false;
    for (const Outpost::HudItem& item : frame.items)
    {
      yard = yard || ((item.text == L"300") && (item.rect.x == Outpost::BUTTON_COST.Within(Outpost::BUILD_BUTTON_YARD_L2).x));
      ore = ore || ((item.text == L"250") && (item.rect.x == Outpost::BUTTON_COST.Within(Outpost::BUILD_BUTTON_ORE_L2).x));
    }
    Assert::IsTrue(yard);
    Assert::IsTrue(ore);
  }
};

} // namespace GameClientTests
