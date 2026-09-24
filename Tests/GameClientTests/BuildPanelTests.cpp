#include "pch.h"

#include <cmath>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
/// The emitted item whose rect is exactly _rect, or nullptr.
[[nodiscard]] const Outpost::HudItem* ItemAt(const Outpost::HudFrame& _frame, const Outpost::HudRect& _rect, Outpost::HudItem::Kind _kind)
{
  for (const Outpost::HudItem& item : _frame.items)
  {
    if ((item.kind == _kind) && (item.rect.x == _rect.x) && (item.rect.y == _rect.y) && (item.rect.w == _rect.w) &&
        (item.rect.h == _rect.h))
    {
      return &item;
    }
  }
  return nullptr;
}

[[nodiscard]] bool Same(const Neuron::QuadColor& _color, std::uint32_t _rgb) noexcept
{
  const Neuron::QuadColor wanted = Neuron::HexColor(_rgb);
  return (std::fabs(_color.red - wanted.red) < 0.002f) && (std::fabs(_color.green - wanted.green) < 0.002f) &&
         (std::fabs(_color.blue - wanted.blue) < 0.002f);
}

[[nodiscard]] bool IsTarget(const Outpost::HudFrame& _frame, Outpost::DesignId _design)
{
  for (const Outpost::HudTarget& target : _frame.hits.Targets())
  {
    if ((target.action == Outpost::HudAction::ArmModule) && (target.argument == static_cast<std::uint8_t>(_design)))
    {
      return true;
    }
  }
  return false;
}

[[nodiscard]] Outpost::HudState Open(std::uint32_t _credits, std::vector<Outpost::DesignId> _ownModules)
{
  Outpost::HudState state;
  state.player = 1;
  state.credits = _credits;
  state.buildPanelOpen = true;
  state.link = Outpost::LinkState::Linked;
  state.ownModules = std::move(_ownModules);
  return state;
}

// The handoff's hex, which the panel's palette is and this suite checks against.
constexpr std::uint32_t PLATE = 0x0E1214;
constexpr std::uint32_t PLATE_DIM = 0x090C0E;
constexpr std::uint32_t SIG_SHORT = 0xFF5A4A;
constexpr std::uint32_t TEXT = 0xE8ECEC;
constexpr std::uint32_t TEXT_DIM = 0x6B7A80;
} // namespace

/// M2.11b. **Two reasons a build button is dead, and they look different** -- asserted on the values the panel
/// emits, not by looking.
TEST_CLASS(TheTwoDeadStates)
{
public:
  /// **UNAFFORDABLE: THE PLATE STAYS LIT AND THE COST REDDENS**, and the button is still a target -- the host is
  /// what refuses an order a player cannot pay for.
  TEST_METHOD(AnUnaffordableModuleKeepsItsPlateAndReddensItsCost)
  {
    const Outpost::HudFrame frame = Outpost::BuildHud(Open(100, {}));
    const Outpost::HudRect button = Outpost::BUILD_BUTTON_YARD_L1;

    Assert::IsTrue(Outpost::ModuleButtonState(Outpost::DesignId::ModuleShipyardL1, Open(100, {})) ==
                   Outpost::BuildButtonState::Unaffordable);
    Assert::IsTrue(Same(ItemAt(frame, button, Outpost::HudItem::Kind::Solid)->color, PLATE));
    Assert::IsTrue(Same(ItemAt(frame, Outpost::BUTTON_COST.Within(button), Outpost::HudItem::Kind::Text)->color, SIG_SHORT));
    Assert::IsTrue(Same(ItemAt(frame, Outpost::BUTTON_NAME_LINE1.Within(button), Outpost::HudItem::Kind::Text)->color, TEXT));
    Assert::IsNotNull(ItemAt(frame, Outpost::BUTTON_SHORT_RULE.Within(button), Outpost::HudItem::Kind::Solid), L"the bottom rule");
    Assert::IsTrue(IsTarget(frame, Outpost::DesignId::ModuleShipyardL1));
  }

  /// **UNAVAILABLE: THE WHOLE BUTTON DIMS** -- plate, name and cost -- carries the hatch, and is not a target.
  /// An L2 with no L1 of its kind is the case the MVP has.
  TEST_METHOD(AnUnavailableModuleDimsEntirely)
  {
    const Outpost::HudFrame frame = Outpost::BuildHud(Open(5000, {}));
    const Outpost::HudRect button = Outpost::BUILD_BUTTON_YARD_L2;

    Assert::IsTrue(Outpost::ModuleButtonState(Outpost::DesignId::ModuleShipyardL2, Open(5000, {})) ==
                   Outpost::BuildButtonState::Unavailable);
    Assert::IsTrue(Same(ItemAt(frame, button, Outpost::HudItem::Kind::Solid)->color, PLATE_DIM));
    Assert::IsTrue(Same(ItemAt(frame, Outpost::BUTTON_COST.Within(button), Outpost::HudItem::Kind::Text)->color, TEXT_DIM));
    Assert::IsTrue(Same(ItemAt(frame, Outpost::BUTTON_NAME_LINE1.Within(button), Outpost::HudItem::Kind::Text)->color, TEXT_DIM));
    Assert::IsNull(ItemAt(frame, Outpost::BUTTON_SHORT_RULE.Within(button), Outpost::HudItem::Kind::Solid), L"no red rule");
    Assert::IsFalse(IsTarget(frame, Outpost::DesignId::ModuleShipyardL2));

    // The hatch: single pixels inside the button, and none inside a live one.
    std::size_t dots = 0;
    for (const Outpost::HudItem& item : frame.items)
    {
      if ((item.rect.w == 1) && (item.rect.h == 1) && (item.rect.x > button.x) && (item.rect.x < (button.x + button.w)) &&
          (item.rect.y > button.y) && (item.rect.y < (button.y + button.h)))
      {
        ++dots;
      }
    }
    Assert::IsTrue(dots > 100, L"the hatch is the geometric cue, not only the hue");
  }

  /// **BOTH AT ONCE READS AS UNAVAILABLE**: the state credits cannot fix wins.
  TEST_METHOD(UnaffordableAndUnavailableReadsAsUnavailable)
  {
    const Outpost::HudState state = Open(0, {});
    Assert::IsTrue(Outpost::ModuleButtonState(Outpost::DesignId::ModuleOreProcessorL2, state) == Outpost::BuildButtonState::Unavailable);

    const Outpost::HudFrame frame = Outpost::BuildHud(state);
    const Outpost::HudRect button = Outpost::BUILD_BUTTON_ORE_L2;
    Assert::IsTrue(Same(ItemAt(frame, button, Outpost::HudItem::Kind::Solid)->color, PLATE_DIM));
    Assert::IsTrue(Same(ItemAt(frame, Outpost::BUTTON_COST.Within(button), Outpost::HudItem::Kind::Text)->color, TEXT_DIM));
  }

  /// **AN L1 OF THE KIND MAKES ITS L2 AVAILABLE**, and only its own kind's.
  TEST_METHOD(AnL1MakesItsOwnL2Available)
  {
    const std::vector<Outpost::DesignId> yard{Outpost::DesignId::ModuleShipyardL1};
    Assert::IsTrue(Outpost::ModuleAvailable(Outpost::DesignId::ModuleShipyardL2, yard));
    Assert::IsFalse(Outpost::ModuleAvailable(Outpost::DesignId::ModuleOreProcessorL2, yard));

    const Outpost::HudFrame frame = Outpost::BuildHud(Open(5000, yard));
    Assert::IsTrue(IsTarget(frame, Outpost::DesignId::ModuleShipyardL2));
    Assert::IsFalse(IsTarget(frame, Outpost::DesignId::ModuleOreProcessorL2));

    // An L2 already built upgrades nothing further.
    Assert::IsFalse(Outpost::ModuleAvailable(Outpost::DesignId::ModuleShipyardL2, std::vector{Outpost::DesignId::ModuleShipyardL2}));
  }

  /// **FOUR MODULES AND NO FIFTH**: a placed level is unavailable at the cap, because an armed placement there
  /// could only ever be refused. An upgrade is still available -- it adds nothing.
  TEST_METHOD(ThePlacedLevelsAreUnavailableAtTheCap)
  {
    const std::vector<Outpost::DesignId> four{Outpost::DesignId::ModuleShipyardL1, Outpost::DesignId::ModuleOreProcessorL1,
                                              Outpost::DesignId::ModuleOreProcessorL1, Outpost::DesignId::ModuleOreProcessorL1};
    Assert::IsFalse(Outpost::ModuleAvailable(Outpost::DesignId::ModuleShipyardL1, four));
    Assert::IsFalse(Outpost::ModuleAvailable(Outpost::DesignId::ModuleOreProcessorL1, four));
    Assert::IsTrue(Outpost::ModuleAvailable(Outpost::DesignId::ModuleShipyardL2, four));
    Assert::IsTrue(Outpost::ModuleAvailable(Outpost::DesignId::ModuleShipyardL1, std::vector(3, Outpost::DesignId::ModuleOreProcessorL1)));
  }

  /// A live button is plain: lit plate, plain cost, a target.
  TEST_METHOD(ALiveModuleIsPlain)
  {
    const Outpost::HudFrame frame = Outpost::BuildHud(Open(5000, {}));
    const Outpost::HudRect button = Outpost::BUILD_BUTTON_ORE_L1;
    Assert::IsTrue(Outpost::ModuleButtonState(Outpost::DesignId::ModuleOreProcessorL1, Open(5000, {})) == Outpost::BuildButtonState::Live);
    Assert::IsTrue(Same(ItemAt(frame, button, Outpost::HudItem::Kind::Solid)->color, PLATE));
    Assert::IsTrue(Same(ItemAt(frame, Outpost::BUTTON_COST.Within(button), Outpost::HudItem::Kind::Text)->color, TEXT));
    Assert::IsTrue(IsTarget(frame, Outpost::DesignId::ModuleOreProcessorL1));
  }

  /// **NO SHIPYARD, NO SHIP ROW** (Q84): both ship buttons dim entirely and are no target, as an unavailable module
  /// is -- and a yard of either level brings them back.
  TEST_METHOD(TheShipRowIsDeadWithoutAShipyard)
  {
    const auto shipTargets = [](const Outpost::HudFrame& _frame)
    {
      std::size_t count = 0;
      for (const Outpost::HudTarget& target : _frame.hits.Targets())
      {
        count += (target.action == Outpost::HudAction::Build) ? 1 : 0;
      }
      return count;
    };

    const Outpost::HudFrame without = Outpost::BuildHud(Open(5000, {Outpost::DesignId::ModuleOreProcessorL1}));
    Assert::AreEqual(std::size_t{0}, shipTargets(without), L"a ship button with no yard is a button the host always refuses");
    Assert::IsTrue(Same(ItemAt(without, Outpost::BUILD_BUTTON_SHIP_0, Outpost::HudItem::Kind::Solid)->color, PLATE_DIM));
    Assert::IsTrue(
      Same(ItemAt(without, Outpost::BUTTON_COST.Within(Outpost::BUILD_BUTTON_SHIP_0), Outpost::HudItem::Kind::Text)->color, TEXT_DIM));

    // Q85: A LEVEL-ONE YARD BUILDS THE TWO OLD SHIPS, and the Cruiser wants a level-two yard AND the HeavyDriver.
    Assert::AreEqual(std::size_t{2}, shipTargets(Outpost::BuildHud(Open(5000, {Outpost::DesignId::ModuleShipyardL1}))));
    Assert::AreEqual(std::size_t{2}, shipTargets(Outpost::BuildHud(Open(5000, {Outpost::DesignId::ModuleShipyardL2}))));
    Outpost::HudState researched = Open(5000, {Outpost::DesignId::ModuleShipyardL2});
    researched.unlocked = 0x01;
    Assert::AreEqual(std::size_t{3}, shipTargets(Outpost::BuildHud(researched)));
    Outpost::HudState smallYard = Open(5000, {Outpost::DesignId::ModuleShipyardL1});
    smallYard.unlocked = 0x01;
    Assert::AreEqual(std::size_t{2}, shipTargets(Outpost::BuildHud(smallYard)), L"a Cruiser on a level-one yard");
  }
};

/// M4.4b, `OpenQuestions.md` Q85. **The research row**: the research station's button, and the research button's four looks.
TEST_CLASS(TheResearchRow)
{
public:
  [[nodiscard]] static const Outpost::HudTarget* ResearchTarget(const Outpost::HudFrame& _frame)
  {
    for (const Outpost::HudTarget& target : _frame.hits.Targets())
    {
      if (target.action == Outpost::HudAction::Research)
      {
        return &target;
      }
    }
    return nullptr;
  }

  /// The research station arms a placement like any placed module, from its own place in the new row.
  TEST_METHOD(TheResearchStationArmsAPlacement)
  {
    const Outpost::HudFrame frame = Outpost::BuildHud(Open(5000, {}));
    Assert::IsTrue(IsTarget(frame, Outpost::DesignId::ModuleResearchStationL1));
    Assert::IsTrue(Same(ItemAt(frame, Outpost::BUILD_BUTTON_RESEARCH_STATION, Outpost::HudItem::Kind::Solid)->color, PLATE));
  }

  /// **NO RESEARCH STATION: HATCHED AND NO TARGET.** With one: a target naming the HeavyDriver. Running: no target.
  /// Researched: no target.
  TEST_METHOD(TheResearchButtonNeedsAStationAndGoesQuietWhileRunningAndWhenDone)
  {
    Assert::IsNull(ResearchTarget(Outpost::BuildHud(Open(5000, {}))), L"research with no research station");
    Assert::IsTrue(
      Same(ItemAt(Outpost::BuildHud(Open(5000, {})), Outpost::BUILD_BUTTON_RESEARCH, Outpost::HudItem::Kind::Solid)->color, PLATE_DIM));

    const Outpost::HudState ready = Open(5000, {Outpost::DesignId::ModuleResearchStationL1});
    const Outpost::HudFrame readyFrame = Outpost::BuildHud(ready);
    const Outpost::HudTarget* target = ResearchTarget(readyFrame);
    Assert::IsNotNull(target);
    Assert::AreEqual(static_cast<int>(Outpost::ComponentId::HeavyDriver), static_cast<int>(target->argument));

    Outpost::HudState running = ready;
    running.researchProgress = 41;
    Assert::IsNull(ResearchTarget(Outpost::BuildHud(running)), L"a second order while one runs would be refused");

    Outpost::HudState done = ready;
    done.unlocked = 0x01;
    Assert::IsNull(ResearchTarget(Outpost::BuildHud(done)));
  }

  /// Unaffordable stays a target, as a ship button does: the host refuses it.
  TEST_METHOD(AnUnaffordableResearchIsStillATarget)
  {
    Assert::IsNotNull(ResearchTarget(Outpost::BuildHud(Open(100, {Outpost::DesignId::ModuleResearchStationL1}))));
  }

  /// The order it sends names the component, and nothing else.
  TEST_METHOD(TheResearchOrderNamesTheComponent)
  {
    const Outpost::Command order = Outpost::ResearchCommand(7, Outpost::ComponentId::HeavyDriver);
    Assert::IsTrue(order.type == Outpost::CommandType::Research);
    Assert::AreEqual(static_cast<int>(Outpost::ComponentId::HeavyDriver), static_cast<int>(order.TargetDesign()));
    Assert::IsTrue(order.selection.empty());
    Assert::IsTrue(Outpost::ResearchableComponent() == Outpost::ComponentId::HeavyDriver);
  }
};

/// M3.9, `OpenQuestions.md` Q69. **The depot's button**, in the ship row's third place.
TEST_CLASS(TheDepotButton)
{
public:
  /// It arms a depot like a module button, at 300, and the station's four modules do not count against it.
  TEST_METHOD(ItArmsADepotWhateverTheModules)
  {
    const Outpost::HudState state = Open(1000, {Outpost::DesignId::ModuleShipyardL1, Outpost::DesignId::ModuleShipyardL1,
                                                Outpost::DesignId::ModuleOreProcessorL1, Outpost::DesignId::ModuleOreProcessorL1});
    const Outpost::HudFrame frame = Outpost::BuildHud(state);
    Assert::IsTrue(IsTarget(frame, Outpost::DesignId::Depot), L"a full base disabled the depot");
    Assert::IsTrue(Outpost::ModuleAvailable(Outpost::DesignId::Depot, state.ownModules, 1));
  }

  /// **TWO A PLAYER**: with two built it is dead, hatched rather than a target.
  TEST_METHOD(TwoBuiltDisablesIt)
  {
    Outpost::HudState state = Open(1000, {});
    state.ownDepots = Outpost::MAXIMUM_DEPOTS_PER_PLAYER;
    const Outpost::HudFrame frame = Outpost::BuildHud(state);
    Assert::IsFalse(IsTarget(frame, Outpost::DesignId::Depot), L"a third depot could be armed");
    Assert::IsFalse(Outpost::ModuleAvailable(Outpost::DesignId::Depot, state.ownModules, 2));
  }
};

} // namespace GameClientTests
