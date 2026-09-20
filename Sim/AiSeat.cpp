#include "pch.h"

#include "AiSeat.h"

#include "Construction.h"
#include "Movement.h"
#include "Placement.h"
#include "Plan.h"
#include "Sim.h"

#include <algorithm>

namespace Outpost
{

namespace
{
/// A subunit coordinate as the cell it is in. The same spelling AiBlackboard.cpp uses, because a
/// seat that decided against one grid and ordered against another would be ordering somewhere it
/// had not looked.
[[nodiscard]] std::uint32_t CellOf(std::int32_t _subunits) noexcept
{
  return _subunits <= 0 ? 0u : static_cast<std::uint32_t>(_subunits / Neuron::SUBUNITS_PER_CELL);
}

/// The row of the first structure with this role, or NO_ROW.
inline constexpr std::uint32_t NO_ROW = 0xFFFFFFFFu;

[[nodiscard]] std::uint32_t RowWithRole(const Sim& _sim, std::uint8_t _seat, StructureRole _role) noexcept
{
  const ContentTree& content = _sim.Content();
  const Seat& seat = _sim.Seats()[_seat];
  for (std::uint32_t row = 0; row < content.structures.structures.size(); ++row)
  {
    const StructureDesc& desc = content.structures.structures[row];
    if (desc.role == _role && UnlockedFor(seat, content, desc.unlockedBy))
    {
      return row;
    }
  }
  return NO_ROW;
}

void Submit(Sim& _sim, std::uint8_t _seat, OrderKind _kind, std::int32_t _a = 0, std::int32_t _b = 0, std::int32_t _c = 0,
            std::int32_t _d = 0)
{
  Order order{};
  order.tick = _sim.Tick() + AI_ORDER_DELAY_TICKS;
  order.seat = _seat;
  order.kind = _kind;
  order.operands = {_a, _b, _c, _d};
  _sim.Submit(order);
}

/// True when this commander could place this structure here. The same check order validation makes,
/// asked before the order rather than after it: a scripted commander that fired off placements and
/// let them be refused would fill the rejection list every decision, and a rejection list is hashed.
[[nodiscard]] bool CanPlace(const Sim& _sim, std::uint8_t _seat, std::uint32_t _row, std::uint32_t _cellX, std::uint32_t _cellY)
{
  const ContentTree& content = _sim.Content();
  if (_row >= content.structures.structures.size())
  {
    return false;
  }
  const StructureDesc& desc = content.structures.structures[_row];
  const Seat& seat = _sim.Seats()[_seat];
  if (seat.powerHundredths < desc.costHundredths || seat.structureCount >= seat.structureCap)
  {
    return false;
  }
  if (PlanCount(_sim.Objects(), _seat) >= MAX_PLANS_PER_SEAT)
  {
    return false;
  }
  const Footprint footprint = FootprintAt(desc, _cellX, _cellY);
  const PlacementQuery query{&_sim.Terrain(), &_sim.Objects(), &seat, &desc, &content, &_sim.Power().Deposits()};
  return CheckPlacement(footprint, query) == PlacementFault::Accepted;
}

/// A free spot for a structure, spiralling out from a cell. Square rings rather than a real spiral:
/// the order is the same on every machine and that is all a placement needs.
[[nodiscard]] bool SpotNear(const Sim& _sim, std::uint8_t _seat, std::uint32_t _row, std::uint32_t _cellX, std::uint32_t _cellY,
                            std::uint32_t _rings, std::uint32_t& _outX, std::uint32_t& _outY)
{
  for (std::uint32_t ring = 0; ring <= _rings; ++ring)
  {
    for (std::int32_t offsetY = -static_cast<std::int32_t>(ring); offsetY <= static_cast<std::int32_t>(ring); ++offsetY)
    {
      for (std::int32_t offsetX = -static_cast<std::int32_t>(ring); offsetX <= static_cast<std::int32_t>(ring); ++offsetX)
      {
        const bool onTheRing =
          static_cast<std::uint32_t>(std::abs(offsetX)) == ring || static_cast<std::uint32_t>(std::abs(offsetY)) == ring;
        if (!onTheRing)
        {
          continue;
        }
        const std::int64_t x = static_cast<std::int64_t>(_cellX) + offsetX;
        const std::int64_t y = static_cast<std::int64_t>(_cellY) + offsetY;
        if (x < 0 || y < 0)
        {
          continue;
        }
        const auto cellX = static_cast<std::uint32_t>(x);
        const auto cellY = static_cast<std::uint32_t>(y);
        if (CanPlace(_sim, _seat, _row, cellX, cellY))
        {
          _outX = cellX;
          _outY = cellY;
          return true;
        }
      }
    }
  }
  return false;
}

/// SaveDesign's operand 3 packs up to four module rows, one per byte (Sim/Order.h).
[[nodiscard]] std::int32_t PackModules(const DeviceDesign& _design) noexcept
{
  std::uint32_t packed = 0;
  for (std::uint8_t index = 0; index < 4; ++index)
  {
    const std::uint32_t row = index < _design.moduleCount ? _design.modules[index] : NO_PACKED_MODULE;
    packed |= (row & 0xFFu) << (8 * index);
  }
  return static_cast<std::int32_t>(packed);
}

/// Where this commander's command post is, or his first structure, or nothing.
[[nodiscard]] bool HomeCell(const Sim& _sim, std::uint8_t _seat, std::uint32_t& _outX, std::uint32_t& _outY)
{
  bool found = false;
  _sim.Objects().ForEachStructure(
    [&](ObjectId, const Structure& _structure)
    {
      if (found || _structure.seat != _seat)
      {
        return;
      }
      _outX = _structure.cellX;
      _outY = _structure.cellY;
      found = true;
    });
  if (found)
  {
    return true;
  }
  _sim.Objects().ForEachDevice(
    [&](ObjectId, const Device& _device)
    {
      if (found || _device.seat != _seat)
      {
        return;
      }
      _outX = CellOf(_device.x);
      _outY = CellOf(_device.z);
      found = true;
    });
  return found;
}

// ── The behaviours, in the order the header lists them ──────────────────────────────────────

[[nodiscard]] bool Design(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard)
{
  const Seat& seat = _sim.Seats()[_seat];
  // Index 0 is the builder design and index 1 the fighter, always, so that the produce behaviour
  // names a slot rather than searching for one.
  for (std::uint32_t index = 0; index < 2; ++index)
  {
    if (index < seat.designs.size())
    {
      continue;
    }
    DeviceDesign design{};
    if (!BestDesign(_sim, _seat, index == 0 ? AiRole::Builder : AiRole::Fighter, _blackboard, design))
    {
      continue;
    }
    Submit(_sim, _seat, OrderKind::SaveDesign, static_cast<std::int32_t>(index), static_cast<std::int32_t>(design.chassis),
           static_cast<std::int32_t>(design.drive), PackModules(design));
    return true;
  }

  // And a fighter design that no longer suits what it is meeting: re-designed against the
  // composition it has actually seen, which is what the damage matrix is for.
  if (seat.designs.size() >= 2 && _blackboard.enemyDevicesSeen > 0)
  {
    DeviceDesign wanted{};
    if (BestDesign(_sim, _seat, AiRole::Fighter, _blackboard, wanted) && !(wanted == seat.designs[1]))
    {
      Submit(_sim, _seat, OrderKind::SaveDesign, 1, static_cast<std::int32_t>(wanted.chassis), static_cast<std::int32_t>(wanted.drive),
             PackModules(wanted));
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool Build(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard, StructureRole _role, std::uint32_t _wanted,
                         std::uint32_t _nearX, std::uint32_t _nearY, std::uint32_t _rings)
{
  const std::uint32_t row = RowWithRole(_sim, _seat, _role);
  if (row == NO_ROW)
  {
    return false;
  }
  AiNeed need{};
  switch (_role)
  {
  case StructureRole::CommandPost:
    need = AiNeed::CommandPost;
    break;
  case StructureRole::Extractor:
    need = AiNeed::Extractor;
    break;
  case StructureRole::Generator:
    need = AiNeed::Generator;
    break;
  case StructureRole::Factory:
    need = AiNeed::Factory;
    break;
  default:
    need = AiNeed::Lab;
    break;
  }
  if (_blackboard.standing[static_cast<std::size_t>(need)] >= _wanted)
  {
    return false;
  }
  std::uint32_t cellX = 0;
  std::uint32_t cellY = 0;
  if (!SpotNear(_sim, _seat, row, _nearX, _nearY, _rings, cellX, cellY))
  {
    return false;
  }
  Submit(_sim, _seat, OrderKind::PlaceStructure, static_cast<std::int32_t>(row), static_cast<std::int32_t>(cellX),
         static_cast<std::int32_t>(cellY));
  return true;
}

[[nodiscard]] bool Scout(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard, const CellPosition& _deposit);

/// How far this device's builder modules reach, in subunits; 0 when it carries none.
[[nodiscard]] std::int64_t BuilderReachSubunits(const Sim& _sim, std::uint8_t _seat, const Device& _device) noexcept
{
  const Seat& seat = _sim.Seats()[_seat];
  if (_device.design >= seat.designs.size())
  {
    return 0;
  }
  const ContentTree& content = _sim.Content();
  const DeviceDesign& design = seat.designs[_device.design];
  std::int64_t reach = 0;
  for (std::uint8_t index = 0; index < design.moduleCount && index < MAX_MOUNTS; ++index)
  {
    const std::uint32_t row = design.modules[index];
    if (row < content.components.modules.size() && content.components.modules[row].systemKind == SystemKind::Builder)
    {
      reach = std::max<std::int64_t>(reach, content.components.modules[row].systemRangeSubunits);
    }
  }
  return reach;
}

/// The drive a device's design walks on; Wheels when the design cannot be read, which is the
/// class every landscape has most of and so the least misleading guess.
/// Whether a device is standing where it was last SENT, having failed to get there: at Stop, with
/// a destination it is nowhere near.
///
/// THIS IS THE ONLY MEMORY A DECISION HAS, and it is the device's own. A behaviour runs from a
/// blackboard rebuilt every decision, so nothing carries over from the last one - but Device keeps
/// its destination when Sim/Movement.cpp gives up on a route it cannot plan (PathState::Unreachable
/// or Refused set Stop and drop the route; neither touches the destination). So a builder at Stop
/// whose destination is the very cell a behaviour is about to order it to again is a builder that
/// has already tried and failed, and ordering it there a second time is the deadlock rather than
/// the cure. A device that ARRIVED is at Stop too, and is excluded by being within ARRIVAL_SUBUNITS
/// of the destination it reached.
[[nodiscard]] bool AlreadyFailedToReach(const Device& _device, std::int32_t _x, std::int32_t _z) noexcept
{
  if (_device.primaryOrder != PrimaryOrder::Stop || _device.destinationX != _x || _device.destinationZ != _z)
  {
    return false;
  }
  return Neuron::LengthSquared(_device.destinationX - _device.x, _device.destinationZ - _device.z) >
         static_cast<std::int64_t>(ARRIVAL_SUBUNITS) * ARRIVAL_SUBUNITS;
}

/// Where to send a device so that it ends up within _reachSubunits of a footprint WITHOUT standing
/// on it, on ground its own drive can hold, and not at a cell it has already failed to reach.
/// False when there is no such cell, which is the answer that lets a caller pass the site over.
///
/// WHY A DEVICE MUST NEVER BE SENT ONTO A FOOTPRINT, which is the defect this function exists for.
/// A structure occupies its cells from the tick construction begins (Sim/Plan.h's Occupies), so
/// those cells leave the cluster graph. The planner then refuses the route, Sim/Movement.cpp falls
/// the device back to Stop, and the behaviour that sent it there - seeing an idle builder and an
/// unfinished site - sends it to the same cell on the next decision, and the next, for the rest of
/// the match. It answers before the generator, the factory and the lab, so the commander stops
/// building anything at all. Measured on the slice landscape: 11,500 of 12,000 ticks spent
/// ordering one truck at a cell it could never reach.
///
/// IT IS A CLASS AND NOT AN INSTANCE, which is why this is one function rather than a fix inside
/// Finish. Every behaviour that issues a Move and returns true starves everything below it when
/// the Move cannot succeed, and Scout has exactly Finish's shape: it walks a builder to a deposit
/// cell that may be a summit no drive can climb. Both call this.
///
/// REACHABILITY IS NOT TESTED HERE AND MUST NOT BE. ClusterGraph::ComponentAt returns a component
/// local to a 16-by-16 cluster, so two cells a truck can drive between are in different components
/// as soon as they are in different clusters; equality would refuse almost every real site. What is
/// tested is that the cell is IN the graph at all - passable, and not the NO_COMPONENT of ground no
/// drive of this class can hold. Whether a route exists is the planner's question and it answers it
/// over several ticks, which one decision cannot wait for; AlreadyFailedToReach is how the answer
/// gets back here afterwards.
[[nodiscard]] bool ApproachSpot(const Sim& _sim, std::uint8_t _seat, const Device& _device, const Footprint& _footprint,
                                std::int64_t _reachSubunits, std::int32_t& _outX, std::int32_t& _outZ)
{
  if (_reachSubunits <= 0)
  {
    return false;
  }
  const DriveClass drive = DriveOf(_sim, _seat, _device);
  const std::uint32_t side = _sim.Terrain().CellsPerSide();
  // Rings outward from the footprint's edge, so the nearest legal cell wins and the order is a
  // function of the state alone. The last ring is the one whose nearest corner is still in reach.
  const std::uint32_t maxRings = static_cast<std::uint32_t>(_reachSubunits / Neuron::SUBUNITS_PER_CELL) + 1;
  for (std::uint32_t ring = 1; ring <= maxRings; ++ring)
  {
    const std::int64_t x0 = static_cast<std::int64_t>(_footprint.cellX) - ring;
    const std::int64_t y0 = static_cast<std::int64_t>(_footprint.cellY) - ring;
    const std::int64_t x1 = static_cast<std::int64_t>(_footprint.cellX) + _footprint.cellsX - 1 + ring;
    const std::int64_t y1 = static_cast<std::int64_t>(_footprint.cellY) + _footprint.cellsY - 1 + ring;
    for (std::int64_t y = y0; y <= y1; ++y)
    {
      for (std::int64_t x = x0; x <= x1; ++x)
      {
        // The ring itself and not the block it bounds: the inner cells were tested by a smaller
        // ring, and the footprint's own cells are what this function exists to refuse.
        if (x != x0 && x != x1 && y != y0 && y != y1)
        {
          continue;
        }
        if (x < 0 || y < 0 || x >= static_cast<std::int64_t>(side) || y >= static_cast<std::int64_t>(side))
        {
          continue;
        }
        const std::uint32_t cellX = static_cast<std::uint32_t>(x);
        const std::uint32_t cellY = static_cast<std::uint32_t>(y);
        if (!_sim.Clusters().Passable(cellX, cellY, drive) || _sim.Clusters().ComponentAt(cellX, cellY, drive) == NO_COMPONENT)
        {
          continue;
        }
        const std::int32_t spotX = static_cast<std::int32_t>(cellX) * Neuron::SUBUNITS_PER_CELL + Neuron::SUBUNITS_PER_CELL / 2;
        const std::int32_t spotZ = static_cast<std::int32_t>(cellY) * Neuron::SUBUNITS_PER_CELL + Neuron::SUBUNITS_PER_CELL / 2;
        // Stage 5's own rule, from Sim/Placement.h, so that a builder sent here is a builder stage
        // 5 counts.
        if (DistanceSquaredTo(_footprint, spotX, spotZ) > _reachSubunits * _reachSubunits)
        {
          continue;
        }
        if (AlreadyFailedToReach(_device, spotX, spotZ))
        {
          continue; // It has been sent here and could not get here; the next cell, or no cell.
        }
        _outX = spotX;
        _outZ = spotZ;
        return true;
      }
    }
  }
  return false;
}

/// Cancels a plan the ground will no longer take.
///
/// A PLAN DOES NOT OCCUPY ITS CELLS (Sim/Plan.h), which is what makes this possible: two plans may
/// overlap, and the first to BEGIN construction flattens its footprint and marks it, which leaves
/// the other refused by CheckPlacement for as long as it exists. Sim/Construction.cpp says exactly
/// what happens then - "a plan that can no longer be built stays a plan until it is cancelled" -
/// and that is the right rule for a human, who can see the thing and press the button. Nothing was
/// doing the cancelling for a scripted commander.
///
/// SO IT STARVED ITSELF ON ITS OWN BOOKKEEPING. AiBlackboard counts what a commander holds "built
/// or building", so a dead plan still counted as the lab it was going to be; the INDUSTRY behaviour
/// therefore never placed another, and the site sat at zero effort with two builders standing
/// beside it, in reach, with the power to build it. Measured on the slice landscape at the raised
/// eye: seat 0 finished the match with a research lab that had been a Plan since tick 4,000 and a
/// CheckPlacement of Occupied, and no second lab was ever placed.
///
/// IT IS FIRST IN THE LIST because a plan that cannot be built is not work, and every behaviour
/// below it reasons about a count this one corrects.
[[nodiscard]] bool Abandon(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard)
{
  const ContentTree& content = _sim.Content();
  for (const ObjectId& id : _blackboard.unfinished)
  {
    const Structure* structure = _sim.Objects().FindStructure(id);
    if (structure == nullptr || structure->state != StructurePhase::Plan)
    {
      continue; // Only a plan: one already under construction has paid and holds its ground.
    }
    if (structure->design >= content.structures.structures.size())
    {
      continue;
    }
    const Footprint footprint = FootprintOf(*structure, &content);
    const PlacementQuery query{
      &_sim.Terrain(), &_sim.Objects(),         &_sim.Seats()[_seat], &content.structures.structures[structure->design],
      &content,        &_sim.Power().Deposits()};
    if (CheckPlacement(footprint, query) == PlacementFault::Accepted)
    {
      continue;
    }
    // The same question stage 5 asks before it begins one, so the commander cancels exactly the
    // plans stage 5 would refuse - no guess about which faults are permanent, and no plan cancelled
    // that stage 5 would have built.
    Submit(_sim, _seat, OrderKind::CancelStructure, static_cast<std::int32_t>(id.value));
    return true;
  }
  return false;
}

/// Walks an idle builder to the oldest thing this commander has placed and not finished. A plan
/// nobody is standing next to is a plan that never goes up, and a commander with one truck and six
/// plans finishes none of them - which is what the first run of this AI actually did.
[[nodiscard]] bool Finish(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard)
{
  // EVERY UNFINISHED SITE, NOT JUST THE OLDEST. This walked only unfinishedPlaces.front() and
  // answered true for having ordered a truck at it, so one site no builder could be got to stopped
  // the commander from finishing any of the others AND from reaching the behaviours below - the
  // generator, the factory, the lab. The slack roomToBuild carries for exactly that case ("a
  // deposit on a summit no drive can climb") could never be used, because Finish answered first.
  // Now a site with nowhere to stand beside it is passed over and the next one is tried.
  for (std::size_t index = 0; index < _blackboard.unfinished.size() && index < _blackboard.unfinishedPlaces.size(); ++index)
  {
    const Structure* structure = _sim.Objects().FindStructure(_blackboard.unfinished[index]);
    if (structure == nullptr)
    {
      continue;
    }
    const Footprint footprint = FootprintOf(*structure, &_sim.Content());
    for (const ObjectId& builder : _blackboard.builderDevices)
    {
      const Device* device = _sim.Objects().FindDevice(builder);
      if (device == nullptr || device->primaryOrder != PrimaryOrder::Stop)
      {
        continue;
      }
      const std::int64_t reach = BuilderReachSubunits(_sim, _seat, *device);
      // Already in reach: construction is stage 5's and needs no order, so there is nothing for
      // this behaviour to do and the list moves on. It is stage 5's OWN test now - the same
      // DistanceSquaredTo against the same footprint and the same range - where it used to be the
      // distance to the site's corner against half the reach. The half was a correction for
      // measuring to a corner; measuring to the footprint, there is nothing to correct.
      if (reach > 0 && DistanceSquaredTo(footprint, device->x, device->z) <= reach * reach)
      {
        return false;
      }
      std::int32_t x = 0;
      std::int32_t z = 0;
      if (!ApproachSpot(_sim, _seat, *device, footprint, reach, x, z))
      {
        continue; // Not this builder, and perhaps not this site; the loops decide which.
      }
      Submit(_sim, _seat, OrderKind::Move, static_cast<std::int32_t>(builder.value), x, z);
      return true;
    }
  }
  return false;
}

/// Claiming deposits, which is two behaviours that have to be one: a deposit must be STOOD ON by
/// its extractor, so a spot cannot be found near it - the list of deposits is the list of spots.
/// The commander therefore walks it in order and takes the first it can build on, walking a builder
/// to the first it has not SEEN yet when it reaches one, because CheckPlacement refuses an
/// unexplored footprint (GameDesign.md §5's "anywhere the commander has explored"). A deposit it
/// has seen and cannot build on - a summit too steep for a footprint - is skipped for good.
[[nodiscard]] bool Expand(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard)
{
  if (_blackboard.standing[static_cast<std::size_t>(AiNeed::Extractor)] >= AI_WANTED_EXTRACTORS)
  {
    return false;
  }
  const std::uint32_t row = RowWithRole(_sim, _seat, StructureRole::Extractor);
  if (row == NO_ROW)
  {
    return false;
  }
  const Seat& seat = _sim.Seats()[_seat];
  for (const CellPosition& deposit : _blackboard.freeDeposits)
  {
    if (CanPlace(_sim, _seat, row, deposit.x, deposit.y))
    {
      Submit(_sim, _seat, OrderKind::PlaceStructure, static_cast<std::int32_t>(row), static_cast<std::int32_t>(deposit.x),
             static_cast<std::int32_t>(deposit.y));
      return true;
    }
    if (seat.fog.Inside(deposit.x, deposit.y) && seat.fog.Explored(deposit.x, deposit.y))
    {
      continue; // Seen, and the ground refuses it: there is no walking that will change the answer.
    }
    return Scout(_sim, _seat, _blackboard, deposit);
  }
  return false;
}

/// A deposit a commander has never seen cannot be built on - CheckPlacement refuses an unexplored
/// footprint, which is GameDesign.md §5's "anywhere the commander has explored". So the commander
/// walks to it first. That is a behaviour rather than an oversight in the list: a scripted seat
/// that could place on ground it had not seen would be reading the map through the fog, which is
/// exactly what the placement rule exists to stop.
[[nodiscard]] bool Scout(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard, const CellPosition& _deposit)
{
  // A builder that is standing still. One that is already walking is on its way somewhere, and
  // re-ordering it every decision is how a scripted commander walks in circles.
  // The deposit's own cell, as a one-cell footprint, so that the guard Finish uses reads the same
  // here: a deposit on a summit no drive can climb is walked to from BESIDE it rather than ordered
  // at for the rest of the match. Scout has Finish's shape - a Move and a true - so it has Finish's
  // deadlock, and the guard belongs in one place rather than copied into each.
  const Footprint deposit = {_deposit.x, _deposit.y, 1, 1};
  for (const ObjectId& builder : _blackboard.builderDevices)
  {
    const Device* device = _sim.Objects().FindDevice(builder);
    if (device == nullptr || device->primaryOrder != PrimaryOrder::Stop)
    {
      continue;
    }
    // Close enough to SEE it, which is what scouting is for: CheckPlacement refuses an unexplored
    // footprint and a cell a viewer is standing next to is explored. The builder's own reach would
    // be the wrong number here - a builder module reaches two cells and the point is the fog, not
    // the building - so it is the sight of what is doing the looking.
    const std::int64_t reach = static_cast<std::int64_t>(SCOUT_APPROACH_CELLS) * Neuron::SUBUNITS_PER_CELL;
    std::int32_t x = 0;
    std::int32_t z = 0;
    if (!ApproachSpot(_sim, _seat, *device, deposit, reach, x, z))
    {
      continue;
    }
    Submit(_sim, _seat, OrderKind::Move, static_cast<std::int32_t>(builder.value), x, z);
    return true;
  }
  return false; // Every builder is busy or cannot be got there; the deposit waits for one.
}

[[nodiscard]] bool Produce(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard)
{
  if (_blackboard.idleFactories.empty() || _sim.Seats()[_seat].designs.size() < 2)
  {
    return false;
  }
  const Seat& seat = _sim.Seats()[_seat];
  if (seat.deviceCount >= seat.deviceCap)
  {
    return false;
  }
  // Two builders first, then fighters. A commander with no builder cannot rebuild what it loses,
  // which is the one way a scripted seat gets stuck for the rest of a match.
  const std::uint32_t design = _blackboard.builders < AI_WANTED_BUILDERS ? 0u : 1u;
  if (design >= seat.designs.size())
  {
    return false;
  }
  Submit(_sim, _seat, OrderKind::SetProduction, static_cast<std::int32_t>(_blackboard.idleFactories.front().value),
         static_cast<std::int32_t>(design), 1);
  return true;
}

/// Where to send a group when the commander has seen nothing yet: the START POSITION furthest from
/// its own base. The landscape's definition is public to every commander from the first tick
/// (TechnicalDesign.md §5.2 names that as the one leak this model keeps), so a scripted commander
/// knowing where the other seats start is exactly what a human knows - and without it two
/// commanders on opposite corners of a landscape never meet at all, which is what the first run of
/// this AI did for twelve thousand ticks.
/// _target moved _cells towards (_homeX, _homeY), never past it and never off the landscape's
/// lower edge. Integer throughout, so two hosts aim at the same cell.
[[nodiscard]] CellPosition ShortOf(const CellPosition& _target, std::uint32_t _homeX, std::uint32_t _homeY, std::uint32_t _cells) noexcept
{
  const std::int64_t dx = static_cast<std::int64_t>(_homeX) - _target.x;
  const std::int64_t dy = static_cast<std::int64_t>(_homeY) - _target.y;
  const std::int64_t length = Neuron::Length(static_cast<std::int32_t>(dx), static_cast<std::int32_t>(dy));
  if (length <= static_cast<std::int64_t>(_cells))
  {
    return _target;
  }
  const std::int64_t x = static_cast<std::int64_t>(_target.x) + dx * _cells / length;
  const std::int64_t y = static_cast<std::int64_t>(_target.y) + dy * _cells / length;
  return {static_cast<std::uint32_t>(std::max<std::int64_t>(x, 0)), static_cast<std::uint32_t>(std::max<std::int64_t>(y, 0))};
}

[[nodiscard]] bool EnemyStart(const Sim& _sim, std::uint32_t _homeX, std::uint32_t _homeY, CellPosition& _out)
{
  std::int64_t furthest = -1;
  for (const CellPosition& start : _sim.Terrain().Definition().starts)
  {
    const std::int64_t dx = static_cast<std::int64_t>(start.x) - _homeX;
    const std::int64_t dy = static_cast<std::int64_t>(start.y) - _homeY;
    const std::int64_t distance = dx * dx + dy * dy;
    if (distance > furthest)
    {
      furthest = distance;
      _out = start;
    }
  }
  return furthest > 0;
}

/// The nearest cell to _wanted that _drive can STAND on, ringing outward. False when there is none
/// inside _maxRings.
///
/// IT ANSWERS WHERE A DEVICE MAY STAND AND NOT WHERE IT MAY GET TO, and the difference is worth
/// spelling out because the first version of this tried to answer both. ClusterGraph's components
/// are per CLUSTER - a flood fill over at most 256 cells - so two cells of one landmass sixty
/// cells apart have different component numbers, and a seat that compared them refused every
/// attack it ever considered: measured, both commanders' armies stayed within four cells of home
/// for twenty thousand ticks, which is worse than the defect this was written to fix.
///
/// Reachability is a SEARCH, and the planner is what does it. A group sent to ground it cannot
/// reach has its route refused and stands where it was, which is what it did before; what this
/// stops is the case S14 measured, where the ground itself was never standable - the cell four
/// back from an enemy start is inside his buildings once his base has grown, and no route to it
/// can exist for anybody.
///
/// RINGS OUTWARD, so the answer is the nearest such cell and is a function of the state alone -
/// the same shape ApproachSpot above uses for a builder, and for the same determinism.
[[nodiscard]] bool StandableNear(const Sim& _sim, DriveClass _drive, const CellPosition& _wanted, std::uint32_t _maxRings,
                                 CellPosition& _out)
{
  const auto side = static_cast<std::int64_t>(_sim.Terrain().CellsPerSide());
  for (std::uint32_t ring = 0; ring <= _maxRings; ++ring)
  {
    const std::int64_t x0 = static_cast<std::int64_t>(_wanted.x) - ring;
    const std::int64_t y0 = static_cast<std::int64_t>(_wanted.y) - ring;
    const std::int64_t x1 = static_cast<std::int64_t>(_wanted.x) + ring;
    const std::int64_t y1 = static_cast<std::int64_t>(_wanted.y) + ring;
    for (std::int64_t y = y0; y <= y1; ++y)
    {
      for (std::int64_t x = x0; x <= x1; ++x)
      {
        // The ring itself and not the block it bounds; a smaller ring has already tested the rest.
        if (ring > 0 && x != x0 && x != x1 && y != y0 && y != y1)
        {
          continue;
        }
        if (x < 0 || y < 0 || x >= side || y >= side)
        {
          continue;
        }
        const auto cellX = static_cast<std::uint32_t>(x);
        const auto cellY = static_cast<std::uint32_t>(y);
        // ONE TEST AND NOT TWO. ComponentAt answers NO_COMPONENT exactly where Passable answers
        // false - that is what the component of an impassable cell is - so asking both is asking
        // the same question twice, and a reader who saw two would look for the difference.
        if (_sim.Clusters().ComponentAt(cellX, cellY, _drive) == NO_COMPONENT)
        {
          continue;
        }
        _out = {cellX, cellY};
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] bool Attack(Sim& _sim, std::uint8_t _seat, const AiBlackboard& _blackboard, std::uint32_t _homeX, std::uint32_t _homeY)
{
  if (_blackboard.idleFighters.size() < ATTACK_GROUP_SIZE)
  {
    return false;
  }
  CellPosition target{};
  if (!_blackboard.knownEnemyPlaces.empty())
  {
    target = _blackboard.knownEnemyPlaces.front();
  }
  else if (!EnemyStart(_sim, _homeX, _homeY, target))
  {
    return false;
  }
  // A few cells short of it, on the line from home. The cell a commander starts on is where his
  // command post stands, and a structure's own cells are impassable - so a route to the middle of
  // an enemy base is unreachable and the group stops where it was built. Short of it is also where
  // a group meets what is defending the base, which is the point of sending one.
  target = ShortOf(target, _homeX, _homeY, APPROACH_CELLS);

  // AND THEN GROUND IT CAN ACTUALLY STAND ON (m1-vertical-slice/S14). Four cells back was enough
  // for a bare start cell and is not enough for a base that has grown: measured on the slice
  // landscape at seed 1, the group was sent to (38,90), which is inside the enemy's own buildings
  // and impassable to everybody; the route came back unreachable one tick later, the seat
  // re-issued the same refused order every ten ticks for the rest of the match, and neither
  // commander fired a shot in twenty thousand ticks. A group standing still while its commander
  // goes on ordering it to attack is the deadlock S13 found among the builders, in the other half
  // of the AI.
  const Device* lead = _sim.Objects().FindDevice(_blackboard.idleFighters.front());
  if (lead == nullptr)
  {
    return false;
  }
  const DriveClass drive = DriveOf(_sim, _seat, *lead);
  CellPosition standable{};
  if (!StandableNear(_sim, drive, target, ATTACK_APPROACH_RINGS, standable))
  {
    return false; // Nothing within sixteen cells of the target this drive class could stand on.
  }
  target = standable;
  const auto x = static_cast<std::int32_t>(target.x * Neuron::SUBUNITS_PER_CELL + Neuron::SUBUNITS_PER_CELL / 2);
  const auto z = static_cast<std::int32_t>(target.y * Neuron::SUBUNITS_PER_CELL + Neuron::SUBUNITS_PER_CELL / 2);
  // One order a device: an order names ONE object (Sim/Order.h), so a group of four is four orders,
  // which is what a human's selection is too. A GROUP, and not everything standing still: the
  // planner's budget is 2,000 nodes a tick shared across every request (Sim/PathPlanner.h), so
  // twenty-nine cross-map searches at once each get seventy nodes and none of them finishes.
  std::size_t sent = 0;
  for (const ObjectId& fighter : _blackboard.idleFighters)
  {
    if (sent >= ATTACK_GROUP_SIZE)
    {
      break;
    }
    Submit(_sim, _seat, OrderKind::AttackMove, static_cast<std::int32_t>(fighter.value), x, z);
    ++sent;
  }
  return true;
}

} // namespace

void DecideForSeat(Sim& _sim, std::uint8_t _seat)
{
  AiBlackboard blackboard;
  Observe(_sim, _seat, blackboard);

  std::uint32_t homeX = 0;
  std::uint32_t homeY = 0;
  const bool home = HomeCell(_sim, _seat, homeX, homeY);
  if (!home)
  {
    return; // Nothing left and nowhere to start from; stage 12 is what ends a match like that.
  }

  // The army and the base do not compete for anything: a group of fighters standing idle is not a
  // resource the economy wants, so the attack behaviour is evaluated on every decision rather than
  // waiting its turn behind the build list. Without that it never runs at all - a factory with
  // something to produce answers first, every decision, for the whole match.
  (void)Attack(_sim, _seat, blackboard, homeX, homeY);

  if (Abandon(_sim, _seat, blackboard))
  {
    return;
  }
  if (Design(_sim, _seat, blackboard))
  {
    return;
  }
  if (Build(_sim, _seat, blackboard, StructureRole::CommandPost, 1, homeX, homeY, 8))
  {
    return;
  }
  if (Finish(_sim, _seat, blackboard))
  {
    return;
  }
  // Nothing new is placed while there are already more unfinished sites than builders to work them.
  // It is the rule that turns a list of plans into a base: without it one truck is spread over six
  // sites and finishes none. The slack of one is deliberate - a site no builder can reach, a
  // deposit on a summit no drive can climb, would otherwise stop the commander placing anything
  // else for the rest of the match, and one spare slot lets the base go up around it.
  const bool roomToBuild = blackboard.unfinished.size() <= std::max<std::size_t>(blackboard.builders, 1);
  // The generator comes BEFORE the next extractor, and that order is the whole of GameDesign.md §4:
  // an extractor earns nothing until a generator reaches it. A commander that claimed four deposits
  // before building one of these would have spent four extractors' worth of power on an income of
  // nothing, which is exactly what the first run of this AI did.
  const std::size_t generators = blackboard.standing[static_cast<std::size_t>(AiNeed::Generator)];
  const bool wantsGenerator =
    blackboard.standing[static_cast<std::size_t>(AiNeed::Extractor)] > 0 && (generators == 0 || blackboard.unservedExtractors > 0);
  if (roomToBuild && wantsGenerator &&
      Build(_sim, _seat, blackboard, StructureRole::Generator, static_cast<std::uint32_t>(generators) + 1, homeX, homeY,
            AI_BASE_SEARCH_RINGS))
  {
    return;
  }
  if (roomToBuild && Expand(_sim, _seat, blackboard))
  {
    return;
  }
  if (roomToBuild && Build(_sim, _seat, blackboard, StructureRole::Factory, 1, homeX, homeY, AI_BASE_SEARCH_RINGS))
  {
    return;
  }
  if (roomToBuild && Build(_sim, _seat, blackboard, StructureRole::ResearchLab, 1, homeX, homeY, AI_BASE_SEARCH_RINGS))
  {
    return;
  }
  (void)Produce(_sim, _seat, blackboard);
}

[[nodiscard]] DriveClass DriveOf(const Sim& _sim, std::uint8_t _seat, const Device& _device) noexcept
{
  const Seat& seat = _sim.Seats()[_seat];
  if (_device.design >= seat.designs.size())
  {
    return DriveClass::Wheels;
  }
  const std::uint32_t drive = seat.designs[_device.design].drive;
  const ContentTree& content = _sim.Content();
  return drive < content.components.drives.size() ? content.components.drives[drive].driveClass : DriveClass::Wheels;
}

void AdvanceAiSeats(Sim& _sim)
{
  const std::uint32_t tick = _sim.Tick();
  const std::span<const Seat> seats = _sim.Seats();
  for (std::size_t index = 0; index < seats.size(); ++index)
  {
    if (!seats[index].Scripted() || seats[index].victory != VictoryState::Playing)
    {
      continue;
    }
    // Staggered by seat, so that eight scripted commanders never decide on one tick and the budget
    // of §7 is a tenth of what it would otherwise be.
    if (tick % AI_DECISION_INTERVAL_TICKS != index % AI_DECISION_INTERVAL_TICKS)
    {
      continue;
    }
    DecideForSeat(_sim, static_cast<std::uint8_t>(index));
  }
}

} // namespace Outpost
