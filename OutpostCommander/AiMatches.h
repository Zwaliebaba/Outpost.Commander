#pragma once

namespace Outpost
{
// Phase 1 plan task 13.1: matches between two AIs on the real server, played headlessly as fast as it ticks, for P1's
// repeatable figure (Phase 1 design §2). Both players are the AI with the packaged settings. Each match is stepped a tick
// at a time, its snapshots handed to both AIs and its commands applied at the next tick, so it reproduces from its seed.
struct AiMatchesDesc
{
  std::uint64_t firstSeed = 1;
  std::uint32_t matches = 10;
  // A match still going this long is left, and logged as left.
  std::uint32_t limitMinutes = 120;
};

// Plays the matches, several at once, and writes each to _log as MatchLog writes a match against the AI, in seed order.
// Returns how many ended within the limit. Throws Neuron::Exception when the game data is missing or invalid, as
// CreateInProcessServer does, or when a match fails.
[[nodiscard]] std::uint32_t PlayAiMatches(std::ostream& _log, const AiSettings& _settings, const AiMatchesDesc& _desc);
} // namespace Outpost
