#pragma once

namespace Outpost
{
// Task 3.7's scene for measuring Q4: 200 ships and 40 structures in combat (design §3). It is match setup and upkeep for
// a measurement run only, never for a match.
inline constexpr size_t STRESS_SHIPS_PER_PLAYER = 100;
inline constexpr size_t STRESS_STRUCTURES_PER_PLAYER = 20;

// Each of the map's two players fields its starting designs in turn, gathered at a rally a third of the way from its
// start to the middle, and its structures, with the tuning data's footprints, hit points, armor and Defence guns, beyond
// it; the Command Station a match starts with counts as one of them. Both fleets
// attack-move on the other's rally, so they meet among the structures. Before every tick the load gives each player back
// the ships it lost, at its rally with the same order, and once a second sends ships standing idle after what is left
// of the enemy, so the whole of both fleets keeps fighting however long it is measured. Ships it adds are not in the
// command log, so a stress run does not replay (ADR-009).
class StressLoad
{
public:
  // Places the ships, counting any warships already there, and the structures, counting the player's own. Throws Neuron::Exception when the map has no
  // two starts, a player has no starting design, or there is too little open ground.
  StressLoad(Simulation& _simulation, const Map& _map, const Tuning& _tuning);

  // Tops each player up to STRESS_SHIPS_PER_PLAYER ships, and returns the orders for the ones it added and, on the first
  // call, for every ship, for the tick about to run.
  [[nodiscard]] std::vector<Command> TopUp(Simulation& _simulation);

private:
  struct Side
  {
    PlayerId player;
    PlanePosition start;
    PlanePosition enemyRally;
    std::vector<DesignId> designs;
    // Open ground round the start, nearest first, where ships are placed and replaced.
    std::vector<PlanePosition> berths;
    size_t nextDesign = 0;
    size_t nextBerth = 0;
  };

  [[nodiscard]] EntityId Launch(Simulation& _simulation, Side& _side);

  std::array<Side, 2> m_sides;
  bool m_ordered = false;
};
} // namespace Outpost