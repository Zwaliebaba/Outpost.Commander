#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
// The narrowest of the three rock meshes reaches 0.60 of its radius on the ground, and a Small hull's footprint is 16 m
// across: what GameClient passes.
constexpr float REACH_SHARE = 0.6f;
constexpr float GAP_METERS = 16.0f;
// The map's fields run from 150 m to 180 m in radius.
constexpr std::array<float, 2> RADII{150.0f, 180.0f};
constexpr std::uint32_t FIELDS = 200;

bool Same(const Outpost::FieldRock& _a, const Outpost::FieldRock& _b)
{
  return _a.xMeters == _b.xMeters && _a.zMeters == _b.zMeters && _a.radiusMeters == _b.radiusMeters && _a.turnRadians == _b.turnRadians;
}

std::vector<Outpost::FieldRock> RocksOf(const Outpost::FieldLayout& _layout)
{
  std::vector<Outpost::FieldRock> rocks{_layout.center};
  rocks.insert(rocks.end(), _layout.ring.begin(), _layout.ring.end());
  return rocks;
}
} // namespace

TEST_CLASS(FieldLayoutTests)
{
public:
  // Interface plan 2, task UI5.2: a field is laid out from its identifier, the same every time.
  TEST_METHOD(LaysOutAFieldTheSameEveryTime)
  {
    const Outpost::FieldLayout first = Outpost::FieldLayout::Of(Outpost::EntityId{7}, 180.0f, REACH_SHARE, GAP_METERS);
    const Outpost::FieldLayout again = Outpost::FieldLayout::Of(Outpost::EntityId{7}, 180.0f, REACH_SHARE, GAP_METERS);
    const std::vector<Outpost::FieldRock> rocks = RocksOf(first);
    const std::vector<Outpost::FieldRock> rocksAgain = RocksOf(again);
    Assert::AreEqual(rocks.size(), rocksAgain.size());
    for (std::size_t i = 0; i < rocks.size(); ++i)
      Assert::IsTrue(Same(rocks[i], rocksAgain[i]));
  }

  // Two fields look apart, and among the map's fields every ring of five to eight shows.
  TEST_METHOD(LaysOutEachFieldItsOwnWay)
  {
    const Outpost::FieldLayout one = Outpost::FieldLayout::Of(Outpost::EntityId{7}, 180.0f, REACH_SHARE, GAP_METERS);
    const Outpost::FieldLayout other = Outpost::FieldLayout::Of(Outpost::EntityId{8}, 180.0f, REACH_SHARE, GAP_METERS);
    Assert::IsFalse(Same(one.center, other.center));
    Assert::IsFalse(Same(one.ring.front(), other.ring.front()));

    std::array<bool, 4> seen{};
    for (std::uint32_t field = 1; field <= FIELDS; ++field)
    {
      const Outpost::FieldLayout layout = Outpost::FieldLayout::Of(Outpost::EntityId{field}, 180.0f, REACH_SHARE, GAP_METERS);
      // The ring's own rocks are a fifth of the field's radius or more, and its fillers an eighth or less.
      const auto own = std::ranges::count_if(layout.ring, [](const Outpost::FieldRock& _rock) { return _rock.radiusMeters >= 35.9f; });
      Assert::IsTrue(own >= 5 && own <= 8, std::format(L"field {} has {} rocks in its ring", field, own).c_str());
      seen[static_cast<std::size_t>(own - 5)] = true;
    }
    Assert::IsTrue(std::ranges::all_of(seen, std::identity{}), L"rings of five, six, seven and eight");
  }

  // Every rock lies inside the field's circle, which is what the server blocks.
  TEST_METHOD(KeepsEveryRockInsideItsCircle)
  {
    for (const float radius : RADII)
    {
      for (std::uint32_t field = 1; field <= FIELDS; ++field)
      {
        for (const Outpost::FieldRock& rock : RocksOf(Outpost::FieldLayout::Of(Outpost::EntityId{field}, radius, REACH_SHARE, GAP_METERS)))
        {
          Assert::IsTrue(rock.radiusMeters > 0.0f);
          Assert::IsTrue(std::hypot(rock.xMeters, rock.zMeters) + rock.radiusMeters <= radius + 0.01f,
                         std::format(L"field {} of {} m", field, radius).c_str());
        }
      }
    }
  }

  // No two neighbors on the ring leave a gap wider than a Small hull between their least reaches, the last and the first
  // included, so the ring reads as closed.
  TEST_METHOD(LeavesNoGapWiderThanASmallHull)
  {
    std::size_t filled = 0;
    for (const float radius : RADII)
    {
      for (std::uint32_t field = 1; field <= FIELDS; ++field)
      {
        const Outpost::FieldLayout layout = Outpost::FieldLayout::Of(Outpost::EntityId{field}, radius, REACH_SHARE, GAP_METERS);
        for (std::size_t i = 0; i < layout.ring.size(); ++i)
        {
          const Outpost::FieldRock& from = layout.ring[i];
          const Outpost::FieldRock& to = layout.ring[(i + 1) % layout.ring.size()];
          const float gap =
            std::hypot(to.xMeters - from.xMeters, to.zMeters - from.zMeters) - (REACH_SHARE * (from.radiusMeters + to.radiusMeters));
          Assert::IsTrue(gap <= GAP_METERS + 0.01f,
                         std::format(L"field {} of {} m: {:.1f} m after rock {}", field, radius, gap, i).c_str());
        }
        filled += layout.ring.size() > 8 ? 1 : 0;
      }
    }
    Assert::IsTrue(filled > 0, L"some rings need fillers");
  }

  // A rock's least reach on the ground, over a whole turn: a 2 x 1 block reaches 0.5 across, a square 1 every way.
  TEST_METHOD(MeasuresTheNarrowestReach)
  {
    const auto block = [](float _halfX, float _halfZ)
    {
      std::vector<Neuron::MeshVertex> vertices;
      for (const float x : {-_halfX, _halfX})
      {
        for (const float z : {-_halfZ, _halfZ})
          vertices.push_back({.position = {x, 0.3f, z}, .normal = {0.0f, 1.0f, 0.0f}});
      }
      return vertices;
    };
    Assert::AreEqual(0.5f, Outpost::FieldLayout::NarrowestReach(block(1.0f, 0.5f)), 0.0001f);
    Assert::AreEqual(1.0f, Outpost::FieldLayout::NarrowestReach(block(1.0f, 1.0f)), 0.0001f);
    Assert::AreEqual(0.0f, Outpost::FieldLayout::NarrowestReach({}));
  }
};
} // namespace GameAppTests
