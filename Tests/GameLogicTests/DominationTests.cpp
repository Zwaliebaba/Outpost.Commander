#include "pch.h"
#include "TerritoryMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = TerritoryMatch::BLUE;
constexpr Outpost::PlayerId RED = TerritoryMatch::RED;
// The repository map's sectors (ADR-036): B1 east of Blue's home, A2 north of it, and B2 between them.
constexpr std::int32_t B1 = 2;
constexpr std::int32_t A2 = 6;
constexpr std::int32_t B2 = 7;
// The repository's tuning data: 1,000 tickets, and every 3 seconds one for each
// node behind, whatever the map's nodes (ADR-057).
constexpr std::int32_t STARTING_TICKETS = 1000;
constexpr std::int32_t DRAIN_PER_NODE = 1;
constexpr std::uint32_t DRAIN_TICKS = 3 * TerritoryMatch::TICKS_PER_SECOND;
} // namespace

TEST_CLASS(DominationTests)
{
public:
  // ADR-057 decisions 4 and 7: each player starts with 1,000 tickets, and both players see both, with the drain's
  // numbers from the tuning data, 1 ticket a node every 3 seconds, which the HUD tells the drain by.
  TEST_METHOD(EachPlayerStartsWithItsTickets)
  {
    TerritoryMatch match;
    Assert::AreEqual(STARTING_TICKETS, match.SimulationState().Tickets(BLUE));
    Assert::AreEqual(STARTING_TICKETS, match.SimulationState().Tickets(RED));
    const Outpost::Snapshot red = match.SimulationState().BuildSnapshot(RED);
    Assert::AreEqual(1000, red.startingTickets);
    Assert::AreEqual(3.0, red.drainIntervalSeconds);
    Assert::AreEqual(1, red.drainTicketsPerNodeDifference);
    Assert::IsTrue(red.tickets == std::vector<Outpost::TicketsView>{{.player = BLUE, .tickets = 1000}, {.player = RED, .tickets = 1000}});
  }

  // Holding as many nodes as the other side costs nothing.
  TEST_METHOD(EqualTerritoryDrainsNothing)
  {
    TerritoryMatch match;
    match.Run(6 * DRAIN_TICKS);
    Assert::AreEqual(STARTING_TICKETS, match.SimulationState().Tickets(BLUE));
    Assert::AreEqual(STARTING_TICKETS, match.SimulationState().Tickets(RED));
  }

  // Every 3 seconds the side behind loses a ticket for each node it is behind; a suppressed
  // Relay counts for its owner.
  TEST_METHOD(TheSideBehindLosesTicketsEveryTenSeconds)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, B1);
    match.Run(DRAIN_TICKS - 1);
    Assert::AreEqual(STARTING_TICKETS, match.SimulationState().Tickets(RED), L"not before three seconds");
    match.Run(1);
    Assert::AreEqual(STARTING_TICKETS - DRAIN_PER_NODE, match.SimulationState().Tickets(RED));
    Assert::AreEqual(STARTING_TICKETS, match.SimulationState().Tickets(BLUE));
    Assert::IsTrue(match.SimulationState().BuildSnapshot(BLUE).tickets[1] == Outpost::TicketsView{.player = RED, .tickets = 999});

    (void)match.Relay(BLUE, A2);
    (void)match.Relay(BLUE, B2);
    const Outpost::PlanePosition node = match.Placement(B2).node;
    (void)match.Warship(RED, {.xMeters = node.xMeters + 200.0f, .zMeters = node.zMeters});
    match.Run(DRAIN_TICKS);
    Assert::IsTrue(match.Sector(BLUE, B2).suppressed);
    Assert::AreEqual(STARTING_TICKETS - DRAIN_PER_NODE - (3 * DRAIN_PER_NODE), match.SimulationState().Tickets(RED),
                     L"three behind, the suppressed B2 included");
  }

  // ADR-057 decision 2: a lead of one node drains the other side in 50 minutes on any map, 1,000
  // drains of three seconds on the 10 km map's 25 nodes as on nine, and the match ends by domination.
  TEST_METHOD(ALeadOfOneNodeWinsInFiftyMinutes)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, B1);
    constexpr std::uint32_t DRAINED = 50 * 60 * TerritoryMatch::TICKS_PER_SECOND;
    static_assert(DRAINED == STARTING_TICKETS * DRAIN_TICKS);
    match.Run(DRAINED - 1);
    Assert::IsFalse(match.SimulationState().MatchOver());
    match.Run(1);
    Assert::IsTrue(match.SimulationState().MatchOver());
    Assert::IsTrue(match.SimulationState().Winner() == BLUE);
    Assert::IsTrue(match.SimulationState().Ending() == Outpost::MatchEnding::Domination);
    const Outpost::Snapshot red = match.SimulationState().BuildSnapshot(RED);
    Assert::IsTrue(red.matchOver && red.winner == BLUE && red.ending == Outpost::MatchEnding::Domination);
    Assert::AreEqual(std::uint64_t{DRAINED}, red.matchEndedTick);
    Assert::AreEqual(0, red.tickets[1].tickets);

    // The outcome stands, and the tickets drain no further.
    match.Run(DRAIN_TICKS);
    Assert::AreEqual(0, match.SimulationState().Tickets(RED));
    Assert::AreEqual(std::uint64_t{DRAINED}, match.SimulationState().BuildSnapshot(BLUE).matchEndedTick);
  }

  // A map without territory has no tickets.
  TEST_METHOD(AMapWithoutSectorsHasNoTickets)
  {
    Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    map.sectors.clear();
    map.outpostRules.clear();
    map.derelictRules.clear();
    Outpost::InProcessServer server(Outpost::LoadTuning(ReadRepositoryTuning()), map, {.seed = 3});
    server.SimulationState().PlaceStartingBases(map);
    Assert::IsTrue(server.SimulationState().BuildSnapshot(BLUE).tickets.empty());
    Assert::AreEqual(0, server.SimulationState().Tickets(BLUE));
  }
};
} // namespace GameLogicTests
