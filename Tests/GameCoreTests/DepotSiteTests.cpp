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

[[nodiscard]] Outpost::Placement RockAt(std::int32_t _x, std::int32_t _y) noexcept
{
  return Outpost::Placement{.kind = Outpost::PlacedKind::Asteroid, .field = Outpost::FieldKind::Contested, .position = At(_x, _y)};
}

const std::vector<Neuron::Vec2> STATIONS{At(-6000, 0), At(6000, 0)};
const std::vector<Outpost::Placement> FIELD{RockAt(0, 3000)};
} // namespace

/// M3.9, `OpenQuestions.md` Q69. **Where a forward depot may go.**
TEST_CLASS(TheDepotSite)
{
public:
  /// Far from every station and near a rock: legal.
  TEST_METHOD(AForwardSiteNearARockIsLegal)
  {
    Assert::IsTrue(Outpost::CheckDepotSite(STATIONS, FIELD, {}, At(0, 2400), Outpost::DesignId::Depot) == Outpost::DepotSiteFault::None);
  }

  /// Each rule in its order: not a depot, the cap, a station, the rock, another depot.
  TEST_METHOD(EachRuleRefusesInItsOrder)
  {
    Assert::IsTrue(Outpost::CheckDepotSite(STATIONS, FIELD, {}, At(0, 2400), Outpost::DesignId::ModuleShipyardL1) ==
                   Outpost::DepotSiteFault::NotADepot);

    const std::vector<Neuron::Vec2> two{At(900, 0), At(-900, 0)};
    Assert::IsTrue(Outpost::CheckDepotSite(STATIONS, FIELD, two, At(0, 2400), Outpost::DesignId::Depot) ==
                   Outpost::DepotSiteFault::AtCapacity);

    // 1,999 from a station is too close; 2,000 is not.
    const std::vector<Outpost::Placement> nearStation{RockAt(-4000, 0)};
    Assert::IsTrue(Outpost::CheckDepotSite(STATIONS, nearStation, {}, At(-4001, 0), Outpost::DesignId::Depot) ==
                   Outpost::DepotSiteFault::NearStation);
    Assert::IsTrue(Outpost::CheckDepotSite(STATIONS, nearStation, {}, At(-4000, 0), Outpost::DesignId::Depot) ==
                   Outpost::DepotSiteFault::None);

    // 800 from the rock is near enough; 801 is not.
    Assert::IsTrue(Outpost::CheckDepotSite(STATIONS, FIELD, {}, At(0, 2200), Outpost::DesignId::Depot) == Outpost::DepotSiteFault::None);
    Assert::IsTrue(Outpost::CheckDepotSite(STATIONS, FIELD, {}, At(0, 2199), Outpost::DesignId::Depot) ==
                   Outpost::DepotSiteFault::FarFromRock);

    const std::vector<Neuron::Vec2> one{At(0, 2440)};
    Assert::IsTrue(Outpost::CheckDepotSite(STATIONS, FIELD, one, At(0, 2400), Outpost::DesignId::Depot) ==
                   Outpost::DepotSiteFault::OnDepot);
  }

  /// A depot is not a module: it does not count against the station's four and is not placed in its circle.
  TEST_METHOD(ADepotIsNotAModule)
  {
    Assert::IsTrue(Outpost::IsDepot(Outpost::DesignId::Depot));
    Assert::IsFalse(Outpost::IsModule(Outpost::DesignId::Depot));
    Assert::IsFalse(Outpost::IsDepot(Outpost::DesignId::ModuleOreProcessorL1));
    const Outpost::DerivedStats stats = Outpost::Derive(Outpost::DesignId::Depot);
    Assert::AreEqual(1500u, stats.hullPoints);
    Assert::AreEqual(300u, stats.cost);
    Assert::IsTrue(stats.acceptsOre);
    Assert::AreEqual(0u, stats.speedUnitsPerSecond, L"a depot does not move");
  }
};

} // namespace GameCoreTests
