#include "pch.h"
#include "RepositoryData.h"
#include "UniverseMatch.h"

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
void Record(Outpost::UniverseLog& _log, std::uint64_t _tick, Outpost::SeatPlay _one, bool _oneFires, bool _twoFires)
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

// ADR-082 decision 1: a universe's log, as ADR-038's match log is written for a match.
TEST_CLASS(UniverseLogTests)
{
public:
  // A seat's records: who plays it when that changes, the orders it fires, its loss, its wait and its restart, and every
  // hour its bank, income, fleet, nodes, research, how long its player and its deputy played it, and the commands applied
  // for it and refused it.
  TEST_METHOD(WritesASeatsRecords)
  {
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
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
      // Two commands applied every thousandth tick, whoever gave them.
      const std::uint32_t orders = _tick % 1000 == 0 ? 2 : 0;
      const std::array<Outpost::SeatTick, 1> seats{{{.player = ONE, .play = _play, .snapshot = &snapshot, .orders = orders}}};
      log.Record(_tick, seats);
      snapshot.events.clear();
    };
    const std::uint64_t hour = 3600ull * TICKS_PER_SECOND;
    for (std::uint64_t tick = 1; tick < hour; ++tick)
    {
      if (tick == 6 || tick == 7)
        snapshot.events.push_back({.kind = Outpost::EventKind::CommandRefused, .refusal = Outpost::CommandResult::NotEnoughOre});
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
      "restart 13 player 1 waited 3",
      "seat 1000 player 1 player",
      std::format("hour {} player 1 ore 500 income 250 fleet 12 cap 20 nodes 1 researched 2 present {} deputy {} orders {} refused 2", hour,
                  (hour - 997) / TICKS_PER_SECOND, 997 / TICKS_PER_SECOND, 2 * (hour / 1000))};
    Assert::IsTrue(lines == expected, std::wstring(winrt::to_hstring(out.str())).c_str());
  }

  // ADR-082 decision 3: an hour whose seat's connection was sampled ends with its round trip at the median and the 95th
  // percentile, the packets sent and lost in the hour, across a newer connection that counts from its own start, its
  // largest backlog, and the bytes sent in the hour, counted as the packets are; an hour without a sample ends as before.
  TEST_METHOD(EndsAnHourWithItsConnection)
  {
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
    const Outpost::Snapshot snapshot = SnapshotOf(ONE, 0);
    // The third sample finds a packet MsQuic had thought lost, and the fourth is a newer connection's.
    const std::array<Neuron::QuicStatistics, 5> samples{
      {{.roundTripMicroseconds = 30'000, .packetsSent = 100, .packetsLost = 1, .backlogBytes = 500, .bytesSent = 1'000},
       {.roundTripMicroseconds = 10'000, .packetsSent = 200, .packetsLost = 3, .backlogBytes = 2'000, .bytesSent = 2'500},
       {.roundTripMicroseconds = 20'000, .packetsSent = 300, .packetsLost = 2, .bytesSent = 4'000},
       {.roundTripMicroseconds = 40'000, .packetsSent = 50, .packetsLost = 1, .backlogBytes = 100, .bytesSent = 600},
       {.roundTripMicroseconds = 50'000, .packetsSent = 120, .packetsLost = 1, .bytesSent = 1'500}}};
    const std::uint64_t hour = 3600ull * TICKS_PER_SECOND;
    for (std::uint64_t tick = 1; tick <= 2 * hour; ++tick)
    {
      Outpost::SeatTick seat{.player = ONE, .snapshot = &snapshot};
      if (tick % TICKS_PER_SECOND == 0 && tick / TICKS_PER_SECOND <= samples.size())
        seat.connection = samples[(tick / TICKS_PER_SECOND) - 1];
      log.Record(tick, std::span(&seat, 1));
    }

    std::vector<std::string> hours;
    std::ranges::copy_if(Lines(out.str()), std::back_inserter(hours), [](const std::string& _line) { return _line.starts_with("hour"); });
    const std::vector<std::string> expected{
      std::format("hour {} player 1 ore 0 income 0 fleet 0 cap 0 nodes 0 researched 0 present 3600 deputy 0 orders 0 refused 0 "
                  "rtt_median_us 30000 rtt_p95_us 50000 sent 420 lost 4 backlog_most 2000 sent_bytes 5500",
                  hour),
      std::format("hour {} player 1 ore 0 income 0 fleet 0 cap 0 nodes 0 researched 0 present 3600 deputy 0 orders 0 refused 0", 2 * hour)};
    Assert::IsTrue(hours == expected, std::wstring(winrt::to_hstring(out.str())).c_str());
  }

  // ADR-082 decision 1: a minute's end writes its longest tick when it took the tick's budget or more, with how many of the
  // minute's ticks did and the parts that took a quarter of it or more, longest first; and the ticks it dropped after stalls.
  // A quiet minute writes neither.
  TEST_METHOD(WritesTheMinutesLongestTickAndItsDroppedTicks)
  {
    using namespace std::chrono_literals;
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
    const std::uint64_t minute = 60ull * TICKS_PER_SECOND;
    const auto timed = [&](std::uint64_t _tick, std::chrono::microseconds _total)
    {
      Outpost::TickTiming timing{.total = _total};
      // The longest tick's commands held a path graph built whole, and a little of its fight.
      if (_tick == minute + 100)
      {
        timing.parts[static_cast<std::size_t>(Outpost::TickPart::Commands)] = 5'600us;
        timing.parts[static_cast<std::size_t>(Outpost::TickPart::GraphBuild)] = 5'100us;
        timing.parts[static_cast<std::size_t>(Outpost::TickPart::Fight)] = 200us;
      }
      log.Timed(_tick, timing);
    };
    for (std::uint64_t tick = 1; tick <= 3 * minute; ++tick)
    {
      if (tick == minute + 100)
        timed(tick, 6'000us);
      else if (tick == minute + 300)
        timed(tick, Outpost::UniverseLog::LONG_TICK);
      else if (tick == minute + 800)
        timed(tick, Outpost::UniverseLog::LONG_TICK - 1us);
      else
        timed(tick, 1'000us);
      if (tick == minute + 500)
        log.Dropped(3);
      if (tick == minute + 900)
        log.Dropped(2);
      if (tick == 2 * minute + 10)
        log.Dropped(7);
    }

    const std::vector<std::string> expected{std::format("tick {} us 6000 over 2 commands 5600 graph_build 5100", minute + 100),
                                            std::format("dropped {} ticks 5", 2 * minute), std::format("dropped {} ticks 7", 3 * minute)};
    Assert::IsTrue(Lines(out.str()) == expected, std::wstring(winrt::to_hstring(out.str())).c_str());
  }

  // ADR-082 decision 1: what came of each connection, with the address it came from: a welcome; a refusal for another build,
  // with the layout its hello named, for a seat it may not take, for what is no hello, and for no hello in time; and a seat's
  // connection gone, taken by a newer one, closed by the game with its code, or lost.
  TEST_METHOD(WritesEveryJoin)
  {
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
    log.Joined(5, {.outcome = Outpost::JoinOutcome::Welcomed, .player = ONE, .address = "192.168.1.20"});
    log.Joined(5, {.outcome = Outpost::JoinOutcome::WrongVersion, .address = "10.0.0.2", .layout = 0x1234'abcd'5678'ef90});
    log.Joined(6, {.outcome = Outpost::JoinOutcome::SeatRefused, .player = TWO, .address = "10.0.0.3"});
    log.Joined(6, {.outcome = Outpost::JoinOutcome::Malformed, .address = "10.0.0.4"});
    log.Joined(6, {.outcome = Outpost::JoinOutcome::Malformed, .player = ONE, .address = "192.168.1.20"});
    log.Joined(7, {.outcome = Outpost::JoinOutcome::Silent, .address = "203.0.113.9"});
    log.Joined(8, {.outcome = Outpost::JoinOutcome::Taken, .player = ONE, .address = "192.168.1.20"});
    log.Joined(9, {.outcome = Outpost::JoinOutcome::Closed, .player = TWO, .address = "::1", .code = 0});
    log.Joined(9, {.outcome = Outpost::JoinOutcome::Lost, .player = TWO});

    const std::vector<std::string> expected{"join 5 welcomed player 1 from 192.168.1.20",
                                            "join 5 refused version layout 1234abcd5678ef90 from 10.0.0.2",
                                            "join 6 refused seat player 2 from 10.0.0.3",
                                            "join 6 refused malformed from 10.0.0.4",
                                            "join 6 refused malformed player 1 from 192.168.1.20",
                                            "join 7 refused silent from 203.0.113.9",
                                            "join 8 gone player 1 taken from 192.168.1.20",
                                            "join 9 gone player 2 closed 0 from ::1",
                                            "join 9 gone player 2 lost from -"};
    Assert::IsTrue(Lines(out.str()) == expected, std::wstring(winrt::to_hstring(out.str())).c_str());
  }

  // ADR-101: the overseer's comings and goings, each with the address it came from (ADR-082 decision 1): its channel opened and closed,
  // and a connection refused for another build, a token that is not the universe's, what is no hello, and no hello in time.
  TEST_METHOD(WritesTheOverseersComingsAndGoings)
  {
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
    log.Oversaw(20, {.outcome = Outpost::OverseerOutcome::Opened, .address = "127.0.0.1"});
    log.Oversaw(21, {.outcome = Outpost::OverseerOutcome::WrongVersion, .address = "127.0.0.1"});
    log.Oversaw(22, {.outcome = Outpost::OverseerOutcome::TokenRefused, .address = "10.0.0.9"});
    log.Oversaw(23, {.outcome = Outpost::OverseerOutcome::Malformed, .address = "::1"});
    log.Oversaw(24, {.outcome = Outpost::OverseerOutcome::Silent, .address = "::1"});
    log.Oversaw(25, {.outcome = Outpost::OverseerOutcome::Closed});

    const std::vector<std::string> expected{"overseer 20 opened from 127.0.0.1",       "overseer 21 refused version from 127.0.0.1",
                                            "overseer 22 refused token from 10.0.0.9", "overseer 23 refused malformed from ::1",
                                            "overseer 24 refused silent from ::1",     "overseer 25 closed from -"};
    Assert::IsTrue(Lines(out.str()) == expected, std::wstring(winrt::to_hstring(out.str())).c_str());
  }

  // ADR-104: each action the overseer asked for, at the tick the server carried it out, with the address it came from.
  TEST_METHOD(WritesTheOverseersActions)
  {
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
    log.Acted(30, {.action = {.action = Outpost::OverseerAction::SaveNow}, .address = "127.0.0.1"});
    log.Acted(
      31, {.action = {.action = Outpost::OverseerAction::DropLink, .seat = Outpost::PlayerId{2}, .deputyAtOnce = true}, .address = "::1"});
    log.Acted(32, {.action = {.action = Outpost::OverseerAction::DropLink, .seat = Outpost::PlayerId{3}}, .address = "::1"});
    log.Acted(33, {.action = {.action = Outpost::OverseerAction::NewToken, .seat = Outpost::PlayerId{2}}, .address = "10.0.0.9"});
    log.Acted(34, {.action = {.action = Outpost::OverseerAction::Stop}});

    const std::vector<std::string> expected{"overseer 30 save from 127.0.0.1", "overseer 31 drop player 2 deputy now from ::1",
                                            "overseer 32 drop player 3 deputy later from ::1", "overseer 33 token player 2 from 10.0.0.9",
                                            "overseer 34 stop from -"};
    Assert::IsTrue(Lines(out.str()) == expected, std::wstring(winrt::to_hstring(out.str())).c_str());
  }

  // ADR-101 decision 7: the day's records, read from the whole log as the overseer's link is made, of the day since a tick and
  // each seat's last hand-over before it, a line ending in a carriage return read as its record; a save is not one.
  TEST_METHOD(ReadsTheDayRecords)
  {
    const TemporaryFolder folder;
    const std::filesystem::path file = folder.Path() / Outpost::UNIVERSE_LOG_FILE;
    std::ofstream(file, std::ios::binary)
      << "start 0 new\r\nseat 10 player 1 deputy\r\njoin 50 welcomed player 1 from 10.0.0.5\r\nsave 60 bytes 10 encode_us 5\r\n"
         "seat 120 player 1 player\r\njoin 120 welcomed player 1 from 10.0.0.5\r\nbattle 130 150 sector 2 player 1 present\r\n"
         "save 160 bytes 10 encode_us 5\r\ntick 180 us 7200 over 1 Movement 5000\r\ndropped 180 ticks 2\r\nhour 200 player 1\r\n";
    Assert::IsTrue(Outpost::UniverseLog::ReadDayRecords(file, 100) ==
                     std::deque<std::string>{"seat 10 player 1 deputy", "seat 120 player 1 player",
                                             "join 120 welcomed player 1 from 10.0.0.5", "battle 130 150 sector 2 player 1 present",
                                             "tick 180 us 7200 over 1 Movement 5000", "dropped 180 ticks 2", "hour 200 player 1"},
                   L"the seat's hand-over before the day, and the day's, without its saves");
    Assert::IsTrue(Outpost::UniverseLog::ReadDayRecords(folder.Path() / "None.log", 0).empty(), L"no log");
  }

  // ADR-101: once asked, the log keeps what it writes for the overseer's stream; and an overseer that connects is sent its last
  // records, read from the end of its file, whole lines only.
  TEST_METHOD(ReadsItsLastRecords)
  {
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
    log.Saved(1, 10, std::chrono::microseconds{5});
    log.KeepLines();
    log.Saved(2, 10, std::chrono::microseconds{5});
    log.Oversaw(3, {.outcome = Outpost::OverseerOutcome::Opened, .address = "::1"});
    Assert::IsTrue(log.TakeLines() == std::vector<std::string>{"save 2 bytes 10 encode_us 5", "overseer 3 opened from ::1"});
    Assert::IsTrue(log.TakeLines().empty(), L"each line taken once");

    // Lines of many lengths, so that the reads from the end start within a line, and a last one still being written.
    const TemporaryFolder folder;
    const std::filesystem::path file = folder.Path() / Outpost::UNIVERSE_LOG_FILE;
    std::vector<std::string> written;
    {
      std::ofstream text(file, std::ios::binary);
      for (std::size_t i = 0; i < 3'000; ++i)
      {
        written.push_back(std::format("record {} {}", i, std::string(i % 53, 'x')));
        text << written.back() << '\n';
      }
      text << "a record still being written";
    }
    Assert::IsTrue(Outpost::UniverseLog::ReadLastRecords(file, 500) == std::vector<std::string>(written.end() - 500, written.end()));
    Assert::IsTrue(Outpost::UniverseLog::ReadLastRecords(file, 5'000) == written, L"a log shorter than asked for, whole");
    Assert::IsTrue(Outpost::UniverseLog::ReadLastRecords(folder.Path() / "None.log", 500).empty(), L"no log");
    // Windows writes the log in text mode, each line ending in a carriage return and a line feed.
    const std::filesystem::path windows = folder.Path() / "Windows.log";
    std::ofstream(windows, std::ios::binary) << "start 0 new\r\nsave 0 bytes 10 encode_us 5\r\n";
    Assert::IsTrue(Outpost::UniverseLog::ReadLastRecords(windows, 500) ==
                     std::vector<std::string>{"start 0 new", "save 0 bytes 10 encode_us 5"},
                   L"without the carriage returns");
  }

  // The battles under way, as the overseer's Health says them (ADR-101): one starts once both sides have fired within the window.
  TEST_METHOD(TellsTheBattlesUnderWay)
  {
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
    Record(log, 100, Outpost::SeatPlay::Player, true, false);
    Assert::IsTrue(log.Battles().empty(), L"one side's fire alone");
    Record(log, 101, Outpost::SeatPlay::Player, false, true);
    const std::vector<Outpost::BattleHealth> battles = log.Battles();
    Assert::AreEqual(std::size_t{1}, battles.size());
    Assert::AreEqual(std::int32_t{4}, battles[0].sector);
    Assert::AreEqual(std::uint64_t{101}, battles[0].startTick);
    Assert::IsTrue(battles[0].players == std::vector<Outpost::PlayerId>{ONE, TWO});
  }

  // A battle is two seats' ships firing at each other in one sector within 10 seconds, until 30 seconds pass without a shot
  // there; each side present when its player played it at a shot of the battle, and otherwise its deputy or the AI. One
  // side's fire alone, or fire too far apart, is none.
  TEST_METHOD(TellsABattleAndWhoWasThere)
  {
    std::ostringstream out;
    Outpost::UniverseLog log(out, TICKS_PER_SECOND);
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

  // A universe's server writes its log beside its saves: its start, each save with how long it took to encode and to write,
  // and who plays each seat; made afresh from its folder, it writes where it came back from (ADR-077).
  TEST_METHOD(AUniversesServerWritesItsLog)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    const auto serve = [&]
    {
      std::unique_ptr<Outpost::InProcessServer> server = Outpost::CreateUniverseServer({.seed = 2, .universe = folder.Path()});
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
    std::ifstream in(folder.Path() / Outpost::UNIVERSE_LOG_FILE);
    const std::vector<std::string> lines = Lines(std::string(std::istreambuf_iterator<char>(in), {}));
    Assert::IsTrue(lines.size() >= 6 && lines[0] == "start 0 new" && lines[1].starts_with("save 0 bytes "), L"a new universe, saved");
    Assert::IsTrue(Has(lines, "seat 1 player 1 ai") && Has(lines, "seat 1 player 2 ai"), L"its AI empires");
    Assert::IsTrue(std::ranges::any_of(lines, [](const std::string& _line) { return _line.starts_with("save 1200 bytes "); }),
                   L"a save a minute");
    // The first save's write is long done a minute of ticks later; the last one's may still be under way as the server goes.
    Assert::IsTrue(std::ranges::any_of(lines, [](const std::string& _line) { return _line.starts_with("written 0 write_us "); }),
                   L"a save written by the folder's thread");
    Assert::IsTrue(std::ranges::any_of(lines, [](const std::string& _line)
                                       { return _line.starts_with("start ") && _line.contains(" recovered save 1200 replayed "); }),
                   L"and where it came back from");
  }
};
} // namespace GameLogicTests
