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
  // ADR-056: a sector the player holds is in its sight whole, unless it is suppressed; another's is not.
  TEST_METHOD(SeesAHeldSectorWhole)
  {
    Outpost::FogOfWar fog;
    fog.Reset(2000.0f);
    std::vector<Outpost::SectorView> sectors{
      {.id = 1, .minXMeters = -1000.0f, .maxXMeters = 0.0f, .minZMeters = -1000.0f, .maxZMeters = 1000.0f, .holder = PLAYER},
      {.id = 2, .minXMeters = 0.0f, .maxXMeters = 1000.0f, .minZMeters = -1000.0f, .maxZMeters = 1000.0f, .holder = ENEMY}};
    fog.Update({}, PLAYER, sectors);
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.xMeters = -990.0f, .zMeters = 990.0f}));
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.xMeters = -10.0f, .zMeters = -990.0f}));
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, fog.ShadeAt({.xMeters = 10.0f, .zMeters = 0.0f}));

    sectors[0].suppressed = true;
    fog.Update({}, PLAYER, sectors);
    Assert::AreEqual(Outpost::FogOfWar::SEEN_BEFORE_SHADE, fog.ShadeAt({.xMeters = -500.0f, .zMeters = 0.0f}));
  }

  // Interface plan 2, task UI1.3: on the ground, what was seen before is darker than the minimap shows it, short of what
  // was never seen, which keeps its shade under the ground mask's knee.
  TEST_METHOD(DarkensWhatWasSeenBeforeOnTheGround)
  {
    Assert::IsTrue(Outpost::FogOfWar::SEEN_SHADE < Outpost::FogOfWar::SEEN_BEFORE_SHADE);
    Assert::IsTrue(Outpost::FogOfWar::SEEN_BEFORE_SHADE < Outpost::FogOfWar::GROUND_SEEN_BEFORE_SHADE);
    Assert::IsTrue(Outpost::FogOfWar::GROUND_SEEN_BEFORE_SHADE <= Outpost::FogOfWar::NEVER_SEEN_SHADE,
                   L"the knee leaves never seen at its own shade");
  }

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

  // ADR-052: the revision moves on when the shades change, and only then, so the client copies them to the GPU only then.
  TEST_METHOD(CountsWhenItsShadesChange)
  {
    Outpost::FogOfWar fog;
    fog.Reset(2000.0f);
    const std::uint64_t reset = fog.Revision();
    fog.Update(std::vector{Seer(PLAYER, {}, 100.0f)}, PLAYER);
    const std::uint64_t seen = fog.Revision();
    Assert::IsTrue(seen != reset, L"a ship clears the ground it sees");
    fog.Update(std::vector{Seer(PLAYER, {}, 100.0f)}, PLAYER);
    Assert::AreEqual(seen, fog.Revision(), L"the same sight changes nothing");
    fog.Update(std::vector{Seer(PLAYER, {.xMeters = 300.0f}, 100.0f)}, PLAYER);
    Assert::IsTrue(fog.Revision() != seen, L"moving on dims what it left and clears what it reached");
    const std::uint64_t moved = fog.Revision();
    fog.Reset(2000.0f);
    Assert::IsTrue(fog.Revision() != moved, L"a new match starts over");
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

  // ADR-052: an update works out again only what moved, so a cell another entity or a held sector still sees stays clear
  // when one that saw it moves away or goes.
  TEST_METHOD(KeepsWhatIsStillSeen)
  {
    Outpost::FogOfWar fog;
    fog.Reset(2000.0f);
    std::vector<Outpost::EntityView> entities{Seer(PLAYER, {}, 100.0f), Seer(PLAYER, {.xMeters = 50.0f}, 100.0f)};
    entities[0].id = Outpost::EntityId{1};
    entities[1].id = Outpost::EntityId{2};
    const std::vector<Outpost::SectorView> sectors{
      {.id = 1, .minXMeters = -1000.0f, .maxXMeters = -500.0f, .minZMeters = -1000.0f, .maxZMeters = 1000.0f, .holder = PLAYER}};
    fog.Update(entities, PLAYER, sectors);
    entities[0].position = {.xMeters = -600.0f};
    fog.Update(entities, PLAYER, sectors);
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.xMeters = 10.0f}), L"the other still sees it");
    Assert::AreEqual(Outpost::FogOfWar::SEEN_BEFORE_SHADE, fog.ShadeAt({.xMeters = -80.0f}), L"left behind");
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.xMeters = -550.0f}), L"in sight and in the sector");

    entities.erase(entities.begin());
    fog.Update(entities, PLAYER, sectors);
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.xMeters = -550.0f}), L"the sector still sees it");
    Assert::AreEqual(Outpost::FogOfWar::SEEN_SHADE, fog.ShadeAt({.xMeters = 10.0f}));
    fog.Update(entities, PLAYER, {});
    Assert::AreEqual(Outpost::FogOfWar::SEEN_BEFORE_SHADE, fog.ShadeAt({.xMeters = -550.0f}), L"the sector lost");
  }

  // ADR-052: the rows whose shades changed are flagged, so that only they are copied to the GPU; a new grid flags every row.
  TEST_METHOD(SaysWhichRowsChanged)
  {
    Outpost::FogOfWar fog;
    fog.Reset(2000.0f);
    Assert::AreEqual(size_t{100}, fog.ChangedRows().size());
    Assert::IsTrue(std::ranges::all_of(fog.ChangedRows(), [](std::uint8_t _row) { return _row != 0; }), L"a new grid");
    fog.ClearChangedRows();
    // 100 m of sight at the center reaches rows 45 to 54, of 20 m each from -1,000 m.
    fog.Update(std::vector{Seer(PLAYER, {}, 100.0f)}, PLAYER);
    for (size_t row = 0; row < 100; ++row)
      Assert::AreEqual(row >= 45 && row <= 54, fog.ChangedRows()[row] != 0, std::to_wstring(row).c_str());
    fog.ClearChangedRows();
    fog.Update(std::vector{Seer(PLAYER, {}, 100.0f)}, PLAYER);
    Assert::IsTrue(std::ranges::none_of(fog.ChangedRows(), [](std::uint8_t _row) { return _row != 0; }), L"nothing changed");
  }

  // ADR-046: the player has seen an asteroid once it has seen any of it, and stays so; without fog it has seen the map.
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
