#pragma once

namespace Outpost
{
// Task 2.7's load for measuring the tick half of Q4: 200 ships and 40 structures on the map (design §3). It is match
// setup for a measurement run only, never for a match.
inline constexpr size_t MEASUREMENT_SHIPS = 200;
inline constexpr size_t MEASUREMENT_STRUCTURES = 40;

// Adds ships and structures until the world holds MEASUREMENT_SHIPS and MEASUREMENT_STRUCTURES, counting the starting
// bases already placed. They stand on a lattice over the open parts of the map, clear of the obstacles, the edge and the
// starts, each on its owner's half of the map, and cycle through the tuning data's hulls on its first drive and through
// the structure kinds. The structures are out of combat, and block movement as every structure does (ADR-016). Throws
// Neuron::Exception if the map has too little open ground for them.
void PlaceMeasurementLoad(Simulation& _simulation, const Map& _map, const Tuning& _tuning);
} // namespace Outpost
