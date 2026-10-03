#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId PLAYER{1};
constexpr Outpost::PlayerId ENEMY{2};

Outpost::EntityView Seer(Outpost::PlayerId _owner, Outpost::PlanePosition _position, float _sightMeters)
{
  return {.owner = _owner, .position = _position, .sightMeters = _sightMeters};
}
} // namespace

// ADR-024: the ground the player sees now is clear, what it saw before is dimmed, and what it never saw is dark.
TEST_CLASS(FogOfWarTests)
{
public:
  TEST_METHOD(StartsWithNothingSeen)
  {
    Outpost::FogOfWar fog;
    Assert::AreEqual(0u, fog.CellsPerSide(), L"no map yet");
    fog.Reset(2000.0f);
    Assert::AreEqual(100u, fog.CellsPerSide());
    Assert::AreEqual(size_t{10000}, fog.Shades().size(), L"a hundred by a hundred");
    Assert::IsTrue(fog.Origin() == Outpost::PlanePosition{.xMeters = -1000.0f, .zMeters = -1000.0f});
    Assert::IsTrue(std::ranges::all_of(fog.Shades(), [](float _shade) { return _shade == Outpost::FogOfWar::NEVER_SEEN_SHADE; }));
  }

  // Only the player's own entities clear the fog, each as far as it sees.
  TEST_METHOD(ClearsWhatItsOwnEntitiesSee)
  {
    Outpost::FogOfWar fog;
    fog.Reset(2000.0f);
    const std::vector<Outpost::EntityView> entities{Seer(PLAYER, {}, 100.0f), Seer(ENEMY, {.xMeters = 500.0f}, 300.0f),
                                                    Seer(PLAYER, {.xMeters = -500.0f}, 0.0f)};
    fog.Update(entities, PLAYER);
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({}));
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.xMeters = 85.0f}));
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, fog.ShadeAt({.xMeters = 115.0f}));
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, fog.ShadeAt({.xMeters = 500.0f}), L"an enemy's sight is not the player's");
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, fog.ShadeAt({.xMeters = -500.0f}), L"an entity that sees nothing");
  }

  // Ground once seen stays dimmed after the player's entities have gone, rather than going dark again.
  TEST_METHOD(DimsWhatItSawBefore)
  {
    Outpost::FogOfWar fog;
    fog.Reset(2000.0f);
    fog.Update(std::vector{Seer(PLAYER, {}, 100.0f)}, PLAYER);
    fog.Update(std::vector{Seer(PLAYER, {.zMeters = 600.0f}, 100.0f)}, PLAYER);
    Assert::AreEqual(Outpost::FogOfWar::SEEN_BEFORE_SHADE, fog.ShadeAt({}));
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.zMeters = 600.0f}));
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, fog.ShadeAt({.zMeters = 300.0f}));

    fog.Reset(2000.0f);
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, fog.ShadeAt({.zMeters = 600.0f}), L"a new match starts unseen");
  }

  // Sight that reaches past the map's edge stays on the grid, and off the map counts as never seen.
  TEST_METHOD(KeepsToTheMap)
  {
    Outpost::FogOfWar fog;
    fog.Reset(2000.0f);
    fog.Update(std::vector{Seer(PLAYER, {.xMeters = 990.0f, .zMeters = -990.0f}, 300.0f)}, PLAYER);
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.xMeters = 999.0f, .zMeters = -999.0f}));
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, fog.ShadeAt({.xMeters = 1001.0f, .zMeters = -990.0f}));
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, fog.ShadeAt({.xMeters = -990.0f, .zMeters = 990.0f}));
  }

  // ADR-045: the player has seen an asteroid once it has seen any of it, and stays so; without fog it has seen the map.
  TEST_METHOD(SaysWhetherAnyOfACircleWasSeen)
  {
    Outpost::FogOfWar fog;
    Assert::IsTrue(fog.HasSeen({.xMeters = 700.0f}, 45.0f), L"no fog of war");
    fog.Reset(2000.0f);
    Assert::IsFalse(fog.HasSeen({}, 45.0f));
    fog.Update(std::vector{Seer(PLAYER, {}, 100.0f)}, PLAYER);
    Assert::IsTrue(fog.HasSeen({.xMeters = 60.0f}, 45.0f), L"its middle in sight");
    Assert::IsTrue(fog.HasSeen({.xMeters = 130.0f}, 45.0f), L"its near edge in sight");
    Assert::IsFalse(fog.HasSeen({.xMeters = 200.0f}, 45.0f), L"none of it in sight");
    fog.Update(std::vector<Outpost::EntityView>{}, PLAYER);
    Assert::IsTrue(fog.HasSeen({.xMeters = 60.0f}, 45.0f), L"seen before");
  }
};
} // namespace GameAppTests
