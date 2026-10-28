#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Clock = std::chrono::steady_clock;
using Outpost::AttentionLevel;

const Clock::time_point START{std::chrono::seconds{1'000}};
// Two hours of universe time at 20 ticks a second, where every test's last Health stands.
constexpr std::uint64_t NOW_TICK = std::uint64_t{20} * 7'200;

Outpost::OverseerWelcomeMessage Welcome()
{
  return {.ticksPerSecond = 20,
          .seats = {{.player = Outpost::PlayerId{1}, .ai = {}}, {.player = Outpost::PlayerId{2}, .ai = "Normal"}},
          .saveSeconds = 60};
}

// A Health _secondsBefore before NOW_TICK, all well: its ticks of 1 ms, the disk with 120 GB free, and seat 1 online with a
// link that keeps up, as _shape changes it.
Outpost::HealthMessage HealthAt(std::uint64_t _secondsBefore)
{
  Outpost::HealthMessage health{.tick = NOW_TICK - (_secondsBefore * 20)};
  health.tickMicroseconds.assign(20, 1'000);
  health.process.diskFreeBytes = std::uint64_t{120} * 1'024 * 1'024 * 1'024;
  health.seats = {{.player = Outpost::PlayerId{1},
                   .play = Outpost::SeatPlay::Player,
                   .link = Outpost::SeatLink::Online,
                   .connection = Outpost::LinkFigures{.roundTripMicroseconds = 40'000, .packetsSent = 1'000 + (60 - _secondsBefore) * 100}},
                  {.player = Outpost::PlayerId{2}, .play = Outpost::SeatPlay::Ai}};
  return health;
}

// _model welcomed, told the last minute of Health, each as _shape changes it, and the log's _records.
void Tell(Outpost::OverseerModel& _model, const std::function<void(Outpost::HealthMessage&, std::uint64_t)>& _shape,
          std::vector<std::string> _records = {std::format("save {} bytes 10 encode_us 5", NOW_TICK - 200)})
{
  _model.Take(Outpost::EncodeOverseerMessage(Welcome()), START);
  _model.Take(Outpost::EncodeOverseerMessage(Outpost::LogRecordsMessage{.lines = std::move(_records)}), START);
  for (std::uint64_t before = 60;; --before)
  {
    Outpost::HealthMessage health = HealthAt(before);
    _shape(health, before);
    _model.Take(Outpost::EncodeOverseerMessage(health), START + std::chrono::seconds{60 - before});
    if (before == 0)
      break;
  }
}

std::vector<std::string> WordsAt(const std::vector<Outpost::Attention>& _found, AttentionLevel _level)
{
  std::vector<std::string> words;
  for (const Outpost::Attention& found : _found)
  {
    if (found.level == _level)
      words.push_back(found.words);
  }
  return words;
}

bool Says(const std::vector<Outpost::Attention>& _found, AttentionLevel _level, std::string_view _words)
{
  return std::ranges::any_of(_found, [&](const Outpost::Attention& _one) { return _one.level == _level && _one.words == _words; });
}

std::wstring Listed(const std::vector<Outpost::Attention>& _found)
{
  std::wstring all;
  for (const Outpost::Attention& found : _found)
    all += std::wstring(winrt::to_hstring(found.words)) + L" | ";
  return all;
}
} // namespace

TEST_CLASS(OverseerAttentionTests)
{
public:
  // ADR-102 decision 7: with nothing to act on or know, every check says it passes, and nothing before a Health.
  TEST_METHOD(PassesEveryCheckWhenAllIsWell)
  {
    Assert::IsTrue(Outpost::AttentionOf(Outpost::OverseerModel()).empty(), L"nothing before a welcome and a Health");
    Outpost::OverseerModel model;
    Tell(model, [](Outpost::HealthMessage&, std::uint64_t) {});
    const std::vector<Outpost::Attention> found = Outpost::AttentionOf(model);
    Assert::IsTrue(WordsAt(found, AttentionLevel::Act).empty() && WordsAt(found, AttentionLevel::Know).empty(), Listed(found).c_str());
    for (const std::string_view words : {"No game was refused for its build in the hour.", "The disk has 120 GB free.", "Saved 0:10 ago.",
                                         "No link wears down.", "Every tick of the last minute kept within 5.00 ms."})
      Assert::IsTrue(Says(found, AttentionLevel::Pass, words), Listed(found).c_str());
  }

  // ADR-102 decision 7, ADR-085 decision 1: what the owner can act on now is a chip: a game of another build refused within the
  // hour, the disk under 5 GB, and the last save older than two of its intervals; and it comes first.
  TEST_METHOD(ActsOnWhatTheOwnerCanFix)
  {
    Outpost::OverseerModel model;
    Tell(model,
         [](Outpost::HealthMessage& _health, std::uint64_t) { _health.process.diskFreeBytes = std::uint64_t{3} * 1'024 * 1'024 * 1'024; },
         {"join 1000 refused version layout 0000000000000001 from 10.0.0.5", std::format("save {} bytes 10 encode_us 5", NOW_TICK - 3'600),
          std::format("join {} refused version layout 0000000000000001 from 10.0.0.5", NOW_TICK - 2'000)});
    const std::vector<Outpost::Attention> found = Outpost::AttentionOf(model);
    Assert::IsTrue(Says(found, AttentionLevel::Act, "A game of another build was refused 1 time in the hour"), Listed(found).c_str());
    Assert::IsTrue(Says(found, AttentionLevel::Act, "The server's disk has 3.0 GB free"), Listed(found).c_str());
    Assert::IsTrue(Says(found, AttentionLevel::Act, "The last save is 3:00 old"), Listed(found).c_str());
    Assert::IsTrue(found.front().level == AttentionLevel::Act, L"what to act on first");
  }

  // ADR-102 decision 7: a link wearing down is told: a backlog grown for 30 s, a round trip over 250 ms at the 95th percentile of
  // the last minute, and loss over 2% in it, each naming its seat.
  TEST_METHOD(TellsOfALinkWearingDown)
  {
    Outpost::OverseerModel model;
    Tell(model,
         [](Outpost::HealthMessage& _health, std::uint64_t _before)
         {
           Outpost::LinkFigures& link = _health.seats[0].connection.value();
           link.backlogBytes = 10'000 + ((60 - _before) * 1'024);
           link.roundTripMicroseconds = 300'000;
           link.packetsLost = (60 - _before) * 5;
         });
    const std::vector<Outpost::Attention> found = Outpost::AttentionOf(model);
    Assert::IsTrue(Says(found, AttentionLevel::Know, "Seat 1's backlog has grown for 30 s, to 69.8 KB."), Listed(found).c_str());
    Assert::IsTrue(Says(found, AttentionLevel::Know, "Seat 1's round trip is 300.00 ms at the 95th percentile."), Listed(found).c_str());
    Assert::IsTrue(Says(found, AttentionLevel::Know, "Seat 1 loses 5.0% of its packets."), Listed(found).c_str());
    Assert::IsFalse(Says(found, AttentionLevel::Pass, "No link wears down."));
    Assert::IsTrue(std::ranges::all_of(found, [](const Outpost::Attention& _one)
                                       { return _one.level != AttentionLevel::Know || _one.seat == Outpost::PlayerId{1}; }),
                   L"each naming seat 1");
  }

  // ADR-102 decision 7: a tick over 5 ms in the last minute, a seat that has waited over 10 minutes for its start, a deputy that
  // has played a seat for over a day, and every battle under way are told.
  TEST_METHOD(TellsOfLongTicksWaitsDeputiesAndBattles)
  {
    Outpost::OverseerModel model;
    Tell(model,
         [](Outpost::HealthMessage& _health, std::uint64_t _before)
         {
           if (_before == 30)
             _health.tickMicroseconds.back() = 7'200;
           _health.seats[1].restartTick = NOW_TICK - (std::uint64_t{20} * 12 * 60);
           _health.seats.push_back({.player = Outpost::PlayerId{3}, .play = Outpost::SeatPlay::Deputy, .link = Outpost::SeatLink::Open});
           _health.battles = {
             {.sector = 4, .startTick = NOW_TICK - (std::uint64_t{20} * 80), .players = {Outpost::PlayerId{1}, Outpost::PlayerId{2}}}};
         });
    // A day and an hour from the start, seat 3's deputy has played it since tick 0.
    Outpost::HealthMessage later = HealthAt(0);
    later.tick = std::uint64_t{20} * 25 * 3'600;
    later.seats.push_back({.player = Outpost::PlayerId{3}, .play = Outpost::SeatPlay::Deputy, .link = Outpost::SeatLink::Open});
    const std::vector<Outpost::Attention> found = Outpost::AttentionOf(model);
    Assert::IsTrue(Says(found, AttentionLevel::Know, "A tick took 7.20 ms in the last minute."), Listed(found).c_str());
    Assert::IsTrue(Says(found, AttentionLevel::Know, "Seat 2 has waited 12:00 for its start to be clear."), Listed(found).c_str());
    Assert::IsTrue(Says(found, AttentionLevel::Know, "A battle in sector 4 for 1:20, between seats 1 and 2."), Listed(found).c_str());
    Assert::IsTrue(std::ranges::none_of(found, [](const Outpost::Attention& _one) { return _one.words.starts_with("Seat 3's deputy"); }),
                   L"not yet a day");
    model.Take(Outpost::EncodeOverseerMessage(later), START + std::chrono::seconds{61});
    Assert::IsTrue(Says(Outpost::AttentionOf(model), AttentionLevel::Know, "Seat 3's deputy has played it for 1 d 01:00:00."),
                   Listed(Outpost::AttentionOf(model)).c_str());
  }
};
} // namespace GameAppTests
