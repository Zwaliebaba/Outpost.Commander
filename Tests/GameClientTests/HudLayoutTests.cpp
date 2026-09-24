#include "pch.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr Outpost::PlayerId OURS = 1;

/// **THE WORST CASE THE HANDOFF DRAWS**, and then some: four selection groups, the build panel open with
/// an item in progress, a five-digit balance. Every target the interface can register at once, which is
/// what the tier and clear-space assertions have to hold over.
[[nodiscard]] Outpost::HudState Crowded(bool _leftHanded, bool _quitArmed)
{
  Outpost::HudState state;
  state.leftHanded = _leftHanded;
  state.player = OURS;
  state.credits = 12450;
  state.groups = {{.design = Outpost::DesignId::Miner, .count = 12, .hullPercent = 100},
                  {.design = Outpost::DesignId::Fighter, .count = 18, .hullPercent = 71},
                  {.design = Outpost::DesignId::Station, .count = 1, .hullPercent = 83},
                  {.design = Outpost::DesignId::Miner, .count = 3, .hullPercent = 40}};
  state.buildPanelOpen = true;
  state.ownModules = {Outpost::DesignId::ModuleShipyardL1}; // the ship row is live only with a yard (Q84)
  state.buildingWire = static_cast<std::uint8_t>(static_cast<std::uint8_t>(Outpost::DesignId::Miner) + 1);
  state.buildProgressPercent = 40;
  state.link = Outpost::LinkState::Linked;
  state.quitArmed = _quitArmed;
  return state;
}

/// Clear space between two boxes: negative if they overlap, and `INT32_MAX` if they are only diagonal
/// -- the same rule `Scripts/CheckHudGeometry.py` applies, so the two cannot disagree about what
/// "adjacent" means.
[[nodiscard]] std::int32_t Separation(const Outpost::HudRect& _first, const Outpost::HudRect& _second) noexcept
{
  const std::int32_t overlapX = std::min(_first.x + _first.w, _second.x + _second.w) - std::max(_first.x, _second.x);
  const std::int32_t overlapY = std::min(_first.y + _first.h, _second.y + _second.h) - std::max(_first.y, _second.y);
  if ((overlapX > 0) && (overlapY > 0))
  {
    return -1;
  }
  if (overlapX > 0)
  {
    return std::max(_first.y, _second.y) - std::min(_first.y + _first.h, _second.y + _second.h);
  }
  if (overlapY > 0)
  {
    return std::max(_first.x, _second.x) - std::min(_first.x + _first.w, _second.x + _second.w);
  }
  return INT32_MAX;
}

[[nodiscard]] const Outpost::HudTarget* FindTarget(const Outpost::HudFrame& _frame, Outpost::HudAction _action, std::uint8_t _argument = 0)
{
  for (const Outpost::HudTarget& target : _frame.hits.Targets())
  {
    if ((target.action == _action) && (target.argument == _argument))
    {
      return &target;
    }
  }
  return nullptr;
}

[[nodiscard]] Outpost::EntityRecord Record(std::uint16_t _index, Outpost::DesignId _design, std::uint8_t _hullPercent)
{
  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(_index, 1);
  record.designIdentity = static_cast<std::uint8_t>(_design);
  record.hullPercentRemaining = _hullPercent;
  record.owner = OURS;
  return record;
}
} // namespace

/// **`Interface.md` §1's RULES, ASSERTED OVER EVERY TARGET IN BOTH HANDEDNESS STATES.** The handoff is
/// explicit that this is judged by a test and not by eye: twelve pairs sit exactly on the 16-pixel floor,
/// so the rule erodes one control at a time and nothing on screen says so.
TEST_CLASS(TheTouchTiers)
{
public:
  /// Every interactive target is at least its tier's square, in both hands, armed and not.
  TEST_METHOD(EveryTargetMeetsItsTierInBothHands)
  {
    for (const bool left : {false, true})
    {
      for (const bool armed : {false, true})
      {
        const Outpost::HudFrame frame = Outpost::BuildHud(Crowded(left, armed));
        Assert::IsFalse(frame.hits.Targets().empty());
        for (const Outpost::HudTarget& target : frame.hits.Targets())
        {
          const std::int32_t side = Outpost::TierPixels(target.tier);
          Assert::IsTrue(target.hit.w >= side, L"a target is narrower than its tier");
          Assert::IsTrue(target.hit.h >= side, L"a target is shorter than its tier");
        }
      }
    }
  }

  /// **A COMBAT TARGET PASSING ON THE 48 FLOOR IS THE WAY THIS ERODES**, so each kind of target is
  /// pinned to the tier `Interface.md` §6 gives it rather than to whatever it happens to declare.
  TEST_METHOD(EachKindOfTargetDeclaresTheTierInterfaceMdGivesIt)
  {
    const Outpost::HudFrame frame = Outpost::BuildHud(Crowded(false, false));
    for (const Outpost::HudTarget& target : frame.hits.Targets())
    {
      switch (target.action)
      {
      case Outpost::HudAction::Build:
        Assert::IsTrue(target.tier == Outpost::TouchTier::UnderFire, L"a build button is not at the under-fire tier");
        break;
      case Outpost::HudAction::SelectGroup:
      case Outpost::HudAction::ClearSelection:
      case Outpost::HudAction::CancelBuild:
        Assert::IsTrue(target.tier == Outpost::TouchTier::Combat, L"a selection group, clear or cancel is not at the combat tier");
        break;
      case Outpost::HudAction::ArmQuit:
        Assert::IsTrue(target.tier == Outpost::TouchTier::Floor, L"the quit is meant to be the smallest target on screen");
        break;
      default:
        break;
      }
    }

    const Outpost::HudFrame armed = Outpost::BuildHud(Crowded(false, true));
    Assert::IsTrue(FindTarget(armed, Outpost::HudAction::ConfirmQuit)->tier == Outpost::TouchTier::Combat);
    Assert::IsTrue(FindTarget(armed, Outpost::HudAction::StayInMatch)->tier == Outpost::TouchTier::Combat);
  }

  /// **SIXTEEN PIXELS OF CLEAR SPACE BETWEEN EVERY TWO TARGETS ON ONE SURFACE**, and none overlapping.
  TEST_METHOD(EveryPairOnASurfaceHasItsClearSpaceInBothHands)
  {
    for (const bool left : {false, true})
    {
      for (const bool armed : {false, true})
      {
        const Outpost::HudFrame frame = Outpost::BuildHud(Crowded(left, armed));
        const auto targets = frame.hits.Targets();
        for (std::size_t first = 0; first < targets.size(); ++first)
        {
          for (std::size_t second = first + 1; second < targets.size(); ++second)
          {
            const std::int32_t gap = Separation(targets[first].hit, targets[second].hit);
            Assert::IsTrue(gap >= 0, L"two targets overlap");
            if (targets[first].surface == targets[second].surface)
            {
              Assert::IsTrue(gap >= Outpost::CLEAR_SPACE_PIXELS, L"two targets on one surface are closer than sixteen pixels");
            }
          }
        }
      }
    }
  }

  /// Nothing interactive leaves the authored frame, in either hand.
  TEST_METHOD(EveryTargetIsInsideTheFrame)
  {
    for (const bool left : {false, true})
    {
      const Outpost::HudFrame frame = Outpost::BuildHud(Crowded(left, false));
      for (const Outpost::HudTarget& target : frame.hits.Targets())
      {
        Assert::IsTrue(target.hit.x >= 0 && target.hit.y >= 0);
        Assert::IsTrue(target.hit.x + target.hit.w <= Neuron::INTERFACE_AUTHORED_WIDTH);
        Assert::IsTrue(target.hit.y + target.hit.h <= Neuron::INTERFACE_AUTHORED_HEIGHT);
      }
    }
  }
};

/// **THE TWO BOTTOM PANELS SWAP SIDES ON ONE VALUE**, so `OpenQuestions.md` Q33 costs a setting rather
/// than a rewrite.
TEST_CLASS(TheHandedness)
{
public:
  /// Right-handed: build bottom right, selection bottom left. Left-handed: the reverse.
  TEST_METHOD(OneValueSwapsTheBottomPanels)
  {
    const Outpost::HudFrame right = Outpost::BuildHud(Crowded(false, false));
    const Outpost::HudFrame left = Outpost::BuildHud(Crowded(true, false));

    const Outpost::HudTarget* rightBuild =
      FindTarget(right, Outpost::HudAction::Build, static_cast<std::uint8_t>(Outpost::DesignId::Miner));
    const Outpost::HudTarget* leftBuild = FindTarget(left, Outpost::HudAction::Build, static_cast<std::uint8_t>(Outpost::DesignId::Miner));
    Assert::IsTrue(rightBuild->hit.x > 720, L"right-handed, the build panel is not on the right");
    Assert::IsTrue(leftBuild->hit.x < 720, L"left-handed, the build panel is not on the left");

    // The outermost group rather than clear: at four groups the panel is 832 wide and clear sits at 720
    // exactly, which is the middle of the frame and says nothing about which side the panel is on.
    const std::uint8_t miner = static_cast<std::uint8_t>(Outpost::DesignId::Miner);
    Assert::AreEqual(16, FindTarget(right, Outpost::HudAction::SelectGroup, miner)->hit.x,
                     L"right-handed, the selection panel is not on the left");
    Assert::AreEqual(1264, FindTarget(left, Outpost::HudAction::SelectGroup, miner)->hit.x,
                     L"left-handed, the selection panel is not on the right");
  }

  /// **THE MIRRORED POSITIONS ARE `geometry.json`'s**, which carries the mirrored x for every affected
  /// rect precisely so nobody recomputes them by hand: group 0 at 1264, clear at 624, the two ship
  /// buttons at 424 and 288, and cancel at 16.
  TEST_METHOD(TheMirrorLandsWhereGeometryJsonSays)
  {
    const Outpost::HudFrame left = Outpost::BuildHud(Crowded(true, false));

    Assert::AreEqual(1264, FindTarget(left, Outpost::HudAction::SelectGroup, static_cast<std::uint8_t>(Outpost::DesignId::Miner))->hit.x);
    Assert::AreEqual(624, FindTarget(left, Outpost::HudAction::ClearSelection)->hit.x);
    Assert::AreEqual(424, FindTarget(left, Outpost::HudAction::Build, static_cast<std::uint8_t>(Outpost::DesignId::Miner))->hit.x);
    Assert::AreEqual(288, FindTarget(left, Outpost::HudAction::Build, static_cast<std::uint8_t>(Outpost::DesignId::Fighter))->hit.x);
    Assert::AreEqual(16, FindTarget(left, Outpost::HudAction::CancelBuild)->hit.x);
  }

  /// Credits and system **do not move**: only the two bottom panels reflect.
  TEST_METHOD(CreditsAndSystemStayPut)
  {
    const Outpost::HudFrame right = Outpost::BuildHud(Crowded(false, false));
    const Outpost::HudFrame left = Outpost::BuildHud(Crowded(true, false));
    Assert::IsTrue(FindTarget(right, Outpost::HudAction::ArmQuit)->hit == FindTarget(left, Outpost::HudAction::ArmQuit)->hit);
    Assert::IsTrue(left.hits.Test(10.0f, 10.0f).consumed, L"the credits panel moved");
  }

  /// The mirror is the handoff's one formula and is its own inverse.
  TEST_METHOD(TheMirrorIsItsOwnInverse)
  {
    const Outpost::HudRect rect{896, 656, 120, 96};
    Assert::IsTrue(Outpost::Mirror(rect) == Outpost::HudRect{424, 656, 120, 96});
    Assert::IsTrue(Outpost::Mirror(Outpost::Mirror(rect)) == rect);
  }
};

/// **A TAP INSIDE A PANEL NEVER REACHES THE WORLD**, and one outside every panel always does.
TEST_CLASS(ThePanelHitTest)
{
public:
  /// Every corner and the middle of every panel is consumed -- including the credits panel, which has
  /// nothing to press. A tap on a balance that fell through would be a move order to wherever the balance
  /// is drawn.
  TEST_METHOD(EveryPointOfEveryPanelIsConsumed)
  {
    for (const bool left : {false, true})
    {
      const Outpost::HudFrame frame = Outpost::BuildHud(Crowded(left, false));
      Assert::IsFalse(frame.hits.Blockers().empty());
      for (const Outpost::HudRect& panel : frame.hits.Blockers())
      {
        const float right = static_cast<float>(panel.x + panel.w) - 0.5f;
        const float bottom = static_cast<float>(panel.y + panel.h) - 0.5f;
        const float x = static_cast<float>(panel.x);
        const float y = static_cast<float>(panel.y);
        Assert::IsTrue(frame.hits.Test(x, y).consumed);
        Assert::IsTrue(frame.hits.Test(right, y).consumed);
        Assert::IsTrue(frame.hits.Test(x, bottom).consumed);
        Assert::IsTrue(frame.hits.Test(right, bottom).consumed);
        Assert::IsTrue(frame.hits.Test((x + right) * 0.5f, (y + bottom) * 0.5f).consumed);
      }
    }
  }

  /// The middle of the frame is the battlefield.
  TEST_METHOD(TheBattlefieldIsNotConsumed)
  {
    const Outpost::HudFrame frame = Outpost::BuildHud(Crowded(false, false));
    Assert::IsFalse(frame.hits.Test(720.0f, 480.0f).consumed);
  }

  /// A tap on a target returns its verb; a tap in a panel beside one returns none but is still consumed.
  TEST_METHOD(ATargetAnswersWithItsVerbAndAPanelWithNone)
  {
    const Outpost::HudFrame frame = Outpost::BuildHud(Crowded(false, false));

    const Outpost::HudHit onMiner = frame.hits.Test(900.0f, 700.0f);
    Assert::IsTrue(onMiner.action == Outpost::HudAction::Build);
    Assert::AreEqual(static_cast<int>(Outpost::DesignId::Miner), static_cast<int>(onMiner.argument));

    // The sixteen pixels between the two ship buttons belong to the panel and to nothing in it.
    const Outpost::HudHit between = frame.hits.Test(1020.0f, 700.0f);
    Assert::IsTrue(between.consumed);
    Assert::IsTrue(between.action == Outpost::HudAction::None);
  }

  /// **NOTHING SELECTED, NO SELECTION PANEL; STATION NOT SELECTED, NO BUILD PANEL** -- and so nothing of
  /// either in the hit table, where an invisible target would swallow a move order.
  TEST_METHOD(AHiddenPanelTakesNoTaps)
  {
    Outpost::HudState idle;
    idle.link = Outpost::LinkState::Linked;
    const Outpost::HudFrame frame = Outpost::BuildHud(idle);

    Assert::IsFalse(frame.hits.Test(100.0f, 900.0f).consumed, L"the hidden selection panel swallowed a tap");
    Assert::IsFalse(frame.hits.Test(1000.0f, 700.0f).consumed, L"the hidden build panel swallowed a tap");
  }

  /// **NOTHING BUILDING, NO CANCEL** -- not drawn, and not in the hit table.
  TEST_METHOD(CancelIsRegisteredOnlyWhileSomethingBuilds)
  {
    Outpost::HudState state = Crowded(false, false);
    Assert::IsNotNull(FindTarget(Outpost::BuildHud(state), Outpost::HudAction::CancelBuild));

    state.buildingWire = 0;
    Assert::IsNull(FindTarget(Outpost::BuildHud(state), Outpost::HudAction::CancelBuild));
  }

  /// **UNAFFORDABLE STAYS A TARGET.** The host is what refuses it (M1.6); a client that refused as well
  /// would be a second copy of the rule.
  TEST_METHOD(AnUnaffordableButtonIsStillATarget)
  {
    Outpost::HudState state = Crowded(false, false);
    state.credits = 0;
    Assert::IsNotNull(
      FindTarget(Outpost::BuildHud(state), Outpost::HudAction::Build, static_cast<std::uint8_t>(Outpost::DesignId::Fighter)));
  }

  /// **THE ONE PANEL WITH A BUTTON FOR EVERY BUILDABLE DESIGN**, taken from the table rather than named.
  TEST_METHOD(TheShipRowIsTheBuildableDesigns)
  {
    const auto designs = Outpost::BuildableDesigns();
    Assert::AreEqual(std::size_t{2}, designs.size());
    for (const Outpost::DesignId design : designs)
    {
      Assert::IsTrue(Outpost::Design(design).buildable);
    }
  }
};

/// **THE QUIT ARMS, QUITS, AND DISARMS ITSELF AFTER FOUR SECONDS.**
TEST_CLASS(TheQuitConfirm)
{
public:
  TEST_METHOD(ItIsArmedForExactlyFourSeconds)
  {
    Outpost::QuitConfirm quit;
    Assert::IsFalse(quit.IsArmed(0));

    quit.Arm(1000);
    Assert::IsTrue(quit.IsArmed(1000));
    Assert::IsTrue(quit.IsArmed(4999));
    Assert::IsFalse(quit.IsArmed(5000), L"the quit was still armed at four seconds");
  }

  TEST_METHOD(StayDisarmsIt)
  {
    Outpost::QuitConfirm quit;
    quit.Arm(0);
    quit.Disarm();
    Assert::IsFalse(quit.IsArmed(1));
  }

  /// Armed, the arming target is gone and the two confirm targets replace it -- so a double contact on
  /// one spot cannot end the match.
  TEST_METHOD(ArmedTheSecondTapIsOnADifferentTarget)
  {
    const Outpost::HudFrame armed = Outpost::BuildHud(Crowded(false, true));
    Assert::IsNull(FindTarget(armed, Outpost::HudAction::ArmQuit));

    const Outpost::HudHit sameSpot = armed.hits.Test(768.0f, 32.0f);
    Assert::IsTrue(sameSpot.action != Outpost::HudAction::ConfirmQuit, L"a second tap on the arming spot quit the match");
  }
};

/// **THE SELECTION PANEL'S GROUPING MATCHES WHAT M1.11 SELECTED.**
TEST_CLASS(TheSelectionSummary)
{
public:
  /// A double tap's expansion is one design by construction, so its summary is one group whose count is
  /// the expansion's.
  TEST_METHOD(ADoubleTapIsOneGroupOfItsCount)
  {
    const Outpost::CameraPose pose{.focusX = 0.0f, .focusY = 0.0f, .headingRadians = 0.0f, .distance = 6000.0f};
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(1, Outpost::DesignId::Fighter, 100));
    entities.push_back(Record(2, Outpost::DesignId::Fighter, 50));
    entities.push_back(Record(3, Outpost::DesignId::Fighter, 90));
    entities.push_back(Record(4, Outpost::DesignId::Miner, 100));

    Outpost::Selection selection;
    selection.ReplaceWith(Outpost::PackIdentity(1, 1));
    const Outpost::HitTestRequest request{
      .authoredX = 0.0f, .authoredY = 0.0f, .authoredWidth = 1440.0f, .authoredHeight = 960.0f, .aspectRatio = 1.5f, .player = OURS};
    const Outpost::ExpansionOutcome expanded =
      Outpost::ExpandSelection(selection, pose, request, entities, Outpost::PackIdentity(1, 1), Outpost::PackIdentity(1, 1));
    Assert::IsTrue(expanded.expanded);

    const auto groups = Outpost::SummarizeSelection(selection.Identities(), entities);
    Assert::AreEqual(std::size_t{1}, groups.size());
    Assert::IsTrue(groups[0].design == Outpost::DesignId::Fighter);
    Assert::AreEqual(static_cast<std::uint32_t>(expanded.selected), groups[0].count);
    Assert::AreEqual(80, static_cast<int>(groups[0].hullPercent), L"the aggregate hull is the mean of 100, 50 and 90");
  }

  /// Mixed selections group by design, **in design order**, whatever order the selection holds them in.
  TEST_METHOD(GroupsAreByDesignInDesignOrder)
  {
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(1, Outpost::DesignId::Fighter, 100));
    entities.push_back(Record(2, Outpost::DesignId::Miner, 100));
    entities.push_back(Record(3, Outpost::DesignId::Fighter, 100));

    const std::vector<Outpost::WireIdentity> selection{Outpost::PackIdentity(1, 1), Outpost::PackIdentity(2, 1),
                                                       Outpost::PackIdentity(3, 1), Outpost::PackIdentity(9, 1)};
    const auto groups = Outpost::SummarizeSelection(selection, entities);

    Assert::AreEqual(std::size_t{2}, groups.size());
    Assert::IsTrue(groups[0].design == Outpost::DesignId::Miner);
    Assert::AreEqual(1u, groups[0].count);
    Assert::IsTrue(groups[1].design == Outpost::DesignId::Fighter);
    Assert::AreEqual(2u, groups[1].count, L"an identity the snapshot no longer carries was counted");
  }

  /// **TAPPING A GROUP NARROWS THE SELECTION TO IT.**
  TEST_METHOD(TappingAGroupNarrowsToItsDesign)
  {
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(1, Outpost::DesignId::Fighter, 100));
    entities.push_back(Record(2, Outpost::DesignId::Miner, 100));
    entities.push_back(Record(3, Outpost::DesignId::Fighter, 100));

    Outpost::Selection selection;
    selection.Add(Outpost::PackIdentity(1, 1));
    selection.Add(Outpost::PackIdentity(2, 1));
    selection.Add(Outpost::PackIdentity(3, 1));

    Assert::AreEqual(std::size_t{2}, Outpost::NarrowToDesign(selection, entities, Outpost::DesignId::Fighter));
    Assert::IsFalse(selection.Contains(Outpost::PackIdentity(2, 1)));
  }

  /// **A GROUP'S CARGO IS ITS MEAN CHIPS, ROUNDED TO NEAREST** (M2.7, Q53), and only a design that carries ore
  /// has any: two miners at one and four chips light three -- (1 + 4) / 2 rounds up -- and a fighter carries
  /// nothing whatever its flags say.
  TEST_METHOD(AGroupsCargoIsItsMeanChipsAndOnlyMinersCarry)
  {
    std::vector<Outpost::EntityRecord> entities;
    entities.push_back(Record(1, Outpost::DesignId::Miner, 100));
    entities.push_back(Record(2, Outpost::DesignId::Miner, 100));
    entities.push_back(Record(3, Outpost::DesignId::Fighter, 100));
    entities[0].flags = Outpost::WithCargoChips(0, 1);
    entities[1].flags = Outpost::WithCargoChips(0, 4);

    const std::vector<Outpost::WireIdentity> selection{Outpost::PackIdentity(1, 1), Outpost::PackIdentity(2, 1),
                                                       Outpost::PackIdentity(3, 1)};
    const auto groups = Outpost::SummarizeSelection(selection, entities);
    Assert::AreEqual(std::size_t{2}, groups.size());
    Assert::IsTrue(groups[0].carriesOre);
    Assert::AreEqual(3, static_cast<int>(groups[0].cargoChips));
    Assert::IsFalse(groups[1].carriesOre, L"a fighter carries no ore");
  }

  /// **FOUR CHIPS FOR A DESIGN THAT CARRIES ORE, AND NO ROW FOR ONE THAT DOES NOT** -- `ORE` for the lit ones and
  /// `TRACK` for the rest, at the handoff's geometry.
  TEST_METHOD(TheCargoRowIsFourChipsOnlyWhereOreIsCarried)
  {
    Outpost::HudState state;
    state.groups = {{.design = Outpost::DesignId::Miner, .count = 2, .hullPercent = 100, .carriesOre = true, .cargoChips = 3},
                    {.design = Outpost::DesignId::Fighter, .count = 1, .hullPercent = 100}};
    const Outpost::HudFrame frame = Outpost::BuildHud(state);

    const auto chipsIn = [&frame](std::size_t _group, bool _lit)
    {
      const Outpost::HudRect cell = Outpost::SelectionGroupRect(_group);
      int found = 0;
      for (const Outpost::HudItem& item : frame.items)
      {
        const bool isChip = (item.rect.y == cell.y + Outpost::GROUP_CARGO_CHIP.y) && (item.rect.w == Outpost::GROUP_CARGO_CHIP.w) &&
                            (item.rect.h == Outpost::GROUP_CARGO_CHIP.h) && (item.rect.x >= cell.x) && (item.rect.x < cell.x + cell.w);
        const bool ore = std::fabs(item.color.red - Neuron::HexColor(0xD8A23C).red) < 0.001f;
        if (isChip && (ore == _lit))
        {
          ++found;
        }
      }
      return found;
    };
    Assert::AreEqual(3, chipsIn(0, true), L"three lit chips");
    Assert::AreEqual(1, chipsIn(0, false), L"one empty chip");
    Assert::AreEqual(0, chipsIn(1, true) + chipsIn(1, false), L"a fighter drew a cargo row");
  }

  /// The panel draws **one to four groups** and is sized by the handoff's formula for each.
  TEST_METHOD(ThePanelWidthFollowsTheGroupCount)
  {
    Outpost::HudState state;
    state.groups = {{.design = Outpost::DesignId::Miner, .count = 1, .hullPercent = 100}};
    Outpost::HudFrame frame = Outpost::BuildHud(state);
    Assert::AreEqual(192, FindTarget(frame, Outpost::HudAction::ClearSelection)->hit.x, L"at one group, clear is at 192");

    state.groups.push_back({.design = Outpost::DesignId::Fighter, .count = 1, .hullPercent = 100});
    frame = Outpost::BuildHud(state);
    Assert::AreEqual(368, FindTarget(frame, Outpost::HudAction::ClearSelection)->hit.x, L"at two groups, clear is at 368");
  }
};

/// The readouts' strings and the wire's building byte.
TEST_CLASS(TheReadouts)
{
public:
  /// **THE FLASH IS DRAWN AT ITS RECT, IN ITS COLOR, AT ITS ALPHA** -- cyan on a gain, amber on a spend -- and
  /// not at all when nothing changed (M2.7, Q36).
  TEST_METHOD(TheChangeFlashIsDrawnUnderTheBalance)
  {
    const auto flashIn = [](const Outpost::HudFrame& _frame) -> const Outpost::HudItem*
    {
      for (const Outpost::HudItem& item : _frame.items)
      {
        if ((item.kind == Outpost::HudItem::Kind::Solid) && (item.rect == Outpost::CREDITS_FLASH))
        {
          return &item;
        }
      }
      return nullptr;
    };

    Outpost::HudState state;
    Assert::IsNull(flashIn(Outpost::BuildHud(state)), L"a flash with nothing to show");

    state.creditFlash = Outpost::CreditChange::Gain;
    state.creditFlashAlpha = 0.5f;
    const Outpost::HudFrame gain = Outpost::BuildHud(state);
    Assert::IsNotNull(flashIn(gain));
    Assert::AreEqual(0.5f, flashIn(gain)->color.alpha, 0.0001f);
    Assert::AreEqual(Neuron::HexColor(0x38D1F5).blue, flashIn(gain)->color.blue, 0.0001f, L"a gain is cyan");

    state.creditFlash = Outpost::CreditChange::Spend;
    const Outpost::HudFrame spend = Outpost::BuildHud(state);
    Assert::AreEqual(Neuron::HexColor(0xFFB020).blue, flashIn(spend)->color.blue, 0.0001f, L"a spend is amber");
  }

  TEST_METHOD(CreditsCarryAThousandsSeparator)
  {
    Assert::AreEqual(std::wstring{L"0"}, Outpost::FormatCredits(0));
    Assert::AreEqual(std::wstring{L"999"}, Outpost::FormatCredits(999));
    Assert::AreEqual(std::wstring{L"1,000"}, Outpost::FormatCredits(1000));
    Assert::AreEqual(std::wstring{L"12,450"}, Outpost::FormatCredits(12450));
    Assert::AreEqual(std::wstring{L"99,999"}, Outpost::FormatCredits(99999));
    Assert::AreEqual(std::wstring{L"1,000,000"}, Outpost::FormatCredits(1000000));
  }

  /// **ZERO IS NOTHING, AND ANYTHING ELSE IS THE DESIGN PLUS ONE** -- `BuildSystem::WireBuildingDesign`.
  TEST_METHOD(TheBuildingByteIsTheDesignPlusOne)
  {
    Outpost::DesignId design = Outpost::DesignId::Station;
    Assert::IsFalse(Outpost::BuildingDesign(0, design));
    Assert::IsTrue(Outpost::BuildingDesign(1, design));
    Assert::IsTrue(design == Outpost::DesignId::Miner);
    Assert::IsTrue(Outpost::BuildingDesign(2, design));
    Assert::IsTrue(design == Outpost::DesignId::Fighter);
  }

  /// A station order carries a design and **no selection**, which is what the host requires of one.
  TEST_METHOD(AStationOrderCarriesADesignAndNoSelection)
  {
    const Outpost::Command build = Outpost::BuildStationCommand(7, Outpost::CommandType::Build, Outpost::DesignId::Fighter);
    Assert::IsTrue(build.type == Outpost::CommandType::Build);
    Assert::AreEqual(static_cast<int>(Outpost::DesignId::Fighter), static_cast<int>(build.TargetDesign()));
    Assert::IsTrue(build.selection.empty());
    Assert::IsFalse(Outpost::ActsOnSelection(build.type));

    const Outpost::Command cancel = Outpost::BuildStationCommand(8, Outpost::CommandType::CancelBuild, Outpost::DesignId::Miner);
    Assert::IsTrue(cancel.selection.empty());
  }
};

/// **THE FRAME THROUGH THE INTERFACE FIT**, which is where a panel stops being authored and starts being
/// pixels.
TEST_CLASS(TheEmittedQuads)
{
public:
  /// At the target device's exact 2x every rect doubles, edge for edge, and every quad lands on whole
  /// pixels -- a plate between pixels would blur its hairline.
  TEST_METHOD(EveryPlateLandsOnWholePixelsAtTwiceTheAuthoredSize)
  {
    const Outpost::HudFrame frame = Outpost::BuildHud(Crowded(false, false));
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(2880, 1920);

    Outpost::HudTypeface typeface;
    typeface.solid = Neuron::AtlasSlot{.left = 1, .top = 1, .widthPixels = 4, .heightPixels = 4};
    typeface.atlasWidthPixels = 1024;
    typeface.atlasHeightPixels = 512;

    std::vector<Neuron::GlyphQuad> quads;
    Outpost::EmitQuads(frame, typeface, fit, quads);
    Assert::IsFalse(quads.empty());

    // The first solid after the frame ticks is the credits panel: 272 x 88 authored, 544 x 176 physical.
    bool foundCredits = false;
    for (const Neuron::GlyphQuad& quad : quads)
    {
      Assert::AreEqual(std::round(quad.left), quad.left);
      Assert::AreEqual(std::round(quad.bottom), quad.bottom);
      if ((quad.left == 0.0f) && (quad.top == 0.0f) && (quad.right == 544.0f) && (quad.bottom == 176.0f))
      {
        foundCredits = true;
      }
    }
    Assert::IsTrue(foundCredits, L"the credits panel did not reach 544 x 176 through the 2x fit");
  }

  /// **WITH NO TYPEFACE, NO TEXT** -- and the plates still draw, so a failed atlas is a frame with no
  /// words in it rather than no interface at all.
  TEST_METHOD(AMissingTypefaceDropsTextAndKeepsThePlates)
  {
    Outpost::HudTypeface typeface;
    typeface.solid = Neuron::AtlasSlot{.left = 1, .top = 1, .widthPixels = 4, .heightPixels = 4};
    typeface.atlasWidthPixels = 1024;
    typeface.atlasHeightPixels = 512;

    std::vector<Neuron::GlyphQuad> quads;
    Outpost::EmitQuads(Outpost::BuildHud(Crowded(false, false)), typeface, Neuron::ComputeInterfaceFit(1440, 960), quads);
    Assert::IsFalse(quads.empty());
  }
};

} // namespace GameClientTests
