#include "pch.h"

#include "MiningSystem.h"

#include "Tick.h"
#include "UnloadTarget.h"

#include <algorithm>

namespace Outpost
{

namespace
{
/// **A BOUND ON HOW MANY PHASES ONE MINER CROSSES IN ONE TICK.** Every transition is taken inside the pass,
/// so a miner that is already in range and already full could otherwise go round forever in principle; the
/// loop only ever needs two (arrive then extract, fill then leave, empty then leave), and four is the whole
/// cycle once.
constexpr int MAXIMUM_TRANSITIONS_PER_TICK = 4;

[[nodiscard]] bool Within(const Neuron::Vec2& _a, const Neuron::Vec2& _b, Neuron::Fixed _reach) noexcept
{
  return UniformGrid::DistanceSquared(_a, _b) <= (static_cast<std::int64_t>(_reach) * _reach);
}

/// Heads for _destination unless it already is. **Re-ordering an order that is already right would reset
/// nothing today**, but a movement system that one day eases in would restart every tick.
void HeadFor(World& _world, std::size_t _slot, const Neuron::Vec2& _destination) noexcept
{
  const MoveOrder& order = _world.OrderInSlot(_slot);
  if (order.active && (order.destination == _destination))
  {
    return;
  }
  const Entity& entity = _world.EntityInSlot(_slot);
  // The turn rate is the ship's own (Q59), and the group is kept, so a miner stays in the fleet it was
  // ordered with (Q61).
  static_cast<void>(_world.OrderMoveTo(entity.id, _destination, SpeedPerTick(entity.design), TurnAnglePerTick(entity.design), order.group));
}

void Halt(World& _world, std::size_t _slot) noexcept
{
  _world.OrderInSlot(_slot).active = false;
}
} // namespace

const MiningSystem::UnloadPoint& MiningSystem::UnloadPointFor(const World& _world, const Entity& _acceptor, DesignId _miner)
{
  for (const UnloadPoint& known : m_unloadPoints)
  {
    if ((known.acceptor == _acceptor.id) && (known.miner == _miner))
    {
      return known;
    }
  }
  UnloadPoint answer{.acceptor = _acceptor.id, .miner = _miner};
  answer.farSide = FarSideUnloadPoint(_world, m_grid, _acceptor, _miner, m_scratch, answer.point);
  m_unloadPoints.push_back(answer);
  return m_unloadPoints.back();
}

void MiningSystem::Advance(World& _world, std::span<const EntityId> _struck)
{
  m_deliveries.clear();
  m_unloadPoints.clear();
  m_grid.Rebuild(_world);

  m_struck.assign(_world.SlotCount(), 0);
  for (const EntityId& id : _struck)
  {
    if (_world.IsAlive(id))
    {
      m_struck[id.index] = 1;
    }
  }

  const std::span<const Placement> field = _world.Field();
  m_rockWorked.assign(field.size(), 0);
  const std::size_t slotCount = _world.SlotCount();
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    MineOrder& mine = _world.MineInSlot(slot);
    if (mine.phase == MiningPhase::None)
    {
      continue;
    }

    // A ROCK THE FIELD DOES NOT HAVE ENDS THE ORDER. The intake refuses one, so this is a world whose field
    // changed under a standing order -- a new match -- and the honest answer is to stop, not to guess.
    if (mine.rock >= field.size())
    {
      mine.phase = MiningPhase::None;
      continue;
    }

    const Entity& miner = _world.EntityInSlot(slot);
    const DerivedStats stats = Derive(miner.design);
    const std::uint32_t capacityMilliOre = stats.oreCapacity * MILLI_ORE_PER_ORE;

    // "A SHIP WITH NO MINING TOOL HAS ZERO CAPACITY AND CANNOT BE GIVEN A MINE ORDER" (section 4). The intake
    // refuses one; this is the same rule at the second place it could matter, so no order can cycle empty.
    if (capacityMilliOre == 0)
    {
      mine.phase = MiningPhase::None;
      continue;
    }
    const Neuron::Vec2 rock = field[mine.rock].position;

    // **Q64: FLIGHT.** Fired on while going to the rock or extracting, the miner runs for home with its rock and
    // cargo kept. On the way home or unloading it is already going to the same place, and a move order -- the
    // player's override -- has ended the mine order, so neither flees.
    const bool struck = m_struck[slot] != 0;
    if (struck && ((mine.phase == MiningPhase::ToOre) || (mine.phase == MiningPhase::Extracting)))
    {
      mine.phase = MiningPhase::Fleeing;
      mine.calmTicks = 0;
      mine.unloadTarget = NO_ENTITY;
    }

    for (int transition = 0; transition < MAXIMUM_TRANSITIONS_PER_TICK; ++transition)
    {
      const MiningPhase before = mine.phase;
      switch (mine.phase)
      {
      case MiningPhase::None:
        break;

      case MiningPhase::ToOre:
        // A FULL HOLD GOES STRAIGHT TO UNLOAD -- a miner pulled away full and sent back is not made to sit at
        // the rock extracting nothing.
        if (mine.cargoMilliOre >= capacityMilliOre)
        {
          mine.phase = MiningPhase::ToUnload;
        }
        else if (Within(miner.position, rock, static_cast<Neuron::Fixed>(stats.miningRangeUnits * Neuron::FIXED_ONE)))
        {
          Halt(_world, slot);
          mine.phase = MiningPhase::Extracting;
        }
        else
        {
          HeadFor(_world, slot, rock);
        }
        break;

      case MiningPhase::Extracting:
      {
        // **ONE EXTRACTOR PER ROCK PER TICK** (Q62, ruled 2026-09-24). A second miner in range holds here with
        // its cargo unchanged and takes its turn when the rock is free, so a rock yields at most one laser's
        // rate and stacking miners on the nearest rock stops beating spreading them. **In slot order**, like
        // everything in this pass, so which miner waits is the store's order and never a race (R16).
        if (m_rockWorked[mine.rock] != 0)
        {
          mine.working = false;
          break;
        }
        m_rockWorked[mine.rock] = 1;
        mine.working = true;

        // EXACTLY TO A FULL HOLD, NEVER PAST IT: the last tick's worth is whatever is left of the capacity,
        // so the cargo equals the capacity on the tick it fills rather than overshooting by a rate.
        const std::uint32_t rateMilliOre = (stats.orePerSecond * MILLI_ORE_PER_ORE) / TICKS_PER_SECOND;
        mine.cargoMilliOre = std::min(capacityMilliOre, mine.cargoMilliOre + rateMilliOre);
        if (mine.cargoMilliOre >= capacityMilliOre)
        {
          mine.phase = MiningPhase::ToUnload;
          mine.unloadTarget = NO_ENTITY;
          continue;
        }
        break;
      }

      case MiningPhase::ToUnload:
      {
        const Entity* target = _world.Find(mine.unloadTarget);
        if (target == nullptr)
        {
          mine.unloadTarget = FindUnloadTarget(_world, m_grid, miner.owner, miner.position, m_scratch);
          target = _world.Find(mine.unloadTarget);
        }
        if (target == nullptr)
        {
          // NOWHERE TO UNLOAD: the miner waits where it is, full, and asks again next tick. That is
          // `GameDesign.md` section 4's economy failing where a player can see it.
          Halt(_world, slot);
          break;
        }
        // **Q63: THE FAR SIDE WHEN SOMETHING HOSTILE IS NEAR**, inside point defense and out of a raider's reach;
        // the near side, as before, when nothing is.
        const UnloadPoint& unload = UnloadPointFor(_world, *target, miner.design);
        const bool arrived = unload.farSide ? Within(miner.position, unload.point, FAR_SIDE_SLACK_UNITS * Neuron::FIXED_ONE)
                                            : Within(miner.position, target->position, UnloadReach(miner.design, target->design));
        if (arrived)
        {
          Halt(_world, slot);
          mine.phase = MiningPhase::Unloading;
        }
        else
        {
          HeadFor(_world, slot, unload.farSide ? unload.point : target->position);
        }
        break;
      }

      case MiningPhase::Fleeing:
      {
        if (struck)
        {
          mine.calmTicks = 0;
        }
        else if (mine.calmTicks < FLEE_CALM_TICKS)
        {
          ++mine.calmTicks;
        }
        if (mine.calmTicks >= FLEE_CALM_TICKS)
        {
          // BACK TO THE ROCK, with whatever it was carrying: a full hold goes on to unload from there.
          mine.phase = MiningPhase::ToOre;
          mine.unloadTarget = NO_ENTITY;
          continue;
        }

        const Entity* home = _world.Find(mine.unloadTarget);
        if (home == nullptr)
        {
          mine.unloadTarget = FindUnloadTarget(_world, m_grid, miner.owner, miner.position, m_scratch);
          home = _world.Find(mine.unloadTarget);
        }
        if (home == nullptr)
        {
          // NOWHERE TO RUN TO: it stops, which is as much as a miner can do.
          Halt(_world, slot);
          break;
        }

        // THE SAME PLACE IT WOULD UNLOAD (Q63): the far side while a hostile is near, which is where point defense
        // covers it; touching the near side otherwise.
        const UnloadPoint& refuge = UnloadPointFor(_world, *home, miner.design);
        const bool safe = refuge.farSide ? Within(miner.position, refuge.point, FAR_SIDE_SLACK_UNITS * Neuron::FIXED_ONE)
                                         : Within(miner.position, home->position, UnloadReach(miner.design, home->design));
        if (safe)
        {
          Halt(_world, slot);
        }
        else
        {
          HeadFor(_world, slot, refuge.farSide ? refuge.point : home->position);
        }
        break;
      }

      case MiningPhase::Unloading:
      {
        const Entity* target = _world.Find(mine.unloadTarget);
        if (target == nullptr)
        {
          // The acceptor died mid-unload. Back to looking for one, with what is left aboard.
          mine.phase = MiningPhase::ToUnload;
          mine.unloadTarget = NO_ENTITY;
          continue;
        }

        // **RE-CHECKED EVERY TICK** (Q64): a hostile arriving on this side moves the unload point, and the miner
        // goes round to it with what is left aboard.
        const UnloadPoint& unload = UnloadPointFor(_world, *target, miner.design);
        if (unload.farSide && !Within(miner.position, unload.point, FAR_SIDE_SLACK_UNITS * Neuron::FIXED_ONE))
        {
          mine.phase = MiningPhase::ToUnload;
          continue;
        }

        const std::uint32_t rateMilliOre = (UNLOAD_ORE_PER_SECOND * MILLI_ORE_PER_ORE) / TICKS_PER_SECOND;
        const std::uint32_t moved = std::min(mine.cargoMilliOre, rateMilliOre);
        mine.cargoMilliOre -= moved;
        if (moved > 0)
        {
          m_deliveries.push_back(OreDelivery{.player = miner.owner, .miner = miner.id, .acceptor = target->id, .milliOre = moved});
        }
        if (mine.cargoMilliOre == 0)
        {
          // AND BACK TO THE SAME ROCK, on the tick the hold empties.
          mine.phase = MiningPhase::ToOre;
          mine.unloadTarget = NO_ENTITY;
          continue;
        }
        break;
      }
      }

      if (mine.phase == before)
      {
        break;
      }
    }
  }
}

} // namespace Outpost
