#pragma once

namespace Outpost
{
// Phase 1 plan task 13.1: matches between two AIs on the real server, played headlessly as fast as it ticks, for P1's
// repeatable figure (Phase 1 design §2). Each match is stepped a tick at a time, its snapshots handed to both AIs and its
// commands applied at the next tick, so it reproduces from its seed and the two AIs' settings.
struct AiMatchesDesc
{
  std::uint64_t firstSeed = 1;
  std::uint32_t matches = 10;
  // A match still going this long is left, and logged as left.
  std::uint32_t limitMinutes = 120;
};

// What the --ai-matches switch is asked to play (ADR-063). With no options it is P1's ten matches of the packaged AI
// against itself (ADR-038); a script such as Tools/SelfPlay.py names the seeds, each AI's settings and the log.
struct AiMatchesOptions
{
  AiMatchesDesc desc;
  // The settings of player 1's AI and player 2's, each a file in Opponent.json's format; empty for the packaged Opponent.json.
  std::array<std::filesystem::path, 2> settings;
  // Where the matches are logged. The file is replaced.
  std::filesystem::path log;
};

// Reads the switch's options from the command line's arguments, which are the program's name, --ai-matches and --quiet
// left out: --first-seed <seed>, --matches <count>, --limit-minutes <minutes>, --ai1 <settings>, --ai2 <settings> and
// --log <file>, each at most once and in any order. A log not named is OutpostCommander-ai-matches.log in the temporary
// folder. Throws Neuron::Exception naming the first argument it does not take.
[[nodiscard]] AiMatchesOptions ReadAiMatchesOptions(std::span<const std::wstring> _arguments);

// Plays the matches, several at once, player 1 the AI of _settings[0] and player 2 that of _settings[1], and writes each
// to _log as MatchLog writes a match against the AI, in seed order. Returns how many ended within the limit. Throws
// Neuron::Exception when the game data is missing or invalid, as CreateInProcessServer does, or when a match fails.
[[nodiscard]] std::uint32_t PlayAiMatches(std::ostream& _log, const std::array<AiSettings, 2>& _settings, const AiMatchesDesc& _desc);

// Loads each AI's settings, plays the matches into the log the options name, and returns how many ended within the limit.
// Throws Neuron::Exception as above, when a settings file is missing or invalid, naming it, or when the log cannot be
// written.
[[nodiscard]] std::uint32_t PlayAiMatches(const AiMatchesOptions& _options);
} // namespace Outpost
