#pragma once

namespace Outpost
{
// The day of universe time the overseer's lanes of who played reach back over (owner, 2026-10-28; ADR-101 decision 7).
inline constexpr std::uint64_t LANE_SECONDS = std::uint64_t{24} * 3'600;

// A universe log record's first two words: its kind and its tick (ADR-082 decision 1).
struct RecordHead
{
  std::string_view kind;
  std::uint64_t tick = 0;
};

// _record's kind and tick; none for a line that does not start with a word and a whole number.
[[nodiscard]] std::optional<RecordHead> HeadOf(std::string_view _record) noexcept;

// Whether a record of _kind is one the overseer's lanes are drawn from: a seat's hand-over between its player and its deputy
// (seat), the universe's start or recovery (start), or a battle (battle).
[[nodiscard]] bool IsLaneRecord(std::string_view _kind) noexcept;

// Keeps of _records, the lanes' records in the order the log wrote them, those of _sinceTick on, and each seat's last
// hand-over before it, which says who played the seat from there. It drops the rest, and every record past a later start's
// tick, since the universe came back to that tick and went on from there.
void KeepLaneRecords(std::deque<std::string>& _records, std::uint64_t _sinceTick);
} // namespace Outpost
