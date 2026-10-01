#pragma once

namespace Outpost
{
// The structures of design §6. Each behaves differently, so they are kinds in code; their numbers are tuning data.
enum class StructureKind : std::uint8_t
{
  CommandStation,
  Shipyard,
  ResearchLab,
  MiningRig,
  DefensePlatform
};

// A Mining Rig ordered onto an ore asteroid, or within this of its edge, snaps to the asteroid's center (design §6). The
// server places it so, and the client's ghost shows it so.
inline constexpr float RIG_SNAP_METERS = 40.0f;
} // namespace Outpost