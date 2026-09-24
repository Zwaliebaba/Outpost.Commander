#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameCoreTests
{

namespace
{
[[nodiscard]] Neuron::Vec2 At(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = _x * Neuron::FIXED_ONE, .y = _y * Neuron::FIXED_ONE};
}

[[nodiscard]] Outpost::ModuleSiteVerdict Check(const std::vector<Outpost::PlacedModule>& _existing, Neuron::Vec2 _site,
                                               Outpost::DesignId _module = Outpost::DesignId::ModuleShipyardL1)
{
  return Outpost::CheckModuleSite(At(0, 0), Outpost::DesignId::Station, _existing, _site, _module);
}

[[nodiscard]] Outpost::PlacedModule Module(Outpost::WireIdentity _identity, std::int32_t _x, std::int32_t _y)
{
  return Outpost::PlacedModule{.identity = _identity, .position = At(_x, _y), .design = Outpost::DesignId::ModuleOreProcessorL1};
}

[[nodiscard]] int Code(Outpost::ModuleSiteFault _fault) noexcept
{
  return static_cast<int>(_fault);
}
} // namespace

/// M2.10. **The five refusals and the accept, each on its own**, against a station at the origin: a 220-unit
/// station and 90-unit frames (M2.10b), so a frame clears the station at 155 and another frame at 90.
TEST_CLASS(TheModuleSite)
{
public:
  /// The handoff's own plate: four frames at 354 from the station, 45 degrees off its arms (`design_handoff_meshes`
  /// section 3) -- the fourth fits beside three.
  TEST_METHOD(AFrameInsideTheRadiusAndClearOfEverythingIsLegal)
  {
    const std::vector<Outpost::PlacedModule> three{Module(1, 250, 250), Module(2, -250, 250), Module(3, -250, -250)};
    const Outpost::ModuleSiteVerdict verdict = Check(three, At(250, -250));
    Assert::IsTrue(verdict.Legal());
    Assert::AreEqual(Outpost::NO_WIRE_IDENTITY, verdict.blockedBy);
  }

  /// **A SHIP IS NOT A MODULE**, and neither is a station -- only a `ModuleFrame` design is placed.
  TEST_METHOD(ADesignThatIsNotAModuleIsRefused)
  {
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::NotAModule), Code(Check({}, At(300, 0), Outpost::DesignId::Miner).fault));
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::NotAModule), Code(Check({}, At(300, 0), Outpost::DesignId::Station).fault));
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::NotAModule), Code(Check({}, At(300, 0), static_cast<Outpost::DesignId>(200)).fault),
                     L"a design the table does not have");
  }

  /// **THE FIFTH IS REFUSED** against a cap of four, wherever it would go.
  TEST_METHOD(AFifthModuleIsRefused)
  {
    const std::vector<Outpost::PlacedModule> four{Module(1, 250, 250), Module(2, -250, 250), Module(3, -250, -250), Module(4, 250, -250)};
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::AtCapacity), Code(Check(four, At(0, 350)).fault));
    Assert::AreEqual(std::size_t{4}, Outpost::MAXIMUM_MODULES_PER_STATION);
  }

  /// **400 IS INSIDE AND A STEP PAST IT IS NOT** -- the radius is inclusive.
  TEST_METHOD(TheRadiusIsInclusiveAndAStepPastItIsRefused)
  {
    Assert::IsTrue(Check({}, At(400, 0)).Legal());
    Assert::IsTrue(Check({}, At(240, 320)).Legal(), L"a 3-4-5 point exactly on the radius");
    const Outpost::ModuleSiteVerdict past = Check({}, Neuron::Vec2{.x = (400 * Neuron::FIXED_ONE) + 1, .y = 0});
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::OutsideRadius), Code(past.fault));
  }

  /// **TOUCHING THE STATION IS CLEAR, AND A STEP CLOSER IS NOT**: 110 + 45 = 155.
  TEST_METHOD(AFrameOverlappingTheStationIsRefused)
  {
    Assert::IsTrue(Check({}, At(155, 0)).Legal());
    const Outpost::ModuleSiteVerdict inside = Check({}, Neuron::Vec2{.x = (155 * Neuron::FIXED_ONE) - 1, .y = 0});
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::OnStation), Code(inside.fault));
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::OnStation), Code(Check({}, At(0, 0)).fault));
  }

  /// **TOUCHING ANOTHER FRAME IS CLEAR, AND A STEP CLOSER IS NOT**: 45 + 45 = 90.
  TEST_METHOD(AFrameOverlappingAnotherModuleIsRefused)
  {
    const std::vector<Outpost::PlacedModule> one{Module(7, 300, 0)};
    Assert::IsTrue(Check(one, At(300, 90)).Legal());
    const Outpost::ModuleSiteVerdict overlap = Check(one, Neuron::Vec2{.x = 300 * Neuron::FIXED_ONE, .y = (90 * Neuron::FIXED_ONE) - 1});
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::OnModule), Code(overlap.fault));
    Assert::AreEqual(Outpost::WireIdentity{7}, overlap.blockedBy);
  }

  /// **THE BLOCKER IS THE LOWEST IDENTITY, IN ANY ORDER** (R16): a site overlapping two frames names the same one
  /// whichever way round they were listed, which is what lets two sides agree about it.
  TEST_METHOD(TheBlockerIsTheLowestIdentityWhateverTheOrder)
  {
    const std::vector<Outpost::PlacedModule> forward{Module(5, 300, 40), Module(9, 300, -40)};
    const std::vector<Outpost::PlacedModule> backward{Module(9, 300, -40), Module(5, 300, 40)};
    Assert::AreEqual(Outpost::WireIdentity{5}, Check(forward, At(300, 0)).blockedBy);
    Assert::AreEqual(Outpost::WireIdentity{5}, Check(backward, At(300, 0)).blockedBy);
  }

  /// **THE CHECKS RUN IN A FIXED ORDER**, so a site wrong in two ways reports the same fault on both sides: past the
  /// cap wins over outside the radius, and outside the radius over anything nearer.
  TEST_METHOD(TheFaultOrderIsFixed)
  {
    const std::vector<Outpost::PlacedModule> four{Module(1, 250, 250), Module(2, -250, 250), Module(3, -250, -250), Module(4, 250, -250)};
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::AtCapacity), Code(Check(four, At(5000, 0)).fault));
    Assert::AreEqual(Code(Outpost::ModuleSiteFault::NotAModule), Code(Check(four, At(0, 0), Outpost::DesignId::Fighter).fault));
  }

  /// **AN L1 IS PLACED AND AN L2 IS UPGRADED INTO** (M2.11, `OpenQuestions.md` Q54), and only within a kind.
  TEST_METHOD(EachFirstLevelUpgradesToItsOwnSecond)
  {
    Assert::IsTrue(Outpost::UpgradesTo(Outpost::DesignId::ModuleShipyardL1, Outpost::DesignId::ModuleShipyardL2));
    Assert::IsTrue(Outpost::UpgradesTo(Outpost::DesignId::ModuleOreProcessorL1, Outpost::DesignId::ModuleOreProcessorL2));
    Assert::IsFalse(Outpost::UpgradesTo(Outpost::DesignId::ModuleShipyardL1, Outpost::DesignId::ModuleOreProcessorL2));
    Assert::IsFalse(Outpost::UpgradesTo(Outpost::DesignId::ModuleShipyardL2, Outpost::DesignId::ModuleShipyardL1));

    Assert::IsTrue(Outpost::IsPlacedLevel(Outpost::DesignId::ModuleShipyardL1));
    Assert::IsTrue(Outpost::IsPlacedLevel(Outpost::DesignId::ModuleOreProcessorL1));
    Assert::IsFalse(Outpost::IsPlacedLevel(Outpost::DesignId::ModuleShipyardL2));
    Assert::IsFalse(Outpost::IsPlacedLevel(Outpost::DesignId::ModuleOreProcessorL2));
    Assert::IsFalse(Outpost::IsPlacedLevel(Outpost::DesignId::Miner));
  }

  /// **AN UPGRADE PAYS THE DIFFERENCE** (Q54): 700 - 400 and 600 - 350, so an L2 costs section 5's figure in all.
  TEST_METHOD(AnUpgradeCostsTheDifference)
  {
    Assert::AreEqual(std::uint32_t{300},
                     Outpost::UpgradeCostCredits(Outpost::DesignId::ModuleShipyardL1, Outpost::DesignId::ModuleShipyardL2));
    Assert::AreEqual(std::uint32_t{250},
                     Outpost::UpgradeCostCredits(Outpost::DesignId::ModuleOreProcessorL1, Outpost::DesignId::ModuleOreProcessorL2));
    Assert::AreEqual(std::uint32_t{0}, Outpost::UpgradeCostCredits(Outpost::DesignId::Miner, Outpost::DesignId::Fighter));
  }

  /// **EVERY MODULE IS INSIDE THE POINT DEFENSE** (ADR-015, Q63): the circle plus half a module frame is within its
  /// reach, so moving either figure past the other fails here.
  TEST_METHOD(TheBuildRadiusIsInsideThePointDefense)
  {
    Assert::IsTrue((Outpost::MODULE_BUILD_RADIUS_UNITS + (Outpost::Hull(Outpost::HullId::ModuleFrame).sizeUnits / 2)) <=
                   static_cast<int>(Outpost::Component(Outpost::ComponentId::PointDefense).rangeUnits));
  }
};

} // namespace GameCoreTests
