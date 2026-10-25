#pragma once

namespace Outpost
{
// The structures (Game §structures.kinds). Each behaves differently, so they are kinds in code; their numbers are tuning data.
enum class StructureKind : std::uint8_t
{
  CommandStation,
  Shipyard,
  ResearchLab,
  MiningRig,
  DefensePlatform,
  // Holds the sector whose node it stands on (ADR-056 decision 3).
  Relay,
  // Repairs the player's ships near it, for nothing (ADR-075 decision 3).
  RepairBay
};

// A Mining Rig ordered onto an ore asteroid, or within this of its edge, snaps to the asteroid's center (ADR-016 decision 4). The
// server places it so, and the client's ghost shows it so.
inline constexpr float RIG_SNAP_METERS = 40.0f;
} // namespace Outpost