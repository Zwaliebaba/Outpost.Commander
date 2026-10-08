#pragma once

namespace Outpost
{
// Phase 1 plan task 13.1: matches between two AIs on the real server, played headlessly as fast as it ticks, for P1's
// repeatable figure (Phase 1 design §2). Each match is stepped a tick at a time, its snapshots handed to both AIs and its
// commands applied at the next tick, so it reproduces from its seed and the two AIs' settings. What the switch is asked to
// play is read in Opponent (AiMatchesOptions.h); only the executable holds both the AI and the log (ADR-038).

// Plays the matches, several at once, player 1 the AI of _settings[0] and player 2 that of _settings[1], and writes each
// to _log as MatchLog writes a match against the AI, in seed order. Returns how many ended within the limit. Throws
// Neuron::Exception when the game data is missing or invalid, as CreateInProcessServer does, or when a match fails.
[[nodiscard]] std::uint32_t PlayAiMatches(std::ostream& _log, const std::array<AiSettings, 2>& _settings, const AiMatchesDesc& _desc);

// Loads each AI's settings, plays the matches into the log the options name, and returns how many ended within the limit.
// Throws Neuron::Exception as above, when a settings file is missing or invalid, naming it, or when the log cannot be
// written.
[[nodiscard]] std::uint32_t PlayAiMatches(const AiMatchesOptions& _options);
} // namespace Outpost
