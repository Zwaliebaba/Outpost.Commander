#include "pch.h"
#include "OverseerAttention.h"

#include <algorithm>

namespace
{
using Outpost::Attention;
using Outpost::AttentionLevel;

std::string MillisecondsText(std::uint32_t _microseconds)
{
  return std::format("{:.2f} ms", static_cast<double>(_microseconds) / 1'000.0);
}

// A span of _ticks of universe time at _ticksPerSecond, in days, hours, minutes and seconds as universe time is written.
std::string SpanOf(std::uint64_t _ticks, std::uint32_t _ticksPerSecond)
{
  const std::uint64_t seconds = _ticksPerSecond > 0 ? _ticks / _ticksPerSecond : 0;
  return seconds >= 86'400 ? Outpost::UniverseTime(_ticks, _ticksPerSecond) : Outpost::MinutesAndSeconds(seconds);
}

// The seats a battle is fought between, "seats 1 and 2".
std::string SeatsOf(std::span<const Outpost::PlayerId> _players)
{
  std::string words = _players.size() == 1 ? "seat" : "seats";
  for (std::size_t index = 0; index < _players.size(); ++index)
  {
    const char* joint = index == 0 ? " " : index + 1 == _players.size() ? " and " : ", ";
    words += std::format("{}{}", joint, _players[index].value);
  }
  return words;
}

// The newest save the log tells of: its tick.
std::optional<std::uint64_t> NewestSaveTick(const std::deque<std::string>& _records)
{
  for (auto record = _records.rbegin(); record != _records.rend(); ++record)
  {
    if (const std::optional<Outpost::RecordHead> head = Outpost::HeadOf(*record); head.has_value() && head->kind == "save")
      return head->tick;
  }
  return std::nullopt;
}

// How many games of another build the log says were refused since _sinceTick.
std::size_t RefusedForTheirBuild(const std::deque<std::string>& _records, std::uint64_t _sinceTick)
{
  return static_cast<std::size_t>(std::ranges::count_if(_records,
                                                        [_sinceTick](const std::string& _record)
                                                        {
                                                          const std::optional<Outpost::RecordHead> head = Outpost::HeadOf(_record);
                                                          return head.has_value() && head->kind == "join" && head->tick >= _sinceTick &&
                                                                 _record.contains(" refused version ");
                                                        }));
}

// A seat's connection in each of _healths from _from on that has one, oldest first.
std::vector<Outpost::LinkFigures> LinkOf(Outpost::PlayerId _seat, const std::deque<Outpost::HeardHealth>& _healths, std::size_t _from)
{
  std::vector<Outpost::LinkFigures> figures;
  for (std::size_t index = _from; index < _healths.size(); ++index)
  {
    const Outpost::HeardHealth& heard = _healths[index];
    const auto seat = std::ranges::find(heard.health.seats, _seat, &Outpost::SeatHealth::player);
    if (seat != heard.health.seats.end() && seat->connection.has_value())
      figures.push_back(seat->connection.value_or(Outpost::LinkFigures{}));
  }
  return figures;
}
} // namespace

std::vector<Outpost::Attention> Outpost::AttentionOf(const OverseerModel& _model)
{
  const HeardHealth* latest = _model.LatestHealth();
  const std::optional<OverseerWelcomeMessage>& welcome = _model.Welcome();
  if (latest == nullptr || !welcome.has_value())
    return {};
  const HealthMessage& health = latest->health;
  const std::uint32_t perSecond = welcome->ticksPerSecond;
  const std::uint64_t tick = health.tick;
  const std::deque<HeardHealth>& healths = _model.Healths();
  const std::size_t minuteFrom = healths.size() > ATTENTION_MINUTE_OF_HEALTH ? healths.size() - ATTENTION_MINUTE_OF_HEALTH : 0;
  std::vector<Attention> act;
  std::vector<Attention> know;
  std::vector<Attention> pass;

  // A game of another build refused within the hour: its player needs this build.
  const std::uint64_t hour = std::uint64_t{3'600} * perSecond;
  if (const std::size_t refused = RefusedForTheirBuild(_model.Records(), tick > hour ? tick - hour : 0); refused > 0)
    act.push_back({.level = AttentionLevel::Act,
                   .words = std::format("A game of another build was refused {} {} in the hour", refused, refused == 1 ? "time" : "times"),
                   .seat = {}});
  else
    pass.push_back({.level = AttentionLevel::Pass, .words = "No game was refused for its build in the hour.", .seat = {}});

  // The disk under the universe's folder, which Windows may not have said, and then reads as none.
  if (const std::uint64_t free = health.process.diskFreeBytes; free > 0)
  {
    if (free < ATTENTION_LEAST_DISK_BYTES)
      act.push_back({.level = AttentionLevel::Act, .words = std::format("The server's disk has {} free", BytesText(free)), .seat = {}});
    else
      pass.push_back({.level = AttentionLevel::Pass, .words = std::format("The disk has {} free.", BytesText(free)), .seat = {}});
  }

  // The last save, against two of its intervals.
  if (const std::uint64_t interval = std::uint64_t{welcome->saveSeconds} * perSecond; interval > 0)
  {
    const std::optional<std::uint64_t> saved = NewestSaveTick(_model.Records());
    const std::uint64_t age = saved.has_value() && tick > *saved ? tick - *saved : saved.has_value() ? 0 : tick;
    if (age > ATTENTION_SAVE_INTERVALS * interval)
    {
      act.push_back({.level = AttentionLevel::Act,
                     .words = saved.has_value() ? std::format("The last save is {} old", SpanOf(age, perSecond))
                                                : std::format("No save in the log for {}", SpanOf(age, perSecond)),
                     .seat = {}});
    }
    else if (saved.has_value())
      pass.push_back({.level = AttentionLevel::Pass, .words = std::format("Saved {} ago.", SpanOf(age, perSecond)), .seat = {}});
  }

  // Each seat's link over the last minute: a backlog that keeps growing, a slow round trip, and loss.
  bool linksWear = false;
  for (const SeatHealth& seat : health.seats)
  {
    const std::vector<LinkFigures> figures = LinkOf(seat.player, healths, minuteFrom);
    if (figures.size() > ATTENTION_BACKLOG_SECONDS)
    {
      const std::span<const LinkFigures> last(figures.end() - static_cast<std::ptrdiff_t>(ATTENTION_BACKLOG_SECONDS) - 1, figures.end());
      const bool grew =
        std::ranges::is_sorted(last, {}, &LinkFigures::backlogBytes) && last.back().backlogBytes > last.front().backlogBytes;
      if (grew)
      {
        know.push_back({.level = AttentionLevel::Know,
                        .words = std::format("Seat {}'s backlog has grown for {} s, to {}.", seat.player.value, ATTENTION_BACKLOG_SECONDS,
                                             BytesText(last.back().backlogBytes)),
                        .seat = seat.player});
        linksWear = true;
      }
    }
    if (!figures.empty())
    {
      std::vector<std::uint32_t> roundTrips;
      roundTrips.reserve(figures.size());
      for (const LinkFigures& figure : figures)
        roundTrips.push_back(figure.roundTripMicroseconds);
      std::ranges::sort(roundTrips);
      const std::size_t rank = std::max<std::size_t>(1, ((roundTrips.size() * ATTENTION_ROUND_TRIP_PERCENTILE) + 99) / 100);
      if (const std::uint32_t slow = roundTrips[rank - 1]; slow > ATTENTION_ROUND_TRIP_MICROSECONDS)
      {
        know.push_back({.level = AttentionLevel::Know,
                        .words = std::format("Seat {}'s round trip is {} at the {}th percentile.", seat.player.value,
                                             MillisecondsText(slow), ATTENTION_ROUND_TRIP_PERCENTILE),
                        .seat = seat.player});
        linksWear = true;
      }
      const std::uint64_t sent = figures.back().packetsSent - std::min(figures.back().packetsSent, figures.front().packetsSent);
      const std::uint64_t lost = figures.back().packetsLost - std::min(figures.back().packetsLost, figures.front().packetsLost);
      if (sent > 0 && static_cast<double>(lost) > ATTENTION_LOSS_SHARE * static_cast<double>(sent))
      {
        know.push_back({.level = AttentionLevel::Know,
                        .words = std::format("Seat {} loses {:.1f}% of its packets.", seat.player.value,
                                             100.0 * static_cast<double>(lost) / static_cast<double>(sent)),
                        .seat = seat.player});
        linksWear = true;
      }
    }
  }
  if (!linksWear)
    pass.push_back({.level = AttentionLevel::Pass, .words = "No link wears down.", .seat = {}});

  // The last minute's longest tick, against the tick's budget.
  std::uint32_t longest = 0;
  for (std::size_t index = minuteFrom; index < healths.size(); ++index)
  {
    for (const std::uint32_t ticked : healths[index].health.tickMicroseconds)
      longest = std::max(longest, ticked);
  }
  if (longest > ATTENTION_TICK_MICROSECONDS)
    know.push_back(
      {.level = AttentionLevel::Know, .words = std::format("A tick took {} in the last minute.", MillisecondsText(longest)), .seat = {}});
  else
    pass.push_back({.level = AttentionLevel::Pass,
                    .words = std::format("Every tick of the last minute kept within {}.", MillisecondsText(ATTENTION_TICK_MICROSECONDS)),
                    .seat = {}});

  // Each seat that has waited long for its start (ADR-084 decision 4), and each its deputy has played long (ADR-079).
  for (const SeatHealth& seat : health.seats)
  {
    if (seat.restartTick.has_value() && tick > *seat.restartTick && tick - *seat.restartTick > ATTENTION_START_WAIT_SECONDS * perSecond)
    {
      know.push_back({.level = AttentionLevel::Know,
                      .words = std::format("Seat {} has waited {} for its start to be clear.", seat.player.value,
                                           SpanOf(tick - *seat.restartTick, perSecond)),
                      .seat = seat.player});
    }
    if (seat.play == SeatPlay::Deputy)
    {
      const std::uint64_t since = seat.deputyTick.has_value() && *seat.deputyTick <= tick ? *seat.deputyTick : seat.linkTick;
      if (tick > since && tick - since > ATTENTION_DEPUTY_SECONDS * perSecond)
      {
        know.push_back({.level = AttentionLevel::Know,
                        .words = std::format("Seat {}'s deputy has played it for {}.", seat.player.value, SpanOf(tick - since, perSecond)),
                        .seat = seat.player});
      }
    }
  }

  // Every battle under way.
  for (const BattleHealth& battle : health.battles)
  {
    know.push_back({.level = AttentionLevel::Know,
                    .words = std::format("A battle in sector {} for {}, between {}.", battle.sector,
                                         SpanOf(tick > battle.startTick ? tick - battle.startTick : 0, perSecond), SeatsOf(battle.players)),
                    .seat = {}});
  }

  std::vector<Attention> found = std::move(act);
  std::ranges::move(know, std::back_inserter(found));
  std::ranges::move(pass, std::back_inserter(found));
  return found;
}
