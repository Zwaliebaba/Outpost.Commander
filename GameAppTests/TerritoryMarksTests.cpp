#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId PLAYER{1};
constexpr Outpost::PlayerId ENEMY{2};
using Look = Outpost::TerritoryMarks::Look;
using Pattern = Outpost::TerritoryMarks::Pattern;

// Four sectors of 2,000 m in a square on a 4,000 m map, each node at its middle: the player's home in the south-west,
// the enemy's in the north-east, and the two between them free.
Outpost::Snapshot Square()
{
  Outpost::Snapshot snapshot{.player = PLAYER};
  snapshot.mapSizeMeters = 4000.0f;
  snapshot.structureTypes.push_back(
    {.structure = Outpost::StructureKind::Relay, .nameUtf8 = "Relay", .radiusMeters = 30.0f, .buildable = true});
  const auto sector = [](std::int32_t _id, float _west, float _south, std::vector<std::int32_t> _adjacent, Outpost::PlayerId _holder)
  {
    return Outpost::SectorView{.id = _id,
                               .minXMeters = _west,
                               .maxXMeters = _west + 2000.0f,
                               .minZMeters = _south,
                               .maxZMeters = _south + 2000.0f,
                               .node = {.xMeters = _west + 1000.0f, .zMeters = _south + 1000.0f},
                               .adjacent = std::move(_adjacent),
                               .holder = _holder};
  };
  snapshot.sectors = {sector(1, -2000.0f, -2000.0f, {2, 3}, PLAYER), sector(2, 0.0f, -2000.0f, {1, 4}, {}),
                      sector(3, -2000.0f, 0.0f, {1, 4}, {}), sector(4, 0.0f, 0.0f, {2, 3}, ENEMY)};
  return snapshot;
}

std::vector<Outpost::EntityView> Stations(const Outpost::Snapshot& _snapshot)
{
  return {{.id = Outpost::EntityId{1},
           .kind = Outpost::EntityKind::Structure,
           .owner = PLAYER,
           .structure = Outpost::StructureKind::CommandStation,
           .position = _snapshot.sectors[0].node,
           .radiusMeters = 45.0f},
          {.id = Outpost::EntityId{2},
           .kind = Outpost::EntityKind::Structure,
           .owner = ENEMY,
           .structure = Outpost::StructureKind::CommandStation,
           .position = _snapshot.sectors[3].node,
           .radiusMeters = 45.0f}};
}

std::vector<Outpost::TerritoryMarks::Line> LinesOf(const Outpost::TerritoryMarks& _marks, Look _look)
{
  std::vector<Outpost::TerritoryMarks::Line> lines;
  std::ranges::copy_if(_marks.lines, std::back_inserter(lines), [_look](const auto& _line) { return _line.look == _look; });
  return lines;
}
} // namespace

// Interface plan 2, task UI1.1: the territory drawn in the world, over the fog.
TEST_CLASS(TerritoryMarksTests)
{
public:
  // The lattice draws every side of every sector once: a square of four sectors has eight sides round it and four inside.
  TEST_METHOD(DrawsEachBorderOnce)
  {
    const Outpost::TerritoryMarks marks = Outpost::MarkTerritory(Square(), {});
    const std::vector<Outpost::TerritoryMarks::Line> lattice = LinesOf(marks, Look::Neutral);
    Assert::AreEqual(size_t{12}, lattice.size());
    for (const auto& line : lattice)
    {
      Assert::IsFalse(line.holder.IsValid());
      Assert::IsTrue(line.pattern == Pattern::Solid);
      const float length = std::hypot(line.to.xMeters - line.from.xMeters, line.to.zMeters - line.from.zMeters);
      Assert::AreEqual(2000.0f, length, 1e-3f, L"each side of a sector, not two sides joined");
    }
  }

  // A held sector is outlined inside its border in its holder's look, so that two holders' outlines lie either side of the
  // lattice where their sectors meet; a free sector has none.
  TEST_METHOD(OutlinesAHeldSectorInsideItsBorder)
  {
    const Outpost::TerritoryMarks marks = Outpost::MarkTerritory(Square(), {});
    const std::vector<Outpost::TerritoryMarks::Line> held = LinesOf(marks, Look::Held);
    Assert::AreEqual(size_t{8}, held.size(), L"the two homes, four sides each");
    const auto own = std::ranges::count(held, PLAYER, &Outpost::TerritoryMarks::Line::holder);
    Assert::AreEqual(std::ptrdiff_t{4}, own);
    constexpr float INSET = Outpost::TERRITORY_INSET_METERS;
    for (const auto& line : held)
    {
      const bool home = line.holder == PLAYER;
      for (const Outpost::PlanePosition end : {line.from, line.to})
      {
        // Each end is a corner of the outline, the inset in from both of its sector's sides.
        Assert::AreEqual(INSET, std::min(std::abs(end.xMeters), 2000.0f - std::abs(end.xMeters)), 1e-3f);
        Assert::AreEqual(INSET, std::min(std::abs(end.zMeters), 2000.0f - std::abs(end.zMeters)), 1e-3f);
        Assert::IsTrue(std::abs(end.xMeters) <= 2000.0f - INSET + 1e-3f && std::abs(end.zMeters) <= 2000.0f - INSET + 1e-3f,
                       L"inside the map's edge by the inset");
        Assert::IsTrue(std::abs(end.xMeters) >= INSET - 1e-3f && std::abs(end.zMeters) >= INSET - 1e-3f,
                       L"inside the sectors' shared sides by the inset");
        Assert::IsTrue(home ? (end.xMeters < 0.0f && end.zMeters < 0.0f) : (end.xMeters > 0.0f && end.zMeters > 0.0f),
                       L"in its own sector");
      }
    }
  }

  // A suppressed sector's outline is dashed and a cut-off one's dotted, so that neither rests on color; suppression,
  // which earns nothing, wins over being cut off. A sector the pirates guard is outlined in their look.
  TEST_METHOD(PatternsSayWhetherASectorIsSuppressedOrCutOff)
  {
    Outpost::Snapshot snapshot = Square();
    snapshot.sectors[1].holder = PLAYER;
    snapshot.sectors[1].cutOff = true;
    snapshot.sectors[3].suppressed = true;
    snapshot.sectors[2].guarded = true;
    const Outpost::TerritoryMarks marks = Outpost::MarkTerritory(snapshot, {});
    const auto patternOf = [&marks](Outpost::PlanePosition _inside)
    {
      std::vector<Pattern> patterns;
      for (const auto& line : marks.lines)
      {
        if (line.look != Look::Neutral && std::signbit(line.from.xMeters) == std::signbit(_inside.xMeters) &&
            std::signbit(line.from.zMeters) == std::signbit(_inside.zMeters))
          patterns.push_back(line.pattern);
      }
      return patterns;
    };
    for (const Pattern pattern : patternOf({.xMeters = -1000.0f, .zMeters = -1000.0f}))
      Assert::IsTrue(pattern == Pattern::Solid, L"home");
    for (const Pattern pattern : patternOf({.xMeters = 1000.0f, .zMeters = -1000.0f}))
      Assert::IsTrue(pattern == Pattern::Dotted, L"cut off");
    for (const Pattern pattern : patternOf({.xMeters = 1000.0f, .zMeters = 1000.0f}))
      Assert::IsTrue(pattern == Pattern::Dashed, L"suppressed");
    const std::vector<Outpost::TerritoryMarks::Line> guarded = LinesOf(marks, Look::Guarded);
    Assert::AreEqual(size_t{4}, guarded.size());
    Assert::IsTrue(std::ranges::all_of(guarded, [](const auto& _line) { return _line.holder == Outpost::PIRATES; }));

    snapshot.sectors[1].suppressed = true;
    for (const auto& line : Outpost::MarkTerritory(snapshot, {}).lines)
    {
      if (line.look == Look::Held && line.from.xMeters > 0.0f && line.from.zMeters < 0.0f)
        Assert::IsTrue(line.pattern == Pattern::Dashed, L"suppressed and cut off: it earns nothing");
    }
  }

  // Every node has a mark: its holder's, the pirates' while they guard it, and the player's where it could claim the node
  // now, which is exactly where the Relay's ghost would be green.
  TEST_METHOD(MarksTheNodesThePlayerCouldClaim)
  {
    Outpost::Snapshot snapshot = Square();
    std::vector<Outpost::EntityView> entities = Stations(snapshot);
    Outpost::TerritoryMarks marks = Outpost::MarkTerritory(snapshot, entities);
    Assert::AreEqual(size_t{4}, marks.nodes.size());
    Assert::IsTrue(marks.nodes[0].look == Look::Held && marks.nodes[0].holder == PLAYER);
    Assert::IsTrue(marks.nodes[1].look == Look::Claimable && marks.nodes[1].holder == PLAYER, L"next to home");
    Assert::IsTrue(marks.nodes[2].look == Look::Claimable);
    Assert::IsTrue(marks.nodes[3].look == Look::Held && marks.nodes[3].holder == ENEMY);
    for (size_t i = 0; i < snapshot.sectors.size(); ++i)
    {
      Assert::IsTrue(marks.nodes[i].position == snapshot.sectors[i].node);
      const bool ghost = Outpost::PlaceGhost(snapshot.structureTypes[0], snapshot.sectors[i].node, entities, snapshot.mapSizeMeters,
                                             snapshot.sectors, PLAYER, snapshot.nodeCap)
                           .valid;
      Assert::AreEqual(ghost, marks.nodes[i].look == Look::Claimable);
    }

    snapshot.sectors[2].guarded = true;
    snapshot.nodeCap = 2;
    entities.push_back({.id = Outpost::EntityId{3},
                        .kind = Outpost::EntityKind::Structure,
                        .owner = PLAYER,
                        .structure = Outpost::StructureKind::Relay,
                        .position = snapshot.sectors[1].node,
                        .radiusMeters = 30.0f,
                        .builtPermille = 100});
    marks = Outpost::MarkTerritory(snapshot, entities);
    Assert::IsTrue(marks.nodes[1].look == Look::Neutral, L"a Relay site takes its node, and the cap");
    Assert::IsTrue(marks.nodes[2].look == Look::Guarded && marks.nodes[2].holder == Outpost::PIRATES);
  }

  // A map without sectors has no territory to draw.
  TEST_METHOD(MarksNothingWithoutSectors)
  {
    Outpost::Snapshot snapshot = Square();
    snapshot.sectors.clear();
    const Outpost::TerritoryMarks marks = Outpost::MarkTerritory(snapshot, {});
    Assert::IsTrue(marks.lines.empty() && marks.nodes.empty());
  }
};
} // namespace GameAppTests
