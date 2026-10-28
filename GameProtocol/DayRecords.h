#pragma once

namespace Outpost
{
// The day of universe time the overseer's lanes of who played and its windows reach back over (owner, 2026-10-28; ADR-101
// decision 7).
inline constexpr std::uint64_t DAY_RECORDS_SECONDS = std::uint64_t{24} * 3'600;

// A universe log record's first two words: its kind and its tick (ADR-082 decision 1).
struct RecordHead
{
  std::string_view kind;
  std::uint64_t tick = 0;
};

// _record's kind and tick; none for a line that does not start with a word and a whole number.
[[nodiscard]] std::optional<RecordHead> HeadOf(std::string_view _record) noexcept;

// Whether a record of _kind is one of the day's records, which the overseer's lanes and windows are drawn from: a seat's
// hand-over between its player and its deputy (seat), the universe's start or recovery (start) and a battle (battle), for the
// lanes; what came of a connection to a seat (join) and a seat's hour (hour), for its window; and a minute's longest tick
// (tick) and its dropped ticks (dropped), for the server's window. A save is not one: the log writes one a minute.
[[nodiscard]] bool IsDayRecord(std::string_view _kind) noexcept;

// Keeps of _records, the day's records in the order the log wrote them, those of _sinceTick on, and each seat's last
// hand-over before it, which says who played the seat from there. It drops the rest, and every record past a later start's
// tick, since the universe came back to that tick and went on from there.
void KeepDayRecords(std::deque<std::string>& _records, std::uint64_t _sinceTick);
} // namespace Outpost
