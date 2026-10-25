#include "pch.h"
#include "UniverseRun.h"

namespace
{
// SplitMix64, for the kills' ticks: the same on every platform, as the run is (ADR-009 needs none of it).
std::uint64_t Next(std::uint64_t& _state) noexcept
{
  std::uint64_t value = (_state += 0x9E3779B97F4A7C15ull);
  value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
  value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
  return value ^ (value >> 31);
}

constexpr std::uint64_t PROGRESS_TICKS = 72'000;

// A universe's server for the run, its seats each played by the run's own player.
std::unique_ptr<Outpost::InProcessServer> Serve(const Outpost::UniverseRunDesc& _desc, const std::filesystem::path& _folder)
{
  std::unique_ptr<Outpost::InProcessServer> server = Outpost::CreateUniverseServer({.seed = _desc.seed, .universe = _folder});
  for (std::size_t seat = 0; seat < server->MapData().starts.size(); ++seat)
  {
    const Outpost::PlayerId player{static_cast<std::uint32_t>(seat + 1)};
    server->Host(player, _desc.makePlayer(player));
  }
  return server;
}
} // namespace

Outpost::UniverseRunResult Outpost::RunUniverses(const UniverseRunDesc& _desc)
{
  if (!_desc.makePlayer || _desc.ticks == 0)
    throw Neuron::Exception("RunUniverses: a universe run needs its players and its length");
  const std::filesystem::path killedFolder = _desc.folder / "Killed";
  const std::filesystem::path straightFolder = _desc.folder / "Straight";
  if (std::filesystem::exists(killedFolder) || std::filesystem::exists(straightFolder))
    throw Neuron::Exception(std::format("RunUniverses: {} holds a universe run already.", _desc.folder.string()));

  UniverseRunResult result;
  std::uint64_t random = _desc.seed;
  while (result.killTicks.size() < _desc.kills && _desc.ticks > 1)
  {
    const std::uint64_t tick = 1 + (Next(random) % (_desc.ticks - 1));
    if (std::ranges::find(result.killTicks, tick) == result.killTicks.end())
      result.killTicks.push_back(tick);
  }
  std::ranges::sort(result.killTicks);

  const auto report = [&_desc](std::string_view _universe, std::uint64_t _tick)
  {
    if (_desc.progress && _tick % PROGRESS_TICKS == 0)
      _desc.progress(_universe, _tick);
  };

  // The killed universe first, which says where each kill brought it back to.
  std::unique_ptr<InProcessServer> killed = Serve(_desc, killedFolder);
  auto kill = result.killTicks.begin();
  while (killed->SimulationState().CurrentTick() < _desc.ticks)
  {
    while (kill != result.killTicks.end() && *kill == killed->SimulationState().CurrentTick())
    {
      killed.reset();
      killed = Serve(_desc, killedFolder);
      result.recoveredTicks.push_back(killed->SimulationState().CurrentTick());
      ++kill;
    }
    killed->Step();
    report("killed", killed->SimulationState().CurrentTick());
  }

  // The universe straight through, its players made anew where the killed universe's came back.
  std::unique_ptr<InProcessServer> straight = Serve(_desc, straightFolder);
  auto recovered = result.recoveredTicks.begin();
  while (straight->SimulationState().CurrentTick() < _desc.ticks)
  {
    for (; recovered != result.recoveredTicks.end() && *recovered == straight->SimulationState().CurrentTick(); ++recovered)
    {
      for (std::size_t seat = 0; seat < straight->MapData().starts.size(); ++seat)
      {
        const PlayerId player{static_cast<std::uint32_t>(seat + 1)};
        straight->ReplaceHosted(player, _desc.makePlayer(player));
      }
    }
    straight->Step();
    report("straight", straight->SimulationState().CurrentTick());
  }
  result.same = killed->SimulationState() == straight->SimulationState();
  return result;
}
