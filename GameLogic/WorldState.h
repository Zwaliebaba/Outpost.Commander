#pragma once

namespace Outpost
{
// The version of the state a world's save holds (ADR-077). A change to what Simulation holds raises it: WorldStateTests
// pins the layout of each version, and fails until it is raised and the new layout recorded (AGENTS.md R18). A save of
// another version is refused.
inline constexpr std::uint32_t WORLD_STATE_VERSION = 3;

// The world a save is of: what a server needs, besides the state, to make the simulation the state loads into.
struct WorldIdentity
{
  // The seed the world was made with. The seeded placement of its map follows from it (ADR-072), and so does its PRNG's
  // first state, which the save then replaces.
  std::uint64_t seed = 0;
  std::uint32_t ticksPerSecond = 0;
  // The tuning data's and the map's files, as DataHash hashes them: a save loads only with the data it was made with.
  std::uint64_t dataHash = 0;

  friend bool operator==(const WorldIdentity&, const WorldIdentity&) = default;
};

// What a save's header names: its world and the tick it was made at.
struct SaveHeader
{
  WorldIdentity identity;
  std::uint64_t tick = 0;
};

// 64-bit FNV-1a over each of _texts in turn, each after its length, so that the boundary between two moves the hash.
[[nodiscard]] std::uint64_t DataHash(std::span<const std::string_view> _texts) noexcept;

// A save of _simulation's state, made between two ticks: a header naming the save's kind, WORLD_STATE_VERSION, the world
// and the tick, then the state, then a checksum of all of it (ADR-077).
[[nodiscard]] std::vector<std::byte> EncodeWorld(const Simulation& _simulation, const WorldIdentity& _identity);

// The header of a save. Throws Neuron::Exception when the bytes are not a whole save, with its checksum, of
// WORLD_STATE_VERSION.
[[nodiscard]] SaveHeader ReadSaveHeader(std::span<const std::byte> _bytes);

// Loads a save's state into _simulation, which was made with the save's seed and tick rate and given the tuning data
// (Simulation::LoadState). Throws Neuron::Exception when ReadSaveHeader would, or when the save is of a world other than
// _identity.
void DecodeWorld(std::span<const std::byte> _bytes, const WorldIdentity& _identity, Simulation& _simulation);
} // namespace Outpost
