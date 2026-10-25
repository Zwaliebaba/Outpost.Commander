#pragma once

namespace Outpost
{
// The universe run (ADR-082 decision 4): a universe with the server's own player in every seat, stepped as fast as the
// processor allows, killed between two ticks at random and made afresh from its folder each time, beside the same universe
// run straight through. A kill destroys the server and everything it held in memory; the server made afresh comes back as
// its newest save and the commands logged since make it, and its players are made anew. The universe run straight through
// makes its players anew at each tick the killed universe came back at, dropping the orders they had not sent, so that the
// two differ only in what the folder kept. At the end they should be the same universe, to the bit (ADR-009).
struct UniverseRunDesc
{
  std::uint64_t seed = 1;
  // Its two universes are made in Killed and Straight under it, which must not exist yet; their saves and logs stay there.
  std::filesystem::path folder;
  // How many ticks each universe runs, and how many times the killed one is killed, at ticks the seed picks.
  std::uint64_t ticks = 0;
  std::uint32_t kills = 0;
  // Makes what plays a seat, an AI empire in --universe-run: at the start, and again at each kill.
  std::function<std::unique_ptr<HostedPlayer>(PlayerId)> makePlayer;
  // Told now and then which universe runs and how far it has come, if given.
  std::function<void(std::string_view, std::uint64_t)> progress;
};

struct UniverseRunResult
{
  // The ticks the killed universe was killed after, and the tick each time it came back at.
  std::vector<std::uint64_t> killTicks;
  std::vector<std::uint64_t> recoveredTicks;
  bool same = false;
};

[[nodiscard]] UniverseRunResult RunUniverses(const UniverseRunDesc& _desc);
} // namespace Outpost
