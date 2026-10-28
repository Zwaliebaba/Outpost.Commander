#include "pch.h"
#include "DayRecords.h"

#include <charconv>
#include <map>

namespace
{
// The word after _skip words of _record, if it has one.
std::string_view WordOf(std::string_view _record, std::size_t _skip) noexcept
{
  std::size_t begin = 0;
  for (std::size_t word = 0; word < _skip; ++word)
  {
    begin = _record.find(' ', begin);
    if (begin == std::string_view::npos)
      return {};
    ++begin;
  }
  return _record.substr(begin, _record.find(' ', begin) - begin);
}

std::optional<std::uint64_t> NumberOf(std::string_view _word) noexcept
{
  std::uint64_t value = 0;
  const auto [end, error] = std::from_chars(_word.data(), _word.data() + _word.size(), value);
  if (_word.empty() || error != std::errc{} || end != _word.data() + _word.size())
    return std::nullopt;
  return value;
}

// The player a seat record hands over: "seat <tick> player <n> <play>".
std::optional<std::uint64_t> SeatPlayerOf(std::string_view _record) noexcept
{
  return WordOf(_record, 2) == "player" ? NumberOf(WordOf(_record, 3)) : std::nullopt;
}
} // namespace

std::optional<Outpost::RecordHead> Outpost::HeadOf(std::string_view _record) noexcept
{
  const std::string_view kind = WordOf(_record, 0);
  const std::optional<std::uint64_t> tick = NumberOf(WordOf(_record, 1));
  if (kind.empty() || !tick.has_value())
    return std::nullopt;
  return RecordHead{.kind = kind, .tick = *tick};
}

bool Outpost::IsDayRecord(std::string_view _kind) noexcept
{
  return _kind == "seat" || _kind == "start" || _kind == "battle" || _kind == "join" || _kind == "hour" || _kind == "tick" ||
         _kind == "dropped";
}

void Outpost::KeepDayRecords(std::deque<std::string>& _records, std::uint64_t _sinceTick)
{
  // A start goes back to its tick: what was written past it before it belongs to a universe that did not go on.
  std::vector<std::string> ordered;
  std::vector<std::uint64_t> ticks;
  for (std::string& record : _records)
  {
    const std::optional<RecordHead> head = HeadOf(record);
    if (!head.has_value() || !IsDayRecord(head->kind))
      continue;
    if (head->kind == "start")
    {
      while (!ticks.empty() && ticks.back() > head->tick)
      {
        ticks.pop_back();
        ordered.pop_back();
      }
    }
    ticks.push_back(head->tick);
    ordered.push_back(std::move(record));
  }
  // Each seat's last hand-over before the day, which the day starts from.
  std::map<std::uint64_t, std::size_t> lastBefore;
  for (std::size_t index = 0; index < ordered.size(); ++index)
  {
    if (ticks[index] >= _sinceTick || HeadOf(ordered[index]).value_or(RecordHead{}).kind != "seat")
      continue;
    if (const std::optional<std::uint64_t> player = SeatPlayerOf(ordered[index]))
      lastBefore[*player] = index;
  }
  std::deque<std::string> kept;
  for (std::size_t index = 0; index < ordered.size(); ++index)
  {
    const std::optional<std::uint64_t> player = SeatPlayerOf(ordered[index]);
    const bool startsTheDay = player.has_value() && lastBefore.contains(*player) && lastBefore[*player] == index;
    if (ticks[index] >= _sinceTick || startsTheDay)
      kept.push_back(std::move(ordered[index]));
  }
  _records = std::move(kept);
}
