#include "pch.h"

#include "Targeting.h"
#include "Design.h"
#include "Movement.h"
#include "Placement.h"
#include "Plan.h"
#include "Production.h"
#include "Sim.h"
#include "Weapons.h"

#include <algorithm>
#include <vector>

namespace Outpost
{

namespace
{

/// The middle of a structure's footprint, in subunits: a structure holds cells and a shot needs a
/// point, so this is the one place the two meet.
void StructurePoint(const ContentTree& _content, const Structure& _structure, std::int32_t& _x, std::int32_t& _z)
{
  const Footprint footprint = FootprintOf(_structure, &_content);
  _x = static_cast<std::int32_t>(footprint.cellX * Neuron::SUBUNITS_PER_CELL + footprint.cellsX * Neuron::SUBUNITS_PER_CELL / 2);
  _z = static_cast<std::int32_t>(footprint.cellY * Neuron::SUBUNITS_PER_CELL + footprint.cellsY * Neuron::SUBUNITS_PER_CELL / 2);
}

[[nodiscard]] bool Allied(const Sim& _sim, std::uint8_t _left, std::uint8_t _right) noexcept
{
  return _left >= _sim.Seats().size() || _right >= _sim.Seats().size() || _sim.Seats()[_left].alliance == _sim.Seats()[_right].alliance;
}

/// What a shooter does with the target it has: keep it while it is alive, visible and in reach.
[[nodiscard]] bool TargetStillGood(const Sim& _sim, std::uint8_t _seat, std::int32_t _x, std::int32_t _z, std::int32_t _range,
                                   ObjectId _target, TargetPoint& _out)
{
  if (_target == NO_OBJECT || !TargetAt(_sim, _target, _out))
  {
    return false;
  }
  if (_seat >= _sim.Seats().size() || !VisibleToSeat(_sim.Seats()[_seat], _out.x, _out.z))
  {
    return false;
  }
  const auto range = static_cast<std::int64_t>(_range);
  return Neuron::LengthSquared(_out.x - _x, _out.z - _z) <= range * range;
}

/// One shot, direct or indirect, from a shooter that has already decided it may fire.
void Fire(Sim& _sim, ObjectId _shooter, std::uint8_t _seat, std::int32_t _x, std::int32_t _y, std::int32_t _z, const ModuleDesc& _module,
          std::uint8_t _rank, const ClassUpgrades& _upgrades, const TargetPoint& _target)
{
  const ContentTree& content = _sim.Content();
  const auto reach = static_cast<std::int32_t>(Neuron::Length(_target.x - _x, _target.z - _z));
  const bool longRange = reach > _module.shortRangeSubunits;
  const std::int32_t chance = HitPercent(_module, _upgrades, _rank, longRange);
  const std::int32_t damage = WeaponDamage(_module, _upgrades, _rank);
  const auto moduleRow = static_cast<std::uint32_t>(&_module - content.components.modules.data());

  // THE TICK'S RECORD OF THE TRIGGER (m1-vertical-slice/C9), written for BOTH fire kinds and before
  // either branch decides anything. A client learns that a shot happened from this and from
  // nothing else: direct fire deliberately leaves no Projectile, and the wire carries no
  // projectiles in any case. It is recorded once a shot is certainly fired - after the reload and
  // the range have been judged by the caller - and NOT per pellet of a salvo, because what a
  // commander sees is a weapon firing rather than each roll of its burst.
  _sim.Shots().push_back({_shooter, _seat, moduleRow, _target.id, _target.x, _target.y, _target.z, _sim.Tick()});

  if (_module.fireKind == FireKind::Indirect)
  {
    // Aimed at the ground where the target stands NOW, and nothing follows it: the shell lands
    // where it was sent and stage 9 asks what is there then (GameDesign.md §8).
    Projectile shot{};
    shot.seat = _seat;
    shot.shooter = _shooter;
    shot.module = moduleRow;
    shot.x = _x;
    shot.y = _y;
    shot.z = _z;
    shot.impactX = _target.x;
    shot.impactY = _target.y;
    shot.impactZ = _target.z;
    shot.ticksToImpact = static_cast<std::uint32_t>(std::max(1, reach / PROJECTILE_SPEED_SUBUNITS_PER_TICK));
    shot.hitPercent = chance;
    shot.damage = damage;
    (void)_sim.Objects().Create(shot);
    return;
  }

  // Direct fire decides the hit at the trigger; what flies is the client's business and carries
  // no record, which is why there is no Projectile here (Sim/Projectile.h says so).
  for (std::uint8_t shot = 0; shot < std::max<std::uint8_t>(_module.shotsPerSalvo, 1); ++shot)
  {
    if (static_cast<std::int32_t>(_sim.Roll(100)) >= chance)
    {
      continue;
    }
    const std::int32_t armor = ArmorAgainst(content.damage, _module.weaponClass, _target.armor);
    const std::int32_t points = DamageDealt(content.damage, _module.weaponClass, _target.armor.column, damage, armor);
    _sim.Damage().push_back({_target.id, _shooter, _seat, points});
  }
}

/// Where a weapon's fire opens, for a device holding this range stance (GameDesign.md §8).
///
/// Optimal range holds fire until the short band and long range opens up at the far edge: that is
/// what the axis buys. A weapon whose short range IS its minimum has no short band to close into -
/// the mortar's are both six cells - so for it the two stances are the same thing, and optimal
/// opens at the long range rather than never: "engage at optimal range" cannot mean "engage inside
/// the range you may not fire within".
///
/// NAMED BECAUSE TWO THINGS READ IT NOW (m1-vertical-slice/S14): whether a weapon may fire this
/// tick, and whether the device should walk closer because none of them may. Those two asking the
/// question differently is exactly how a device ends up closing on a target it could already hit.
[[nodiscard]] std::int32_t OpeningRange(const ModuleDesc& _row, RangeStance _stance) noexcept
{
  const bool hasShortBand = _row.shortRangeSubunits > _row.minimumRangeSubunits;
  return _stance == RangeStance::Optimal && hasShortBand ? _row.shortRangeSubunits : _row.longRangeSubunits;
}

/// One device's tick of shooting, after its reloads have counted down. It takes the DESIGN and not
/// the derived statistics, because what a weapon reaches is the module row's and what a commander
/// can see is the seat's fog: a shooter's own sight radius decides nothing here (Targeting.h says
/// why), so the one statistic that might have been wanted is the one that is not.
void ShootDevice(Sim& _sim, ObjectId _id, Device& _device, const DeviceDesign& _design)
{
  if (_device.fire == FireStance::HoldFire)
  {
    _device.target = NO_OBJECT;
    return;
  }
  const ContentTree& content = _sim.Content();
  std::array<WeaponMount, MAX_MOUNTS> weapons{};
  const std::uint8_t count = WeaponsOf(content, _design, weapons);
  if (count == 0)
  {
    return;
  }

  // The reach of the longest weapon it carries, which is what it looks for a target inside.
  std::int32_t reach = 0;
  for (std::uint8_t index = 0; index < count; ++index)
  {
    reach = std::max(reach, content.components.modules[weapons[index].module].longRangeSubunits);
  }

  const Seat& seat = _sim.Seats()[_device.seat];
  TargetPoint target{};
  if (!TargetStillGood(_sim, _device.seat, _device.x, _device.z, reach, _device.target, target))
  {
    // Return fire never goes looking: it answers what shot at it, which stage 10 is what puts on
    // the record. Fire at will acquires (GameDesign.md §8).
    _device.target = _device.fire == FireStance::FireAtWill ? Acquire(_sim, _device.seat, _device.x, _device.z, reach) : NO_OBJECT;
    if (!TargetStillGood(_sim, _device.seat, _device.x, _device.z, reach, _device.target, target))
    {
      return;
    }
  }

  const std::uint8_t rank = RankOf(_device.experience);
  const auto distance = static_cast<std::int32_t>(Neuron::Length(target.x - _device.x, target.z - _device.z));
  // WHETHER IT IS SIMPLY TOO FAR AWAY, which the closing rule below reads. Too far and not merely
  // unable to fire: a mortar with the target inside its minimum range cannot fire either, and
  // walking closer is the one thing that would make that worse.
  bool tooFarForEveryWeapon = true;
  for (std::uint8_t index = 0; index < count; ++index)
  {
    const ModuleDesc& row = content.components.modules[weapons[index].module];
    tooFarForEveryWeapon = tooFarForEveryWeapon && distance > OpeningRange(row, _device.range);
  }
  for (std::uint8_t index = 0; index < count; ++index)
  {
    const ModuleDesc& row = content.components.modules[weapons[index].module];
    if (_device.reloadTicks[weapons[index].mount] > 0)
    {
      continue;
    }
    if (distance > OpeningRange(row, _device.range) || distance < row.minimumRangeSubunits)
    {
      continue;
    }
    Fire(_sim, _id, _device.seat, _device.x, _device.y, _device.z, row, rank, seat.upgrades, target);
    _device.reloadTicks[weapons[index].mount] = ReloadTicks(row, seat.upgrades);
  }

  // An Attack order follows its target wherever it goes; an AttackMove closes on what it meets
  // only if its stance says to pursue, and holds its ground if it says to hold (GameDesign.md §8).
  // A Move, a Patrol or a Guard is the commander's and outranks what the weapon would rather do.
  //
  // AND A DEVICE WITH NO ORDER AT ALL CLOSES ON WHAT IT CAN SEE AND CANNOT REACH, by the owner's
  // ruling of 2026-09-20 on m1-vertical-slice/S14. "Optimal range" means fight at your best range,
  // and a commander who set it and then watched his tank stand two cells outside its own short
  // band while an enemy shot at it would call that a fault - which it was: measured, a machine gun
  // acquires at twelve cells and opens at eight, and two scripted armies stared at each other from
  // ten for the rest of the match. STOP IS NO ORDER, which is what makes this safe: Move, Patrol
  // and Guard still outrank the weapon, and HoldPosition still means hold, so a device a commander
  // put somewhere deliberately stays there. What moves is one that has finished what it was told
  // and can see something it is not allowed to shoot at yet.
  if (tooFarForEveryWeapon && _device.primaryOrder == PrimaryOrder::Stop && _device.movement == MovementStance::Pursue)
  {
    // IT GIVES ITSELF THE ORDER A COMMANDER WOULD HAVE GIVEN: advance and engage. Stage 3 does not
    // move a device on Stop at all - Sim/Movement.cpp's Walk returns on it - so setting a
    // destination alone changes nothing; Stop has to be left for the device to walk anywhere.
    //
    // AttackMove and not Attack, because Arrive turns an AttackMove back into Stop when it gets
    // there: a device that closed and then lost its target stands again rather than holding an
    // order nobody gave it for the rest of the match.
    //
    // From here it behaves exactly as a commander's AttackMove does, closing the rest of the way
    // while it shoots. Whether an AttackMove ought to stop at its own opening range is a question
    // about AttackMove and not about the range stance, and the answer is the same for both.
    _device.primaryOrder = PrimaryOrder::AttackMove;
  }
  const bool chasing = _device.primaryOrder == PrimaryOrder::Attack ||
                       (_device.primaryOrder == PrimaryOrder::AttackMove && _device.movement == MovementStance::Pursue);
  if (chasing && (_device.destinationX != target.x || _device.destinationZ != target.z))
  {
    _device.destinationX = target.x;
    _device.destinationZ = target.z;
    // The route it holds was to where the target used to be. It asks for another only when the
    // target has actually moved, so a target standing still costs the planner nothing.
    if (_device.pathIndex != NO_PATH_INDEX)
    {
      _sim.Planner().Cancel(_id);
      _device.pathIndex = NO_PATH_INDEX;
    }
  }
}

/// One standing structure's tick of shooting. A structure has no stances - SetStance names a device
/// (Sim/Order.h's table) - so it fires at will at the priority Targeting.h states.
void ShootStructure(Sim& _sim, ObjectId _id, Structure& _structure)
{
  const ContentTree& content = _sim.Content();
  WeaponMount weapon{};
  if (_structure.state != StructurePhase::Standing || !WeaponOf(content, _structure, weapon))
  {
    return;
  }
  const ModuleDesc& row = content.components.modules[weapon.module];
  std::int32_t x = 0;
  std::int32_t z = 0;
  StructurePoint(content, _structure, x, z);

  TargetPoint target{};
  if (!TargetStillGood(_sim, _structure.seat, x, z, row.longRangeSubunits, NO_OBJECT, target))
  {
    const ObjectId found = Acquire(_sim, _structure.seat, x, z, row.longRangeSubunits);
    if (!TargetStillGood(_sim, _structure.seat, x, z, row.longRangeSubunits, found, target))
    {
      return;
    }
  }
  if (_structure.reloadTicks > 0)
  {
    return;
  }
  const auto distance = static_cast<std::int32_t>(Neuron::Length(target.x - x, target.z - z));
  if (distance < row.minimumRangeSubunits)
  {
    return;
  }
  const Seat& seat = _sim.Seats()[_structure.seat];
  Fire(_sim, _id, _structure.seat, x, _structure.y, z, row, 0, seat.upgrades, target);
  _structure.reloadTicks = ReloadTicks(row, seat.upgrades);
}

} // namespace

bool TargetAt(const Sim& _sim, ObjectId _id, TargetPoint& _out)
{
  const ContentTree& content = _sim.Content();
  if (const Device* device = _sim.Objects().FindDevice(_id); device != nullptr)
  {
    if (device->seat >= _sim.Seats().size())
    {
      return false;
    }
    const Seat& seat = _sim.Seats()[device->seat];
    DesignStats stats{};
    if (DeriveSeatDesign(seat, content, device->design, stats) != DesignFault::None)
    {
      return false;
    }
    const std::uint32_t drive = seat.designs[device->design].drive;
    if (drive >= content.components.drives.size())
    {
      return false;
    }
    _out = {_id,
            device->x,
            device->z,
            device->y,
            {TargetColumnOf(content.components.drives[drive].driveClass), stats.kineticArmor, stats.thermalArmor},
            stats.costHundredths,
            true};
    return true;
  }
  if (const Structure* structure = _sim.Objects().FindStructure(_id); structure != nullptr)
  {
    if (!Occupies(structure->state) || structure->design >= content.structures.structures.size())
    {
      return false; // A plan occupies nothing and is not a thing to shoot at.
    }
    const StructureDesc& row = content.structures.structures[structure->design];
    std::int32_t x = 0;
    std::int32_t z = 0;
    StructurePoint(content, *structure, x, z);
    _out = {_id, x, z, structure->y, {TargetColumnOf(row.strength), row.kineticArmor, row.thermalArmor}, row.costHundredths, false};
    return true;
  }
  return false;
}

bool VisibleToSeat(const Seat& _seat, std::int32_t _x, std::int32_t _z) noexcept
{
  if (_seat.fog.Empty())
  {
    return false;
  }
  const std::int32_t cellX = _x >> Neuron::SUBUNITS_PER_CELL_SHIFT;
  const std::int32_t cellY = _z >> Neuron::SUBUNITS_PER_CELL_SHIFT;
  return cellX >= 0 && cellY >= 0 && _seat.fog.Visible(static_cast<std::uint32_t>(cellX), static_cast<std::uint32_t>(cellY));
}

ObjectId Acquire(const Sim& _sim, std::uint8_t _seat, std::int32_t _x, std::int32_t _z, std::int32_t _rangeSubunits)
{
  if (_seat >= _sim.Seats().size())
  {
    return NO_OBJECT;
  }
  const Seat& seat = _sim.Seats()[_seat];
  const auto range = static_cast<std::int64_t>(_rangeSubunits);

  ObjectId best = NO_OBJECT;
  std::int64_t bestDistance = 0;
  bool bestIsDevice = false;
  const auto consider = [&](ObjectId _id, std::uint8_t _theirSeat, std::int32_t _theirX, std::int32_t _theirZ, bool _device)
  {
    if (Allied(_sim, _seat, _theirSeat) || !VisibleToSeat(seat, _theirX, _theirZ))
    {
      return;
    }
    const std::int64_t distance = Neuron::LengthSquared(_theirX - _x, _theirZ - _z);
    if (distance > range * range)
    {
      return;
    }
    // Devices before structures, then the nearest, then the lower id - which the walk gives for
    // nothing, because it is ascending and a tie keeps the one already held (Sim/Targeting.h).
    if (best == NO_OBJECT || (_device && !bestIsDevice) || (_device == bestIsDevice && distance < bestDistance))
    {
      best = _id;
      bestDistance = distance;
      bestIsDevice = _device;
    }
  };

  _sim.Objects().ForEachDevice([&consider](ObjectId _id, const Device& _device)
                               { consider(_id, _device.seat, _device.x, _device.z, true); });
  _sim.Objects().ForEachStructure(
    [&consider, &_sim](ObjectId _id, const Structure& _structure)
    {
      if (!Occupies(_structure.state))
      {
        return;
      }
      std::int32_t x = 0;
      std::int32_t z = 0;
      StructurePoint(_sim.Content(), _structure, x, z);
      consider(_id, _structure.seat, x, z, false);
    });
  return best;
}

void AdvanceTargeting(Sim& _sim)
{
  // The tick's damage starts empty: stages 8 and 9 fill it and stage 10 spends it, so a queue left
  // over from the last tick would be a hit landing twice.
  _sim.Damage().clear();

  World& world = _sim.Objects();
  std::vector<ObjectId> shooters;
  world.ForEachDevice([&shooters](ObjectId _id, const Device&) { shooters.push_back(_id); });
  for (const ObjectId id : shooters)
  {
    Device* device = world.FindDevice(id);
    if (device == nullptr || device->seat >= _sim.Seats().size())
    {
      continue;
    }
    for (std::uint32_t& reload : device->reloadTicks)
    {
      if (reload > 0)
      {
        --reload;
      }
    }
    const Seat& seat = _sim.Seats()[device->seat];
    DesignStats stats{};
    if (DeriveSeatDesign(seat, _sim.Content(), device->design, stats) != DesignFault::None)
    {
      continue;
    }
    ShootDevice(_sim, id, *device, seat.designs[device->design]);
  }

  std::vector<ObjectId> emplacements;
  world.ForEachStructure([&emplacements](ObjectId _id, const Structure&) { emplacements.push_back(_id); });
  for (const ObjectId id : emplacements)
  {
    Structure* structure = world.FindStructure(id);
    if (structure == nullptr || structure->seat >= _sim.Seats().size())
    {
      continue;
    }
    if (structure->reloadTicks > 0)
    {
      --structure->reloadTicks;
    }
    ShootStructure(_sim, id, *structure);
  }
}

} // namespace Outpost
