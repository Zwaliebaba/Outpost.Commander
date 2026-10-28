#pragma once

namespace Outpost
{
// How much what the overseer finds needs the owner (ADR-103 decision 7): to be acted on now, a chip (ADR-085 decision 1;
// ADR-066 decision 2); to be known, in plain text; or a check that passes, so that a quiet list says all is well.
enum class AttentionLevel : std::uint8_t
{
  Act,
  Know,
  Pass
};

// One thing found: how much it needs the owner, what it says, and the seat it is about, no player for none.
struct Attention
{
  AttentionLevel level = AttentionLevel::Know;
  std::string words;
  PlayerId seat;
};

// The rules' thresholds, in this one place (ADR-103 decision 7): the disk free below which, and how many save intervals past
// the last save after which, the owner acts; how long a backlog grows, how slow a round trip at which percentile, how much loss
// and how long a tick, how long a seat waits for its start and how long a deputy plays, past which the owner is told; and the
// last minute of Health, one a second, which the link's rules and the tick's look over.
inline constexpr std::uint64_t ATTENTION_LEAST_DISK_BYTES = std::uint64_t{5} * 1'024 * 1'024 * 1'024;
inline constexpr std::uint32_t ATTENTION_SAVE_INTERVALS = 2;
inline constexpr std::size_t ATTENTION_BACKLOG_SECONDS = 30;
inline constexpr std::uint32_t ATTENTION_ROUND_TRIP_MICROSECONDS = 250'000;
inline constexpr std::size_t ATTENTION_ROUND_TRIP_PERCENTILE = 95;
inline constexpr double ATTENTION_LOSS_SHARE = 0.02;
inline constexpr std::uint32_t ATTENTION_TICK_MICROSECONDS = 5'000;
inline constexpr std::uint64_t ATTENTION_START_WAIT_SECONDS = std::uint64_t{10} * 60;
inline constexpr std::uint64_t ATTENTION_DEPUTY_SECONDS = std::uint64_t{24} * 3'600;
inline constexpr std::size_t ATTENTION_MINUTE_OF_HEALTH = 60;

// What _model's Health and log say needs the owner: what to act on first, then what to know, then the checks that pass;
// none before a welcome and a Health.
[[nodiscard]] std::vector<Attention> AttentionOf(const OverseerModel& _model);
} // namespace Outpost
