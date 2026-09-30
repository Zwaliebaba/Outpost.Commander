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
} // namespace Outpost
