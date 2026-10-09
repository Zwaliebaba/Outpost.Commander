#include "pch.h"
#include "RepositoryData.h"
#include "WorldMatch.h"

#include <sstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId ONE{1};
constexpr Outpost::PlayerId TWO{2};
constexpr std::uint32_t TICKS_PER_SECOND = 20;

std::vector<std::string> Lines(const std::string& _text)
{
  std::vector<std::string> lines;
  std::istringstream in(_text);
  for (std::string line; std::getline(in, line);)
    lines.push_back(line);
  return lines;
}

// A snapshot of player _player at _tick with a ship of each player's in sector 4.
Outpost::Snapshot SnapshotOf(Outpost::PlayerId _player, std::uint64_t _tick)
{
  Outpost::Snapshot snapshot{.tick = _tick, .player = _player};
  snapshot.entities = {{.id = Outpost::EntityId{10}, .owner = ONE, .position = {.xMeters = 10.0f}},
                       {.id = Outpost::EntityId{20}, .owner = TWO, .position = {.xMeters = 20.0f}}};
  snapshot.sectors = {{.id = 4, .minXMeters = -100.0f, .maxXMeters = 100.0f, .minZMeters = -100.0f, .maxZMeters = 100.0f}};
  return snapshot;
}

// One tick of both seats, _one playing seat 1 and seat 2 the AI, each with the shots of its own ship at the other's given.
void Record(Outpost::WorldLog& _log, std::uint64_t _tick, Outpost::SeatPlay _one, bool _oneFires, bool _twoFires)
{
  Outpost::Snapshot first = SnapshotOf(ONE, _tick);
  Outpost::Snapshot second = SnapshotOf(TWO, _tick);
  if (_oneFires)
    first.shots.push_back({.shooter = Outpost::EntityId{10}, .target = Outpost::EntityId{20}, .from = {.xMeters = 10.0f}});
  if (_twoFires)
    second.shots.push_back({.shooter = Outpost::EntityId{20}, .target = Outpost::EntityId{10}, .from = {.xMeters = 20.0f}});
  const std::array<Outpost::SeatTick, 2> seats{
    {{.player = ONE, .play = _one, .snapshot = &first}, {.player = TWO, .play = Outpost::SeatPlay::Ai, .snapshot = &second}}};
  _log.Record(_tick, seats);
}

bool Has(const std::vector<std::string>& _lines, std::string_view _line)
{
  return std::ranges::find(_lines, _line) != _lines.end();
}
} // namespace

// Phase 5 design §10 (ADR-082): a world's log, as ADR-038's match log is written for a match.
TEST_CLASS(WorldLogTests)
{
public:
  // A seat's records: who plays it when that changes, the orders it fires, its loss, its wait and its restart, and every
  // hour its bank, income, fleet, nodes, research and how long its player and its deputy played it.
  TEST_METHOD(WritesASeatsRecords)
  {
    std::ostringstream out;
    Outpost::WorldLog log(out, TICKS_PER_SECOND);
    log.Started(0, std::nullopt, 0);
    log.Saved(1200, 78'000, std::chrono::microseconds{360});
    log.Started(1250, 1200, 7);

    Outpost::Snapshot snapshot = SnapshotOf(ONE, 0);
    snapshot.ore = 500;
    snapshot.oreIncomeHundredthsPerSecond = 250;
    snapshot.commandPoints = 12;
    snapshot.fleetCap = 20;
    snapshot.sectors[0].holder = ONE;
    snapshot.research = {{.researched = true}, {.researched = false}, {.researched = true}};
    const auto record = [&](std::uint64_t _tick, Outpost::SeatPlay _play)
    {
      const std::array<Outpost::SeatTick, 1> seats{{{.player = ONE, .play = _play, .snapshot = &snapshot}}};
      log.Record(_tick, seats);
      snapshot.events.clear();
    };
    const std::uint64_t hour = 3600ull * TICKS_PER_SECOND;
    for (std::uint64_t tick = 1; tick < hour; ++tick)
    {
      if (tick == 4)
      {
        snapshot.events.push_back({.kind = Outpost::EventKind::OrderFired,
                                   .order = 7,
                                   .action = Outpost::ScheduledActionKind::AttackMove,
                                   .outcome = Outpost::OrderOutcome::HeldInstead});
      }
      if (tick == 5)
        snapshot.events.push_back({.kind = Outpost::EventKind::EmpireLost});
      snapshot.restartTick = tick >= 5 && tick < 13 ? std::optional<std::uint64_t>(10) : std::nullopt;
      if (tick == 13)
        snapshot.events.push_back({.kind = Outpost::EventKind::EmpireRestarted});
      record(tick, tick < 3 || tick >= 1000 ? Outpost::SeatPlay::Player : Outpost::SeatPlay::Deputy);
    }
    record(hour, Outpost::SeatPlay::Player);

    const std::vector<std::string> lines = Lines(out.str());
    const std::vector<std::string> expected{
      "start 0 new",
      "save 1200 bytes 78000 encode_us 360",
      "start 1250 recovered save 1200 replayed 7",
      "seat 1 player 1 player",
      "seat 3 player 1 deputy",
      "order 4 player 1 order 7 attack-move held",
      "lost 5 player 1",
      "waiting 11 player 1",
      "restart 13 player 1",
      "seat 1000 player 1 player",
      std::format("hour {} player 1 ore 500 income 250 fleet 12 cap 20 nodes 1 researched 2 present {} deputy {}", hour,
                  (hour - 997) / TICKS_PER_SECOND, 997 / TICKS_PER_SECOND)};
    Assert::IsTrue(lines == expected, std::wstring(winrt::to_hstring(out.str())).c_str());
  }

  // A battle is two seats' ships firing at each other in one sector within 10 seconds, until 30 seconds pass without a shot
  // there; each side present when its player played it at a shot of the battle, and otherwise its deputy or the AI. One
  // side's fire alone, or fire too far apart, is none.
  TEST_METHOD(TellsABattleAndWhoWasThere)
  {
    std::ostringstream out;
    Outpost::WorldLog log(out, TICKS_PER_SECOND);
    const std::uint64_t window = 10ull * TICKS_PER_SECOND;
    const std::uint64_t gap = 30ull * TICKS_PER_SECOND;
    std::uint64_t tick = 100;
    // Seat 1's player fires; the AI answers inside the window, and the battle starts; then the deputy fights on for seat 1.
    Record(log, tick, Outpost::SeatPlay::Player, true, false);
    Record(log, tick + window, Outpost::SeatPlay::Player, false, true);
    Record(log, tick + window + 40, Outpost::SeatPlay::Deputy, true, false);
    for (std::uint64_t quiet = tick + window + 41; quiet <= tick + window + 40 + gap; ++quiet)
      Record(log, quiet, Outpost::SeatPlay::Deputy, false, false);
    const std::uint64_t firstEnd = tick + window + 40;
    // The deputy alone against the AI; then fire a window and more apart, which is no battle.
    tick = firstEnd + gap + 100;
    Record(log, tick, Outpost::SeatPlay::Deputy, true, true);
    Record(log, tick + 1, Outpost::SeatPlay::Deputy, false, false);
    for (std::uint64_t quiet = tick + 2; quiet <= tick + gap + 1; ++quiet)
      Record(log, quiet, Outpost::SeatPlay::Deputy, false, false);
    tick += gap + 100;
    Record(log, tick, Outpost::SeatPlay::Player, true, false);
    Record(log, tick + window + 1, Outpost::SeatPlay::Player, false, true);
    for (std::uint64_t quiet = tick + window + 2; quiet <= tick + window + gap + 2; ++quiet)
      Record(log, quiet, Outpost::SeatPlay::Player, false, false);

    const std::vector<std::string> lines = Lines(out.str());
    const std::vector<std::string> battles{
      std::format("battle {} {} sector 4 player 1 present player 2 ai", 100 + window, firstEnd),
      std::format("battle {} {} sector 4 player 1 deputy player 2 ai", firstEnd + gap + 100, firstEnd + gap + 100)};
    std::vector<std::string> written;
    std::ranges::copy_if(lines, std::back_inserter(written), [](const std::string& _line) { return _line.starts_with("battle"); });
    Assert::IsTrue(written == battles, std::wstring(winrt::to_hstring(out.str())).c_str());
  }

  // A world's server writes its log beside its saves: its start, each save with how long it took, and who plays each seat;
  // made afresh from its folder, it writes where it came back from (ADR-077).
  TEST_METHOD(AWorldsServerWritesItsLog)
  {
    const ScopedHomeDirectory home(RepositoryHome());
    const TemporaryFolder folder;
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    const auto serve = [&]
    {
      std::unique_ptr<Outpost::InProcessServer> server = Outpost::CreateWorldServer({.seed = 2, .world = folder.Path()});
      for (const Outpost::PlayerId player : {ONE, TWO})
        server->Host(player, std::make_unique<Outpost::AiEmpire>(settings, server->TicksPerSecond()));
      return server;
    };
    {
      std::unique_ptr<Outpost::InProcessServer> server = serve();
      for (std::uint32_t tick = 0; tick < 61 * TICKS_PER_SECOND; ++tick)
        server->Step();
    }
    (void)serve();
    std::ifstream in(folder.Path() / Outpost::WORLD_LOG_FILE);
    const std::vector<std::string> lines = Lines(std::string(std::istreambuf_iterator<char>(in), {}));
    Assert::IsTrue(lines.size() >= 6 && lines[0] == "start 0 new" && lines[1].starts_with("save 0 bytes "), L"a new world, saved");
    Assert::IsTrue(Has(lines, "seat 1 player 1 ai") && Has(lines, "seat 1 player 2 ai"), L"its AI empires");
    Assert::IsTrue(std::ranges::any_of(lines, [](const std::string& _line) { return _line.starts_with("save 1200 bytes "); }),
                   L"a save a minute");
    Assert::IsTrue(std::ranges::any_of(lines, [](const std::string& _line)
                                       { return _line.starts_with("start ") && _line.contains(" recovered save 1200 replayed "); }),
                   L"and where it came back from");
  }
};
} // namespace GameLogicTests
