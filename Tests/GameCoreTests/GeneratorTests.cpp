#include "pch.h"

#include <array>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameCoreTests
{

namespace
{
/// The seed M0 to M2 run (`GameDesign.md` section 3), and the one the table below is pinned for.
constexpr std::uint64_t MATCH_SEED = 20260922;

/// Enough seeds that a constraint which only bites occasionally shows up here rather than in a match.
constexpr std::uint64_t SEEDS_SWEPT = 200;

constexpr std::size_t REGION_ASTEROIDS =
  Outpost::HOME_FIELD_ASTEROID_COUNT + (Outpost::CONTESTED_FIELD_COUNT * Outpost::CONTESTED_FIELD_ASTEROID_COUNT);

[[nodiscard]] std::int64_t Squared(std::int64_t _value)
{
  return _value * _value;
}

[[nodiscard]] std::int32_t WholeX(const Outpost::Placement& _placed)
{
  return _placed.position.x / Neuron::FIXED_ONE;
}

[[nodiscard]] std::int32_t WholeY(const Outpost::Placement& _placed)
{
  return _placed.position.y / Neuron::FIXED_ONE;
}

[[nodiscard]] std::int64_t WholeDistanceSquared(std::int32_t _ax, std::int32_t _ay, std::int32_t _bx, std::int32_t _by)
{
  return Squared(static_cast<std::int64_t>(_ax) - _bx) + Squared(static_cast<std::int64_t>(_ay) - _by);
}

/// One row of the checked-in table: which field, and where, in whole units.
struct PinnedRock
{
  Outpost::FieldKind field;
  std::int32_t x;
  std::int32_t y;
};

/// **SEED 20260922 AT TWO PLAYERS, AS FIRST GENERATED ON 2026-09-23.** A change to the draw order, the
/// stream, a constant or `Pcg32` moves these, and that is a different map for every match that has
/// ever been played on this seed -- so it fails here, by row, rather than presenting as rocks that moved.
/// If the move is deliberate, regenerate the table in the same commit and say why.
constexpr std::array<PinnedRock, REGION_ASTEROIDS> PINNED_REGION{{
  {Outpost::FieldKind::Home, -5400, 525},        {Outpost::FieldKind::Home, -5928, 1047},
  {Outpost::FieldKind::Home, -6693, -845},       {Outpost::FieldKind::Home, -5196, 677},
  {Outpost::FieldKind::Home, -4681, -190},       {Outpost::FieldKind::Home, -7037, 648},
  {Outpost::FieldKind::Home, -5196, 890},        {Outpost::FieldKind::Home, -5313, -440},
  {Outpost::FieldKind::Home, -4506, -69},        {Outpost::FieldKind::Home, -4971, 28},
  {Outpost::FieldKind::Contested, -1436, -2123}, {Outpost::FieldKind::Contested, -1500, -2809},
  {Outpost::FieldKind::Contested, -1239, -2411}, {Outpost::FieldKind::Contested, -2096, -3033},
  {Outpost::FieldKind::Contested, -2143, -2336}, {Outpost::FieldKind::Contested, -2144, -2871},
  {Outpost::FieldKind::Contested, -1382, 2594},  {Outpost::FieldKind::Contested, -1679, 3399},
  {Outpost::FieldKind::Contested, -993, 3041},   {Outpost::FieldKind::Contested, -1922, 3217},
  {Outpost::FieldKind::Contested, -1867, 2573},  {Outpost::FieldKind::Contested, -1609, 2648},
}};
} // namespace

/// M2.1. **The property that matters is that both sides get the same answer** (R23), and the two ways of
/// failing it are pinned separately: against a checked-in table, and against itself.
TEST_CLASS(TheGenerator)
{
public:
  /// `TechnicalDesign.md` section 8: the generator's output is pinned for a seed against a checked-in table.
  TEST_METHOD(TheMatchSeedProducesThePinnedRegion)
  {
    const std::vector<Outpost::Placement> region = Outpost::GenerateRegion(MATCH_SEED, 2);
    Assert::AreEqual(PINNED_REGION.size(), region.size());

    for (std::size_t index = 0; index < region.size(); ++index)
    {
      Assert::IsTrue(region[index].field == PINNED_REGION[index].field, L"a rock changed field");
      Assert::AreEqual(PINNED_REGION[index].x, WholeX(region[index]), L"a rock moved");
      Assert::AreEqual(PINNED_REGION[index].y, WholeY(region[index]), L"a rock moved");

      // WHOLE UNITS EXACTLY. The generator places on the whole-unit grid, so the fraction is zero and a
      // row that came back with one was produced by something other than this generator.
      Assert::AreEqual(0, region[index].position.x % Neuron::FIXED_ONE);
      Assert::AreEqual(0, region[index].position.y % Neuron::FIXED_ONE);
    }
  }

  /// **RUN TWICE, COMPARED WHOLE** -- every field of every row, at both player counts and across seeds,
  /// which is "byte-identical" asserted rather than assumed.
  TEST_METHOD(TheSameSeedProducesTheSameRegion)
  {
    for (const std::size_t players : {std::size_t{1}, std::size_t{2}, std::size_t{4}})
    {
      for (const std::uint64_t seed : {std::uint64_t{0}, MATCH_SEED, ~std::uint64_t{0}})
      {
        const std::vector<Outpost::Placement> first = Outpost::GenerateRegion(seed, players);
        const std::vector<Outpost::Placement> second = Outpost::GenerateRegion(seed, players);
        Assert::IsTrue(first == second, L"the generator disagreed with itself");
      }
    }
  }

  /// The seed is read now, which M1.5's layout deliberately did not.
  TEST_METHOD(ADifferentSeedIsADifferentField)
  {
    Assert::IsFalse(Outpost::GenerateRegion(MATCH_SEED, 2) == Outpost::GenerateRegion(MATCH_SEED + 1, 2));
  }

  /// **Q26's FIGURES, BY NAME**, and the count holds for every seed swept -- which is what says the attempt
  /// cap is never what stopped a field.
  TEST_METHOD(EveryRegionHoldsQ26sCount)
  {
    Assert::AreEqual(static_cast<std::size_t>(10), Outpost::HOME_FIELD_ASTEROID_COUNT);
    Assert::AreEqual(static_cast<std::size_t>(2), Outpost::CONTESTED_FIELD_COUNT);
    Assert::AreEqual(static_cast<std::size_t>(6), Outpost::CONTESTED_FIELD_ASTEROID_COUNT);
    Assert::AreEqual(6000, Outpost::ANCHOR_RADIUS_UNITS);

    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        std::size_t home = 0;
        std::size_t contested = 0;
        for (const Outpost::Placement& placed : Outpost::GenerateRegion(seed, players))
        {
          home += (placed.field == Outpost::FieldKind::Home) ? 1 : 0;
          contested += (placed.field == Outpost::FieldKind::Contested) ? 1 : 0;
        }
        Assert::AreEqual(Outpost::HOME_FIELD_ASTEROID_COUNT, home, L"a home field came back short");
        Assert::AreEqual(Outpost::CONTESTED_FIELD_COUNT * Outpost::CONTESTED_FIELD_ASTEROID_COUNT, contested,
                         L"a contested field came back short");
      }
    }
  }

  /// An asteroid is nobody's, has no heading (its drawn yaw is the client's, M2.4), and is not a design.
  TEST_METHOD(EveryRockIsAnUnownedAsteroid)
  {
    for (const Outpost::Placement& placed : Outpost::GenerateRegion(MATCH_SEED, 2))
    {
      Assert::IsTrue(placed.kind == Outpost::PlacedKind::Asteroid);
      Assert::IsTrue(placed.field != Outpost::FieldKind::None);
      Assert::AreEqual(static_cast<int>(Outpost::NO_PLAYER), static_cast<int>(placed.owner));
      Assert::AreEqual(0, static_cast<int>(placed.heading));
    }
  }

  TEST_METHOD(NoPlayersPlaceNothing)
  {
    Assert::AreEqual(static_cast<std::size_t>(0), Outpost::GenerateRegion(MATCH_SEED, 0).size());
  }
};

/// Where the rocks are allowed to be, over every seed swept. **These are the constraints, not the output**,
/// so they hold whatever the table says.
TEST_CLASS(TheGeneratedGeometry)
{
public:
  /// **BETWEEN 600 AND 1,500 FROM THE ANCHOR**: "within about 1,500" is `GameDesign.md` section 3, and 600
  /// keeps every rock off the 400-unit module ring and outside point defense.
  TEST_METHOD(AHomeFieldSitsInItsAnnulus)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      const Neuron::Vec2 anchor = Outpost::StartAnchor(players, 1);
      const std::int32_t anchorX = anchor.x / Neuron::FIXED_ONE;
      const std::int32_t anchorY = anchor.y / Neuron::FIXED_ONE;

      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        for (const Outpost::Placement& placed : Outpost::GenerateRegion(seed, players))
        {
          if (placed.field != Outpost::FieldKind::Home)
          {
            continue;
          }
          const std::int64_t fromAnchor = WholeDistanceSquared(WholeX(placed), WholeY(placed), anchorX, anchorY);
          Assert::IsTrue(fromAnchor >= Squared(Outpost::HOME_FIELD_INNER_RADIUS_UNITS), L"a home rock sits on the base");
          Assert::IsTrue(fromAnchor <= Squared(Outpost::HOME_FIELD_OUTER_RADIUS_UNITS), L"a home rock left its field");
        }
      }
    }
  }

  /// **TOWARD THE MIDDLE AND NOT A SECOND HOME FIELD.** Every contested rock is within its cluster's reach of
  /// the 1,500-to-3,500 band, and therefore at least 1,900 from the anchor -- beyond the home field's edge.
  TEST_METHOD(ContestedRocksSitTowardTheMiddle)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      const Neuron::Vec2 anchor = Outpost::StartAnchor(players, 1);
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        for (const Outpost::Placement& placed : Outpost::GenerateRegion(seed, players))
        {
          if (placed.field != Outpost::FieldKind::Contested)
          {
            continue;
          }
          const std::int64_t fromMiddle = WholeDistanceSquared(WholeX(placed), WholeY(placed), 0, 0);
          Assert::IsTrue(fromMiddle >= Squared(Outpost::CONTESTED_FIELD_NEAREST_UNITS - Outpost::CONTESTED_FIELD_RADIUS_UNITS));
          Assert::IsTrue(fromMiddle <= Squared(Outpost::CONTESTED_FIELD_FARTHEST_UNITS + Outpost::CONTESTED_FIELD_RADIUS_UNITS));

          const std::int64_t fromAnchor =
            WholeDistanceSquared(WholeX(placed), WholeY(placed), anchor.x / Neuron::FIXED_ONE, anchor.y / Neuron::FIXED_ONE);
          Assert::IsTrue(fromAnchor > Squared(Outpost::HOME_FIELD_OUTER_RADIUS_UNITS), L"a contested rock is in the home field");
        }
      }
    }
  }

  /// **NO TWO ROCKS WITHIN 150 ANYWHERE ON THE MAP**, including across the edges the copies meet at. M2.1
  /// pinned this with the rotations applied by hand; since M2.2 it is the field `GenerateField` returns.
  TEST_METHOD(RocksKeepTheirSpacingAcrossTheCopies)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        const std::vector<Outpost::Placement> field = Outpost::GenerateField(seed, players);
        for (std::size_t first = 0; first < field.size(); ++first)
        {
          for (std::size_t second = first + 1; second < field.size(); ++second)
          {
            Assert::IsTrue(WholeDistanceSquared(WholeX(field[first]), WholeY(field[first]), WholeX(field[second]), WholeY(field[second])) >=
                             Squared(Outpost::ASTEROID_SPACING_UNITS),
                           L"two rocks closer than the spacing");
          }
        }
      }
    }
  }

  /// Every rock is inside the region by half the spacing, and inside the play area.
  TEST_METHOD(EveryRockIsInsideItsRegionAndThePlayArea)
  {
    const std::int32_t halfExtent = Outpost::PLAY_AREA_HALF_EXTENT / Neuron::FIXED_ONE;
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        for (const Outpost::Placement& placed : Outpost::GenerateRegion(seed, players))
        {
          Assert::IsTrue(Outpost::InRegion(WholeX(placed), WholeY(placed), players, Outpost::ASTEROID_SPACING_UNITS / 2));
          Assert::IsTrue((WholeX(placed) > -halfExtent) && (WholeX(placed) < halfExtent));
          Assert::IsTrue((WholeY(placed) > -halfExtent) && (WholeY(placed) < halfExtent));
        }
      }
    }
  }
};

/// M2.2. **Every player's start is the same start** (`GameDesign.md` section 3), asserted at two and at four
/// players although the MVP runs only two -- `TechnicalDesign.md` section 8's requirement in full.
TEST_CLASS(TheSymmetry)
{
public:
  TEST_METHOD(TwoPlayersMakeTwoCopiesAndFourMakeFour)
  {
    Assert::AreEqual(std::size_t{0}, Outpost::FieldCopyCount(0));
    Assert::AreEqual(std::size_t{2}, Outpost::FieldCopyCount(1));
    Assert::AreEqual(std::size_t{2}, Outpost::FieldCopyCount(2));
    Assert::AreEqual(std::size_t{4}, Outpost::FieldCopyCount(3));
    Assert::AreEqual(std::size_t{4}, Outpost::FieldCopyCount(4));
    Assert::AreEqual(std::size_t{4}, Outpost::FieldCopyCount(8), L"a stress count gets the four-player field");

    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      Assert::AreEqual(REGION_ASTEROIDS * Outpost::FieldCopyCount(players), Outpost::GenerateField(MATCH_SEED, players).size());
    }
    Assert::AreEqual(std::size_t{0}, Outpost::GenerateField(MATCH_SEED, 0).size());
  }

  /// **THE REGION IS THE FIELD'S FIRST ROWS, UNCHANGED**, so the table M2.1 pinned still pins the map and
  /// copying cost the generator no draws.
  TEST_METHOD(TheFieldOpensWithTheRegion)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      const std::vector<Outpost::Placement> region = Outpost::GenerateRegion(MATCH_SEED, players);
      const std::vector<Outpost::Placement> field = Outpost::GenerateField(MATCH_SEED, players);
      for (std::size_t index = 0; index < region.size(); ++index)
      {
        Assert::IsTrue(field[index] == region[index], L"the field does not open with the region");
      }
    }
  }

  /// **THE 180-DEGREE COPY OF THE PINNED TABLE, BY NUMBER.** A negation of both coordinates and nothing else,
  /// so the second player's field is pinned exactly as the first's is.
  TEST_METHOD(TheSecondPlayersFieldIsThePinnedRegionNegated)
  {
    const std::vector<Outpost::Placement> field = Outpost::GenerateField(MATCH_SEED, 2);
    for (std::size_t index = 0; index < PINNED_REGION.size(); ++index)
    {
      const Outpost::Placement& copy = field[PINNED_REGION.size() + index];
      Assert::IsTrue(copy.field == PINNED_REGION[index].field, L"a copy changed field");
      Assert::AreEqual(-PINNED_REGION[index].x, WholeX(copy));
      Assert::AreEqual(-PINNED_REGION[index].y, WholeY(copy));
    }
  }

  /// **FOR EACH PLACED OBJECT, EXACTLY ONE COUNTERPART AT THE ROTATED POSITION** -- `M2.2`'s exit criterion,
  /// to the fixed-point bit and not to the unit. The rotation here is written out by hand rather than
  /// through `QuarterTurn`, so a wrong turn in the one the generator uses cannot pass by agreeing with
  /// itself.
  TEST_METHOD(EveryRockHasExactlyOneCounterpartOneStepRound)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      const std::size_t turnsPerStep = Outpost::ANCHOR_COUNT / Outpost::FieldCopyCount(players);
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        const std::vector<Outpost::Placement> field = Outpost::GenerateField(seed, players);
        for (const Outpost::Placement& placed : field)
        {
          Neuron::Fixed x = placed.position.x;
          Neuron::Fixed y = placed.position.y;
          for (std::size_t turn = 0; turn < turnsPerStep; ++turn)
          {
            const Neuron::Fixed turned = -y;
            y = x;
            x = turned;
          }

          std::size_t counterparts = 0;
          for (const Outpost::Placement& other : field)
          {
            if ((other.position.x == x) && (other.position.y == y))
            {
              ++counterparts;
              Assert::IsTrue(other.field == placed.field, L"a rock's counterpart is in another kind of field");
              Assert::IsTrue(other.kind == placed.kind);
            }
          }
          Assert::AreEqual(std::size_t{1}, counterparts, L"a rock without exactly one counterpart");
        }
      }
    }
  }

  /// **NOTHING SITS ON THE CENTER**, the one point every rotation fixes -- a rock there would be its own
  /// counterpart and would be placed once per copy. The region keeps half the spacing from every edge, so
  /// it cannot happen; this says so over the sweep.
  TEST_METHOD(NoRockLandsOnTheCenter)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        for (const Outpost::Placement& placed : Outpost::GenerateField(seed, players))
        {
          Assert::IsFalse((placed.position.x == 0) && (placed.position.y == 0), L"a rock on the center");
        }
      }
    }
  }

  /// **COPY k IS PLAYER k + 1's**: its home field sits in the annulus around that player's anchor, which is
  /// what lets a later step say whose home field a rock is in without measuring.
  TEST_METHOD(EachCopysHomeFieldSurroundsItsPlayersAnchor)
  {
    for (const std::size_t players : {std::size_t{2}, std::size_t{4}})
    {
      for (std::uint64_t seed = 0; seed < SEEDS_SWEPT; ++seed)
      {
        const std::vector<Outpost::Placement> field = Outpost::GenerateField(seed, players);
        for (std::size_t index = 0; index < field.size(); ++index)
        {
          if (field[index].field != Outpost::FieldKind::Home)
          {
            continue;
          }
          const Outpost::PlayerId player = static_cast<Outpost::PlayerId>((index / REGION_ASTEROIDS) + 1);
          const Neuron::Vec2 anchor = Outpost::StartAnchor(players, player);
          const std::int64_t fromAnchor =
            WholeDistanceSquared(WholeX(field[index]), WholeY(field[index]), anchor.x / Neuron::FIXED_ONE, anchor.y / Neuron::FIXED_ONE);
          Assert::IsTrue(fromAnchor >= Squared(Outpost::HOME_FIELD_INNER_RADIUS_UNITS), L"a copied home rock sits on a base");
          Assert::IsTrue(fromAnchor <= Squared(Outpost::HOME_FIELD_OUTER_RADIUS_UNITS), L"a copied home rock is not around its anchor");
        }
      }
    }
  }

  /// Run twice, compared whole, as the region is.
  TEST_METHOD(TheSameSeedProducesTheSameField)
  {
    for (const std::size_t players : {std::size_t{1}, std::size_t{2}, std::size_t{4}})
    {
      Assert::IsTrue(Outpost::GenerateField(MATCH_SEED, players) == Outpost::GenerateField(MATCH_SEED, players),
                     L"the field disagreed with itself");
    }
  }
};

/// The region's edges, which are exactly the lines the rotation copies across.
TEST_CLASS(TheRegion)
{
public:
  TEST_METHOD(AtTwoPlayersItIsTheNegativeHalf)
  {
    Assert::IsTrue(Outpost::InRegion(-100, 5000, 2, 0));
    Assert::IsTrue(Outpost::InRegion(-100, -5000, 2, 100));
    Assert::IsFalse(Outpost::InRegion(-99, 0, 2, 100));
    Assert::IsFalse(Outpost::InRegion(1, 0, 2, 0));
  }

  /// The quarter between the diagonals: `(-4000, 3000)` is 1,000 inside the edge along the axis-aligned
  /// measure and 707 across it, so it clears a 700 margin and not a 710 one.
  TEST_METHOD(AtFourPlayersItIsTheQuarterBetweenTheDiagonals)
  {
    Assert::IsTrue(Outpost::InRegion(-6000, 0, 4, 0));
    Assert::IsTrue(Outpost::InRegion(-4000, 3000, 4, 700));
    Assert::IsFalse(Outpost::InRegion(-4000, 3000, 4, 710));
    Assert::IsTrue(Outpost::InRegion(-4000, -3000, 4, 700));
    Assert::IsFalse(Outpost::InRegion(-3000, 3000, 4, 0), L"a point on the diagonal belongs to neither side");
    Assert::IsFalse(Outpost::InRegion(-1000, 5000, 4, 0), L"that is the next player's quarter");
  }
};

} // namespace GameCoreTests
