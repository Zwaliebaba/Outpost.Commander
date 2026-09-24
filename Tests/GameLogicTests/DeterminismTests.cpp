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

/// **THE TICK THE MINERS ARE ORDERED ELSEWHERE** (M2.6), so abandoning the loop mid-cycle, cargo aboard, is
/// inside the hash -- and the mine orders after it pick the cycle back up.
constexpr std::uint32_t ABANDON_TICK = 1500;

/// **M3.2: THE FIGHT.** Each player's fighters are ordered to attack the other's first fighter, which puts
/// targeting, arcs, turning to bear, remainders and the standoff arc inside the hash. The two fleets are about
/// 3,500 units apart at this tick, so they close, stand off and fire for most of the last forty seconds.
constexpr std::uint32_t FIGHT_TICK = 1600;

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

/// A player's ships of one design, in slot order -- the fleet split the script orders separately since
/// M2.6, because a move order ends a mine order and a script that moved the miners every fifty ticks would
/// never let one finish a cycle.
[[nodiscard]] std::vector<Outpost::WireIdentity> OwnedOf(const Outpost::World& _world, Outpost::PlayerId _player, Outpost::DesignId _design,
                                                         bool _onlyIdleMiners)
{
  std::vector<Outpost::WireIdentity> out;
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Outpost::Entity& entity = _world.EntityInSlot(slot);
    if ((entity.owner != _player) || (entity.design != _design))
    {
      continue;
    }
    if (_onlyIdleMiners && (_world.MineInSlot(slot).phase != Outpost::MiningPhase::None))
    {
      continue;
    }
    out.push_back(Outpost::PackIdentity(entity.id.index, entity.id.generation));
  }
  return out;
}

[[nodiscard]] Outpost::Command AttackCommand(std::uint16_t _sequence, Outpost::WireIdentity _target,
                                             std::vector<Outpost::WireIdentity> _selection)
{
  Outpost::Command command{.sequence = _sequence, .type = Outpost::CommandType::Attack, .selection = std::move(_selection)};
  command.AimAt(_target);
  return command;
}

[[nodiscard]] Outpost::Command MineCommand(std::uint16_t _sequence, std::uint16_t _rock, std::vector<Outpost::WireIdentity> _selection)
{
  Outpost::Command command{.sequence = _sequence, .type = Outpost::CommandType::Mine, .selection = std::move(_selection)};
  command.AimAtRock(_rock);
  return command;
}

/// What one run produced, so the assertions below share one script rather than two copies of it.
struct MatchResult
{
  std::uint64_t hash = 0;
  std::size_t alive = 0;
  std::uint32_t creditsOfPlayerOne = 0;
  std::size_t shipsOfPlayerOne = 0;
  std::uint64_t deliveredMilliOre = 0;

  /// Hull points missing across every entity at the end: proof the fight was in the hash rather than beside it.
  std::uint64_t hullLost = 0;
};

/// **THE WHOLE MATCH, AS A FUNCTION.** A seed in, a state hash out. Everything the tick touches is
/// driven from here and nothing reads a clock, a socket or an address.
///
/// The script is written to cover what the milestone added rather than to be long: two players build
/// whenever their station is idle, move everything they own every fifty ticks, and one of them replaces
/// an item mid-build. That reaches entity creation, ring assignment over a growing fleet, arrival,
/// re-ordering a fleet that is already moving, and Q35's refund.
///
/// **SINCE M2.6 THE MINERS MINE.** Every fifty ticks, offset from the fleet's moves, each player's idle
/// miners are sent to one of their ten home rocks -- a different one each time, so the unload query runs
/// from many places -- and the fighters alone take the fleet moves. At `ABANDON_TICK` everything is moved,
/// miners included. That reaches the whole five-state loop, the unload query, abandonment with cargo aboard
/// and a standing order picked back up.
[[nodiscard]] MatchResult RunScriptedMatch()
{
  Outpost::World world;
  Outpost::CommandIntake intake;
  Outpost::BuildSystem build;
  Outpost::MiningSystem mining;
  Outpost::WeaponSystem weapons;
  Outpost::Economy economy;
  std::uint64_t deliveredMilliOre = 0;

  world.SetField(Outpost::GenerateField(MATCH_SEED, PLAYERS));
  const std::size_t regionRocks = world.Field().size() / Outpost::FieldCopyCount(PLAYERS);

  for (const Outpost::Placement& placed : Outpost::GenerateLayout(MATCH_SEED, PLAYERS))
  {
    static_cast<void>(world.Create(placed.position, placed.heading, placed.design, placed.owner));
  }
  build.Begin(PLAYERS);

  std::uint16_t sequence = 0;
  for (std::uint32_t tick = 1; tick <= TICKS; ++tick)
  {
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

    if ((tick % 50) == 25)
    {
      for (Outpost::PlayerId player = 1; player <= static_cast<Outpost::PlayerId>(PLAYERS); ++player)
      {
        std::vector<Outpost::WireIdentity> idle = OwnedOf(world, player, Outpost::DesignId::Miner, true);
        if (!idle.empty())
        {
          // Player k's home field is copy k - 1 of the region (M2.2), and its first ten rows are the home
          // field (M2.1).
          const std::size_t rock =
            (static_cast<std::size_t>(player - 1) * regionRocks) + ((tick / 50) % Outpost::HOME_FIELD_ASTEROID_COUNT);
          static_cast<void>(intake.Apply(world, build, player, MineCommand(++sequence, static_cast<std::uint16_t>(rock), std::move(idle))));
        }
      }
    }

    // THE FLEET MOVES STOP AT THE FIGHT: a move order ends an attack order (M3.2), so moving the fighters every
    // fifty ticks would call the fight off fifty ticks after it was ordered.
    if (((tick % 50) == 0) && (tick < FIGHT_TICK))
    {
      for (Outpost::PlayerId player = 1; player <= static_cast<Outpost::PlayerId>(PLAYERS); ++player)
      {
        std::vector<Outpost::WireIdentity> selection =
          (tick == ABANDON_TICK) ? OwnedShips(world, player) : OwnedOf(world, player, Outpost::DesignId::Fighter, false);
        if (!selection.empty())
        {
          const std::int32_t sign = (player == 1) ? 1 : -1;
          static_cast<void>(intake.Apply(world, build, player, MoveTo(++sequence, sign * 1500, sign * -900, std::move(selection))));
        }
      }
    }

    // M3.2's FIGHT: each side's fighters on the other's first fighter.
    if (tick == FIGHT_TICK)
    {
      for (Outpost::PlayerId player = 1; player <= static_cast<Outpost::PlayerId>(PLAYERS); ++player)
      {
        const Outpost::PlayerId enemy = (player == 1) ? 2 : 1;
        std::vector<Outpost::WireIdentity> fighters = OwnedOf(world, player, Outpost::DesignId::Fighter, false);
        const std::vector<Outpost::WireIdentity> targets = OwnedOf(world, enemy, Outpost::DesignId::Fighter, false);
        if (!fighters.empty() && !targets.empty())
        {
          static_cast<void>(intake.Apply(world, build, player, AttackCommand(++sequence, targets.front(), std::move(fighters))));
        }
      }
    }

    Outpost::Tick(world);
    weapons.Advance(world, tick);
    mining.Advance(world);
    for (const Outpost::OreDelivery& delivery : mining.Deliveries())
    {
      deliveredMilliOre += delivery.milliOre;
    }

    // **INCOME IS MINED SINCE M2.7.** Until then the script granted fifteen credits a second by hand, the
    // design's running rate, because nothing delivered ore; now the miners' unloads are the only income
    // there is, as in a match.
    economy.Credit(mining.Deliveries(), world, build);
    build.Advance(world);
  }

  std::uint64_t hullLost = 0;
  for (std::size_t slot = 0; slot < world.SlotCount(); ++slot)
  {
    if (world.IsSlotAlive(slot))
    {
      const Outpost::Entity& entity = world.EntityInSlot(slot);
      hullLost += Outpost::Derive(entity.design).hullPoints - entity.hullRemaining;
    }
  }

  return MatchResult{.hash = Outpost::MatchHash(world, build, economy),
                     .alive = world.AliveCount(),
                     .creditsOfPlayerOne = build.Credits(1),
                     .shipsOfPlayerOne = OwnedShips(world, 1).size(),
                     .deliveredMilliOre = deliveredMilliOre,
                     .hullLost = hullLost};
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
  /// **M3.2: THE SCRIPT FIGHTS**, so the pin below covers targeting, arcs and the remainders. A hash that stopped
  /// seeing damage would still be a hash, and this is what says it is not.
  TEST_METHOD(TheScriptedMatchHasAFight)
  {
    const MatchResult result = RunScriptedMatch();
    Logger::WriteMessage((std::wstring{L"SCRIPTED MATCH hull lost: "} + std::to_wstring(result.hullLost) + L"\n").c_str());
    Assert::IsTrue(result.hullLost > 0, L"nothing in the scripted match took damage");
  }

  /// **A CHANGE HERE IS EITHER DELIBERATE OR IT IS A DESYNCHRONISATION.** If this literal starts
  /// disagreeing without anybody editing the script above it, the tick has stopped being deterministic.
  ///
  /// **MOVED DELIBERATELY FOUR TIMES, AND EVERY MOVE IS A RULE THAT CHANGED.** From `0x37f846ed90b74ca1` --
  /// verified identical on Debug and Release, x64 and ARM64, on 2026-09-22 -- M2.6 moved it because the
  /// script began to mine (`0xc8f7f00e056d4d46`), and M2.7 because its income became what the miners
  /// deliver (`0x18e094912655348f`, computed off Windows under g++ and clang). On a branch beside those,
  /// M1.17 made ships turn as they fly and route around structures and each other (`OpenQuestions.md`
  /// Q59, Q60, Q61), which gave `0xc8b1069f59fa3f85` without the mining. **This value is the two together**,
  /// taken when the branch merged, and it was the same on all four MSVC pairs before it was pinned -- which
  /// is also the four-pair run M2.7's value was still owed.
  ///
  /// **MOVED A FIFTH TIME BY THE 2026-09-23 REVIEW (M6), AND NOT BY A RULE**: the hash widened. It now folds
  /// hull points, owner and the mine order per entity, and the script's result is `MatchHash` -- credits,
  /// income owed and the item building too -- where it was `StateHash` over the world alone, which could not
  /// see M3's damage arithmetic diverge. The script is unchanged; `0x073f1184808b252a` was its value under the
  /// old hash. **Computed off Windows only**, identically under g++ -O1 and -O3 and clang -O0 and -O2; the
  /// four MSVC pairs are owed, once, at M3's entry (ADR-002).
  ///
  /// **AND A SIXTH TIME THE SAME DAY, BY A RULE** (`OpenQuestions.md` Q62): the home field moved to 1,200-2,000
  /// units of the anchor and a rock yields to one miner a tick. `0xfc22fd27ca6ad39e` was confirmed on `Debug|x64`
  /// in CI before this; this value is g++ and clang's, and the four pairs are owed with it at M3's entry.
  /// **They were run at M3.1, 2026-09-24**: `0x4c8b850e5dec326e` passed on Debug and Release, x64 and ARM64.
  ///
  /// **A SEVENTH TIME AT M3.2, BY A RULE AND A WIDER HASH TOGETHER**: the tick fires weapons (ADR-004, ADR-014,
  /// Q67, Q68, Q78), the hash folds each ship's weapon remainders and attack order, and the script now fights
  /// from `FIGHT_TICK`, where it stops the fleet moves that would call the fight off. The same on all four MSVC
  /// pairs before it was pinned.
  TEST_METHOD(TheScriptedMatchHashesToItsPinnedValue)
  {
    Assert::AreEqual(0x2664e2cf4dcbaf7full, RunScriptedMatch().hash);
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

    // M2.6: WHOLE HOLDS, AND MANY OF THEM. A script whose miners never finished a cycle would hash stably
    // and pin nothing about the loop.
    Assert::IsTrue(result.deliveredMilliOre >= 20u * 100u * Outpost::MILLI_ORE_PER_ORE, L"the miners barely mined");
  }
};

} // namespace GameLogicTests
