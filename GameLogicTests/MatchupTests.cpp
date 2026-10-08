#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr std::uint32_t TICKS_PER_SECOND = 20;
// Matchups.json's second, 25 Small Mass Drivers against 10 Medium Lances.
constexpr std::uint32_t SWARM_AGAINST_LINE = 1;

std::vector<Outpost::Matchup> RepositoryMatchups()
{
  return Outpost::LoadMatchups(ReadRepositoryData("Matchups.json"), Outpost::LoadTuning(ReadRepositoryTuning()));
}

// A server on the repository's map and data set up for a battle matchup, as the factory sets one up: no fog of war, no
// pirates and no derelicts, and the matchup's fleets in place of the bases.
class MatchupBattle
{
public:
  explicit MatchupBattle(std::uint32_t _matchup, std::uint64_t _seed = 3)
    : m_server(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()), {.seed = _seed, .matchup = _matchup})
  {
    World().PlaceMatchup(RepositoryMatchups()[_matchup]);
  }

  [[nodiscard]] Outpost::Simulation& World() noexcept
  {
    return m_server.World();
  }

  [[nodiscard]] std::vector<const Outpost::Entity*> Warships(Outpost::PlayerId _owner)
  {
    std::vector<const Outpost::Entity*> ships;
    for (const Outpost::Entity& entity : World().Entities())
    {
      if (entity.owner == _owner && entity.kind == Outpost::EntityKind::Ship && entity.role == Outpost::ShipRole::Warship)
        ships.push_back(&entity);
    }
    return ships;
  }

private:
  Outpost::InProcessServer m_server;
};

Outpost::PlanePosition Middle(const std::vector<const Outpost::Entity*>& _ships)
{
  Outpost::PlanePosition sum{};
  for (const Outpost::Entity* ship : _ships)
    sum = {sum.xMeters + ship->position.xMeters, sum.zMeters + ship->position.zMeters};
  const auto count = static_cast<float>(_ships.size());
  return {sum.xMeters / count, sum.zMeters / count};
}
} // namespace

// Horizon §9 (ADR-083): the battle matchups, fought with no bases, the AI's battle behavior on both sides.
TEST_CLASS(MatchupTests)
{
public:
  // Matchups.json's five, each naming the tuning data's components; and what is not a matchup refused.
  TEST_METHOD(LoadsTheMatchups)
  {
    const std::vector<Outpost::Matchup> matchups = RepositoryMatchups();
    Assert::AreEqual(std::size_t{5}, matchups.size());
    Assert::AreEqual(25, matchups[SWARM_AGAINST_LINE].sides[0].front().count);
    Assert::AreEqual(2u, matchups[SWARM_AGAINST_LINE].sides[1].front().weapon.value, L"the Lance");
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const auto refused = [&tuning](std::string_view _json)
    { Assert::ExpectException<Neuron::Exception>([&] { (void)Outpost::LoadMatchups(_json, tuning); }); };
    refused(R"({"matchups": [{"name": "a", "distanceMeters": 700, "sides": [[{"hull": 9, "drive": 1, "weapon": 1, "count": 1}],
                                                                            [{"hull": 1, "drive": 1, "weapon": 1, "count": 1}]]}]})");
    refused(R"({"matchups": [{"name": "a", "distanceMeters": 700, "sides": [[{"hull": 1, "drive": 1, "weapon": 1, "count": 1}]]}]})");
    refused(R"({"matchups": [{"name": "a", "distanceMeters": 700, "sides": [[], [{"hull": 1, "drive": 1, "weapon": 1, "count": 1}]]}]})");
    refused(R"({"matchups": [{"name": "a", "distanceMeters": 700, "sides": [[{"hull": 1, "drive": 1, "weapon": 1, "count": 0}],
                                                                            [{"hull": 1, "drive": 1, "weapon": 1, "count": 1}]]}]})");
    refused(R"({"matchups": []})");
  }

  // Each side's fleet, its matchup's distance apart and facing the other, its ships never going back for repair; no base,
  // no pirate and no fog of war.
  TEST_METHOD(PlacesTheFleetsFacingEachOther)
  {
    MatchupBattle battle(SWARM_AGAINST_LINE);
    const std::vector<const Outpost::Entity*> swarm = battle.Warships(BLUE);
    const std::vector<const Outpost::Entity*> line = battle.Warships(RED);
    Assert::AreEqual(std::size_t{25}, swarm.size());
    Assert::AreEqual(std::size_t{10}, line.size());
    Assert::IsTrue(std::ranges::all_of(battle.World().Entities(),
                                       [](const Outpost::Entity& _entity) { return _entity.kind != Outpost::EntityKind::Structure; }),
                   L"no base and no pirate");
    Assert::IsTrue(
      std::ranges::all_of(swarm, [](const Outpost::Entity* _ship) { return _ship->retreat == Outpost::RetreatThreshold::Never; }));
    Assert::IsFalse(battle.World().BuildSnapshot(BLUE).fogOfWar);
    // The front rows are the distance apart; the rows behind them are further.
    const float apart = Outpost::Distance(Middle(swarm), Middle(line));
    Assert::IsTrue(apart > 700.0f && apart < 900.0f, std::format(L"{} m apart", apart).c_str());
    const Outpost::PlaneVector towardLine = Middle(line) - Middle(swarm);
    const float heading = swarm.front()->headingRadians;
    Assert::IsTrue((std::cos(heading) * towardLine.xMeters) + (std::sin(heading) * towardLine.zMeters) > 0.0f, L"facing the line");
  }

  // The AI's battle behavior on both sides fights it to its end: one side's warships all destroyed and the other the winner.
  TEST_METHOD(EndsWhenASidesWarshipsAreGone)
  {
    MatchupBattle battle(SWARM_AGAINST_LINE);
    std::array<Outpost::MatchupPlayer, 2> players{Outpost::MatchupPlayer(TICKS_PER_SECOND), Outpost::MatchupPlayer(TICKS_PER_SECOND)};
    for (std::uint32_t tick = 0; tick < Outpost::Simulation::MATCHUP_SECONDS * TICKS_PER_SECOND && !battle.World().MatchOver(); ++tick)
    {
      std::vector<Outpost::Command> commands;
      for (const Outpost::PlayerId player : {BLUE, RED})
      {
        for (Outpost::Command& command : players[player.value - 1].Update(battle.World().BuildSnapshot(player)))
        {
          command.player = player;
          commands.push_back(std::move(command));
        }
      }
      (void)battle.World().Tick(commands);
    }
    Assert::IsTrue(battle.World().MatchOver(), L"fought to an end");
    Assert::IsTrue(battle.World().Ending() == Outpost::MatchEnding::FleetDestroyed);
    const Outpost::PlayerId winner = battle.World().Winner();
    const Outpost::PlayerId loser{3 - winner.value};
    Assert::IsTrue(winner == BLUE || winner == RED);
    Assert::IsFalse(battle.Warships(winner).empty());
    Assert::IsTrue(battle.Warships(loser).empty());
  }

  // Fleets that never close end as a draw after MATCHUP_SECONDS.
  TEST_METHOD(EndsInADrawWhenTimeRunsOut)
  {
    MatchupBattle battle(SWARM_AGAINST_LINE);
    const std::uint64_t last = Outpost::Simulation::MATCHUP_SECONDS * TICKS_PER_SECOND;
    while (battle.World().CurrentTick() + 1 < last)
      (void)battle.World().Tick({});
    Assert::IsFalse(battle.World().MatchOver(), L"not before its time");
    (void)battle.World().Tick({});
    Assert::IsTrue(battle.World().MatchOver());
    Assert::IsTrue(battle.World().Ending() == Outpost::MatchEnding::TimeLimit);
    Assert::IsFalse(battle.World().Winner().IsValid(), L"a draw");
  }

  // The seed picks where they meet: two seeds, two places.
  TEST_METHOD(TheSeedPicksTheField)
  {
    MatchupBattle first(SWARM_AGAINST_LINE, 3);
    MatchupBattle second(SWARM_AGAINST_LINE, 4);
    Assert::IsTrue(Outpost::Distance(Middle(first.Warships(BLUE)), Middle(second.Warships(BLUE))) > 1.0f);
  }
};
} // namespace GameLogicTests
