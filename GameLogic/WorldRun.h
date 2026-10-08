#pragma once

namespace Outpost
{
// The world run (Phase 5 design §10, ADR-082): a world with the server's own player in every seat, stepped as fast as the
// processor allows, killed between two ticks at random and made afresh from its folder each time, beside the same world
// run straight through. A kill destroys the server and everything it held in memory; the server made afresh comes back as
// its newest save and the commands logged since make it, and its players are made anew. The world run straight through
// makes its players anew at each tick the killed world came back at, dropping the orders they had not sent, so that the
// two differ only in what the folder kept. At the end they should be the same world, to the bit (ADR-009).
struct WorldRunDesc
{
  std::uint64_t seed = 1;
  // Its two worlds are made in Killed and Straight under it, which must not exist yet; their saves and logs stay there.
  std::filesystem::path folder;
  // How many ticks each world runs, and how many times the killed one is killed, at ticks the seed picks.
  std::uint64_t ticks = 0;
  std::uint32_t kills = 0;
  // Makes what plays a seat, an AI empire in --world-run: at the start, and again at each kill.
  std::function<std::unique_ptr<HostedPlayer>(PlayerId)> makePlayer;
  // Told now and then which world runs and how far it has come, if given.
  std::function<void(std::string_view, std::uint64_t)> progress;
};

struct WorldRunResult
{
  // The ticks the killed world was killed after, and the tick each time it came back at.
  std::vector<std::uint64_t> killTicks;
  std::vector<std::uint64_t> recoveredTicks;
  bool same = false;
};

[[nodiscard]] WorldRunResult RunWorlds(const WorldRunDesc& _desc);
} // namespace Outpost
