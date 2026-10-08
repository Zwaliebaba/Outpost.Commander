#pragma once

namespace Outpost
{
// Phase 1 plan task 13.1: matches between two AIs on the real server, played headlessly as fast as it ticks, for P1's
// repeatable figure (Phase 1 design §2). The executable plays them (AiMatches.h); what it is asked to play is read here,
// where a test can reach it.
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
} // namespace Outpost
