#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr std::uint64_t MATCH_SEED = 20260922;
constexpr std::size_t PLAYERS = 2;
/// **TWO MINUTES OF MATCH**, against the five `GameDesign.md` section 2 gives one. Long enough that the
/// fleet outgrows the first ring -- which is what makes this a test of ring assignment rather than of
/// three ships in a row -- and short enough that running it three times costs a fraction of a second.
constexpr std::uint32_t TICKS = 2400;

/// The tick a deliberate replacement lands on, so the run covers Q35's refund path rather than only the
/// completion path.
constexpr std::uint32_t REPLACEMENT_TICK = 910;

[[nodiscard]] std::int16_t WirePoint(std::int32_t _units) noexcept
{
  return Outpost::QuantizePosition(_units * Neuron::FIXED_ONE);
}

[[nodiscard]] Outpost::Command MoveTo(std::uint16_t _sequence, std::int32_t _x, std::int32_t _y,
                                      std::vector<Outpost::WireIdentity> _selection)
{
  return Outpost::Command{.sequence = _sequence,
                          .type = Outpost::CommandType::MoveTo,
                          .targetX = WirePoint(_x),
                          .targetY = WirePoint(_y),
                          .selection = std::move(_selection)};
}

[[nodiscard]] Outpost::Command BuildOrder(std::uint16_t _sequence, Outpost::DesignId _design)
{
  return Outpost::Command{.sequence = _sequence,
                          .type = Outpost::CommandType::Build,
                          .targetX = static_cast<std::int16_t>(_design),
                          .targetY = 0,
                          .selection = {}};
}

/// Every wire identity a player's SHIPS carry, in slot order -- which is what a client's hit test would
/// produce and is therefore the order the host actually receives. The station is left out because it
/// cannot move and an order naming it would prove nothing.
[[nodiscard]] std::vector<Outpost::WireIdentity> OwnedShips(const Outpost::World& _world, Outpost::PlayerId _player)
{
  std::vector<Outpost::WireIdentity> out;
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Outpost::Entity& entity = _world.EntityInSlot(slot);
    if ((entity.owner == _player) && (entity.design != Outpost::DesignId::Station))
    {
      out.push_back(Outpost::PackIdentity(entity.id.index, entity.id.generation));
    }
  }
  return out;
}

/// What one run produced, so the assertions below share one script rather than two copies of it.
struct MatchResult
{
  std::uint64_t hash = 0;
  std::size_t alive = 0;
  std::uint32_t creditsOfPlayerOne = 0;
  std::size_t shipsOfPlayerOne = 0;
};

/// **THE WHOLE MATCH, AS A FUNCTION.** A seed in, a state hash out. Everything the tick touches is
/// driven from here and nothing reads a clock, a socket or an address.
///
/// The script is written to cover what the milestone added rather than to be long: two players build
/// whenever their station is idle, move everything they own every fifty ticks, and one of them replaces
/// an item mid-build. That reaches entity creation, ring assignment over a growing fleet, arrival,
/// re-ordering a fleet that is already moving, and Q35's refund.
[[nodiscard]] MatchResult RunScriptedMatch()
{
  Outpost::World world;
  Outpost::CommandIntake intake;
  Outpost::BuildSystem build;

  for (const Outpost::Placement& placed : Outpost::GenerateLayout(MATCH_SEED, PLAYERS))
  {
    static_cast<void>(world.Create(placed.position, placed.heading, placed.design, placed.owner));
  }
  build.Begin(PLAYERS);

  std::uint16_t sequence = 0;
  for (std::uint32_t tick = 1; tick <= TICKS; ++tick)
  {
    // **INCOME, AT THE DESIGN'S OWN RATE.** `GameDesign.md` section 4 puts a running economy at about
    // fifteen credits a second, which is fifteen every twenty ticks. Mining is M2's and this is not it
    // -- it is the only way to give this script a fleet worth assigning ring slots to, and it drives
    // `BuildSystem::Grant`, which nothing else calls yet.
    if ((tick % Outpost::TICKS_PER_SECOND) == 0)
    {
      for (Outpost::PlayerId player = 1; player <= static_cast<Outpost::PlayerId>(PLAYERS); ++player)
      {
        build.Grant(player, 15);
      }
    }

    for (Outpost::PlayerId player = 1; player <= static_cast<Outpost::PlayerId>(PLAYERS); ++player)
    {
      // A STATION THAT IS IDLE STARTS SOMETHING. Ordering unconditionally would replace the item every
      // time and nothing would ever finish, which is what the first draft of this script did.
      if (!build.Item(player).active)
      {
        const Outpost::DesignId design = ((tick / 20) % 2 == 0) ? Outpost::DesignId::Miner : Outpost::DesignId::Fighter;
        static_cast<void>(intake.Apply(world, build, player, BuildOrder(++sequence, design)));
      }
    }

    // One deliberate replacement, so Q35's refund is inside the hash rather than beside it.
    if (tick == REPLACEMENT_TICK)
    {
      static_cast<void>(intake.Apply(world, build, 1, BuildOrder(++sequence, Outpost::DesignId::Fighter)));
    }

    if ((tick % 50) == 0)
    {
      for (Outpost::PlayerId player = 1; player <= static_cast<Outpost::PlayerId>(PLAYERS); ++player)
      {
        std::vector<Outpost::WireIdentity> selection = OwnedShips(world, player);
        if (!selection.empty())
        {
          const std::int32_t sign = (player == 1) ? 1 : -1;
          static_cast<void>(intake.Apply(world, build, player, MoveTo(++sequence, sign * 1500, sign * -900, std::move(selection))));
        }
      }
    }

    Outpost::Tick(world);
    build.Advance(world);
  }

  return MatchResult{.hash = Outpost::StateHash(world),
                     .alive = world.AliveCount(),
                     .creditsOfPlayerOne = build.Credits(1),
                     .shipsOfPlayerOne = OwnedShips(world, 1).size()};
}
} // namespace

/// `TechnicalDesign.md` section 8: **the most valuable test in the tree**, and the one that protects R16.
///
/// It runs a fixed tick count from a seed against a scripted order list and asserts the state hash. A
/// change here is either deliberate or it is a desynchronisation -- and a desynchronisation is the class
/// of defect that cannot be debugged from a report of what happened, because it is two machines quietly
/// disagreeing rather than one of them crashing.
///
/// **ADR-002 OWES THIS ON ALL FOUR CONFIGURATION AND PLATFORM PAIRS, BY HAND.** `AGENTS.md` section 6
/// guarantees nothing in CI ever will: CI compiles x64 and does not run ARM64 at all, and the whole
/// point of the hash is that two different code generators reach the same number.
TEST_CLASS(TheDeterminismTest)
{
public:
  /// **A CHANGE HERE IS EITHER DELIBERATE OR IT IS A DESYNCHRONISATION.** If this literal starts
  /// disagreeing without anybody editing the script above it, the tick has stopped being deterministic.
  ///
  /// Verified identical on Debug and Release, x64 and ARM64, on 2026-09-22. **MOVED DELIBERATELY ON
  /// 2026-09-23** from `0x37f846ed90b74ca1`, by M1.17: ships now turn as they fly and route around
  /// structures (`OpenQuestions.md` Q51, Q52). The new value was the same on all four pairs before it
  /// was pinned.
  TEST_METHOD(TheScriptedMatchHashesToItsPinnedValue)
  {
    Assert::AreEqual(0xc8b1069f59fa3f85ull, RunScriptedMatch().hash);
  }

  /// **RUN TWICE IN ONE PROCESS**, which catches the failures a pinned literal cannot: mutable static
  /// state, an allocator address reaching an outcome, a container whose iteration order depends on what
  /// was in memory before it.
  TEST_METHOD(TwoRunsInOneProcessAgree)
  {
    Assert::AreEqual(RunScriptedMatch().hash, RunScriptedMatch().hash);
  }

  /// **THE RUN HAS TO ACTUALLY EXERCISE SOMETHING.** A script that produced an empty world would hash
  /// stably and prove nothing, which is the failure `AGENTS.md` section 3 names about empty suites --
  /// and the first draft of this script did exactly that, by replacing the item every twenty ticks so
  /// that nothing ever finished.
  TEST_METHOD(TheScriptBuildsAFleetAndSpendsForIt)
  {
    const MatchResult result = RunScriptedMatch();

    Assert::IsTrue(result.alive > 2, L"nothing was ever built");
    Assert::IsTrue(result.shipsOfPlayerOne >= 7, L"the fleet is too small to have outgrown the first ring");
    Assert::IsTrue(result.creditsOfPlayerOne < Outpost::STARTING_CREDITS, L"nothing was ever spent");
  }
};

} // namespace GameLogicTests
