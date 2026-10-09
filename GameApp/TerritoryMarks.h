#pragma once

namespace Outpost
{
// The territory as the world shows it (interface plan 2, task UI1.1): the lattice of sector borders, each drawn once; an
// outline inside each held or guarded sector, in its holder's look, in a pattern that says whether it is suppressed or
// cut off; and a mark on every node. Every player sees the territory, fog or not (ADR-056 decision 10), so GameClient
// draws it over the fog, as the minimap draws its outlines. Pure, so that it is tested without a GPU.
struct TerritoryMarks
{
  // Whose a line or a node is, which GameClient colors it by.
  enum class Look : std::uint8_t
  {
    // Nobody's: the lattice, and a node no one holds that the player cannot claim now.
    Neutral,
    // Its holder's.
    Held,
    // The pirates' (ADR-073).
    Guarded,
    // A free node the player could claim now, by the Relay ghost's rule (CanClaim).
    Claimable
  };

  // How a held sector's outline runs, so that its state never rests on its color alone.
  enum class Pattern : std::uint8_t
  {
    Solid,
    // Suppressed: an enemy warship stands at its Relay, and it earns nothing (ADR-056 decision 6).
    Dashed,
    // Cut off from its holder's home, and it earns the tuning data's share (ADR-056 decision 7).
    Dotted
  };

  struct Line
  {
    PlanePosition from;
    PlanePosition to;
    Look look = Look::Neutral;
    // The holder of a held sector's outline, PIRATES for a guarded one's, and no player for the lattice.
    PlayerId holder;
    Pattern pattern = Pattern::Solid;

    friend bool operator==(const Line&, const Line&) = default;
  };

  struct Node
  {
    PlanePosition position;
    Look look = Look::Neutral;
    // Its holder, PIRATES while they guard it, or the player for a node it could claim; no player for a neutral one.
    PlayerId holder;

    friend bool operator==(const Node&, const Node&) = default;
  };

  // The lattice first, then each held or guarded sector's outline, in the map's order of sectors; a node for each sector.
  std::vector<Line> lines;
  std::vector<Node> nodes;

  friend bool operator==(const TerritoryMarks&, const TerritoryMarks&) = default;
};

// How far inside its sector a held or guarded sector's outline runs, so that where two holders' sectors meet both outlines
// show, one either side of the lattice's line.
inline constexpr float TERRITORY_INSET_METERS = 4.0f;

// The marks of _newest's sectors as its player sees them, the nodes it could claim judged against _entities, as the Relay's
// ghost judges them. None on a map without sectors.
[[nodiscard]] TerritoryMarks MarkTerritory(const Snapshot& _newest, std::span<const EntityView> _entities);
} // namespace Outpost
