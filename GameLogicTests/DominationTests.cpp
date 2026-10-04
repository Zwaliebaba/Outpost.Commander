#include "pch.h"
#include "TerritoryMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = TerritoryMatch::BLUE;
constexpr Outpost::PlayerId RED = TerritoryMatch::RED;
constexpr std::int32_t SOUTH = 2;
constexpr std::int32_t WEST = 4;
constexpr std::int32_t CENTER = 5;
// The repository's tuning data (Phase 2 design §8): 1,000 tickets, and every 10 seconds 30 for each node behind, over the
// map's nine nodes, kept as nine shares to a ticket (ADR-057).
constexpr std::int64_t NODES = 9;
constexpr std::int64_t STARTING_SHARES = 1000 * NODES;
constexpr std::int64_t DRAIN_PER_NODE = 30;
constexpr std::uint32_t DRAIN_TICKS = 10 * TerritoryMatch::TICKS_PER_SECOND;
} // namespace

TEST_CLASS(DominationTests)
{
public:
  // Phase 2 design §8: each player starts with 1,000 tickets, and both players see both.
  TEST_METHOD(EachPlayerStartsWithItsTickets)
  {
    TerritoryMatch match;
    Assert::AreEqual(STARTING_SHARES, match.World().TicketShares(BLUE));
    Assert::AreEqual(STARTING_SHARES, match.World().TicketShares(RED));
    const Outpost::Snapshot red = match.World().BuildSnapshot(RED);
    Assert::AreEqual(1000, red.startingTickets);
    Assert::IsTrue(red.tickets == std::vector<Outpost::TicketsView>{{.player = BLUE, .tickets = 1000}, {.player = RED, .tickets = 1000}});
  }

  // Holding as many nodes as the other side costs nothing.
  TEST_METHOD(EqualTerritoryDrainsNothing)
  {
    TerritoryMatch match;
    match.Run(6 * DRAIN_TICKS);
    Assert::AreEqual(STARTING_SHARES, match.World().TicketShares(BLUE));
    Assert::AreEqual(STARTING_SHARES, match.World().TicketShares(RED));
  }

  // Every 10 seconds the side behind loses 30 tickets for each node it is behind, divided by the map's nine; a suppressed
  // Relay counts for its owner.
  TEST_METHOD(TheSideBehindLosesTicketsEveryTenSeconds)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, SOUTH);
    match.Run(DRAIN_TICKS - 1);
    Assert::AreEqual(STARTING_SHARES, match.World().TicketShares(RED), L"not before ten seconds");
    match.Run(1);
    Assert::AreEqual(STARTING_SHARES - DRAIN_PER_NODE, match.World().TicketShares(RED));
    Assert::AreEqual(STARTING_SHARES, match.World().TicketShares(BLUE));
    Assert::IsTrue(match.World().BuildSnapshot(BLUE).tickets[1] == Outpost::TicketsView{.player = RED, .tickets = 997},
                   L"996 and two thirds, shown rounded up");

    (void)match.Relay(BLUE, WEST);
    (void)match.Relay(BLUE, CENTER);
    const Outpost::PlanePosition node = match.Placement(CENTER).node;
    (void)match.Warship(RED, {.xMeters = node.xMeters + 200.0f, .zMeters = node.zMeters});
    match.Run(DRAIN_TICKS);
    Assert::IsTrue(match.Sector(BLUE, CENTER).suppressed);
    Assert::AreEqual(STARTING_SHARES - DRAIN_PER_NODE - (3 * DRAIN_PER_NODE), match.World().TicketShares(RED),
                     L"three behind, the suppressed center included");
  }

  // Phase 2 design §8: a lead of one node of nine drains the other side in 50 minutes, and the match ends by domination.
  TEST_METHOD(ALeadOfOneNodeWinsInFiftyMinutes)
  {
    TerritoryMatch match;
    (void)match.Relay(BLUE, SOUTH);
    constexpr std::uint32_t FIFTY_MINUTES = 50 * 60 * TerritoryMatch::TICKS_PER_SECOND;
    match.Run(FIFTY_MINUTES - 1);
    Assert::IsFalse(match.World().MatchOver());
    match.Run(1);
    Assert::IsTrue(match.World().MatchOver());
    Assert::IsTrue(match.World().Winner() == BLUE);
    Assert::IsTrue(match.World().Ending() == Outpost::MatchEnding::Domination);
    const Outpost::Snapshot red = match.World().BuildSnapshot(RED);
    Assert::IsTrue(red.matchOver && red.winner == BLUE && red.ending == Outpost::MatchEnding::Domination);
    Assert::AreEqual(std::uint64_t{FIFTY_MINUTES}, red.matchEndedTick);
    Assert::AreEqual(0, red.tickets[1].tickets);

    // The outcome stands, and the tickets drain no further.
    match.Run(DRAIN_TICKS);
    Assert::AreEqual(std::int64_t{0}, match.World().TicketShares(RED));
    Assert::AreEqual(std::uint64_t{FIFTY_MINUTES}, match.World().BuildSnapshot(BLUE).matchEndedTick);
  }

  // A map without territory has no tickets.
  TEST_METHOD(AMapWithoutSectorsHasNoTickets)
  {
    Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    map.sectors.clear();
    Outpost::InProcessServer server(Outpost::LoadTuning(ReadRepositoryTuning()), map, {.seed = 3});
    server.World().PlaceStartingBases(map);
    Assert::IsTrue(server.World().BuildSnapshot(BLUE).tickets.empty());
    Assert::AreEqual(std::int64_t{0}, server.World().TicketShares(BLUE));
  }
};
} // namespace GameLogicTests
