#include "pch.h"

#include "BuildSystem.h"

#include "ModuleEffects.h"

#include "RingAssignment.h"

#include <vector>

namespace Outpost
{

namespace
{
/// The player's station, or nullptr. A linear scan over the store, which at the design's 110 entities
/// is nothing -- and the alternative, a per-player cached identity, is a second thing to keep correct
/// across a death that M3 has not written yet.
[[nodiscard]] const Entity* StationOf(const World& _world, PlayerId _player) noexcept;

/// **THIS PLAYER'S MODULES, AS `CheckModuleSite` TAKES THEM** (M2.11) -- the same list the client builds from
/// its records, which is what lets the two sides evaluate one rule. In slot order; the rule does not care.
[[nodiscard]] std::vector<PlacedModule> ModulesOf(const World& _world, PlayerId _player)
{
  std::vector<PlacedModule> modules;
  const std::size_t slotCount = _world.SlotCount();
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if ((entity.owner == _player) && IsModule(entity.design))
    {
      modules.push_back(PlacedModule{
        .identity = PackIdentity(entity.id.index, entity.id.generation), .position = entity.position, .design = entity.design});
    }
  }
  return modules;
}

const Entity* StationOf(const World& _world, PlayerId _player) noexcept
{
  const std::size_t slotCount = _world.SlotCount();
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if ((entity.owner == _player) && (entity.design == DesignId::Station))
    {
      return &entity;
    }
  }
  return nullptr;
}
} // namespace

std::uint32_t BuildSystem::TicksToBuild(DesignId _design, std::uint32_t _multiplierPercent) noexcept
{
  return TicksForCost(Derive(_design).cost, _multiplierPercent);
}

std::uint32_t BuildSystem::TicksForCost(std::uint32_t _costCredits, std::uint32_t _multiplierPercent) noexcept
{
  const std::uint32_t multiplier = (_multiplierPercent == 0) ? 100 : _multiplierPercent;
  const std::uint64_t cost = _costCredits;

  // cost / (rate * multiplier / 100) seconds, times the tick rate. Written as one division so there is
  // one rounding rather than two, and both sides of the wire compute it the same way (R16).
  //
  // **AND IT ROUNDS UP** (M2.12, `OpenQuestions.md` Q56): a shipyard never makes anything faster than its
  // stated rate, so 400 credits at x1.5 is 267 ticks and not 266. Every ship divides exactly at both levels;
  // only a module needs the rule.
  const std::uint64_t numerator = cost * TICKS_PER_SECOND * 100;
  const std::uint64_t denominator = static_cast<std::uint64_t>(BUILD_RATE_CREDITS_PER_SECOND) * multiplier;
  const std::uint64_t ticks = (numerator + denominator - 1) / denominator;

  // AT LEAST ONE. A design with no cost would otherwise complete on the tick it started, and the
  // catalog has rows with no cost in it.
  return (ticks == 0) ? 1 : static_cast<std::uint32_t>(ticks);
}

void BuildSystem::Begin(std::size_t _playerCount) noexcept
{
  m_playerCount = (_playerCount > MAX_PLAYERS) ? MAX_PLAYERS : _playerCount;
  m_credits.fill(0);
  m_items.fill(BuildItem{});
  for (std::vector<BuildItem>& queued : m_queued)
  {
    queued.clear();
  }
  m_stranded = 0;

  for (std::size_t player = 1; player <= m_playerCount; ++player)
  {
    m_credits[player] = STARTING_CREDITS;
  }
}

bool BuildSystem::Holds(PlayerId _player) const noexcept
{
  return (_player != NO_PLAYER) && (static_cast<std::size_t>(_player) <= m_playerCount);
}

Neuron::Vec2 BuildSystem::SpawnPoint(const Entity& _station, DesignId _design) noexcept
{
  const std::int64_t offset = SpawnDistanceUnits(_design) * Neuron::FIXED_ONE;

  // The station's heading, through ADR-002's pinned table -- the only trigonometry the simulation is
  // allowed (R16). The station faces the center of the map, so a new ship appears on the side the
  // player is looking toward.
  const std::int64_t alongX = (static_cast<std::int64_t>(Neuron::Cosine(_station.heading)) * offset) / Neuron::SINE_ONE;
  const std::int64_t alongY = (static_cast<std::int64_t>(Neuron::Sine(_station.heading)) * offset) / Neuron::SINE_ONE;

  return ClampToPlayArea(Neuron::Vec2{.x = static_cast<Neuron::Fixed>(static_cast<std::int64_t>(_station.position.x) + alongX),
                                      .y = static_cast<Neuron::Fixed>(static_cast<std::int64_t>(_station.position.y) + alongY)});
}

Neuron::Vec2 BuildSystem::FreeSpawnPoint(const World& _world, const Entity& _station, DesignId _design) noexcept
{
  // THE FIRST THREE RINGS AROUND THE SPAWN POINT: 1 + 6 + 12 + 18 slots.
  constexpr std::size_t SPAWN_SLOTS = 37;

  const Neuron::Vec2 base = SpawnPoint(_station, _design);
  const std::int64_t outside = ClearOfModulesUnits(_design) * Neuron::FIXED_ONE;
  const std::int64_t shipHalf = static_cast<std::int64_t>(Hull(Design(_design).hull).sizeUnits) / 2;
  const auto spacing = static_cast<Neuron::Fixed>(static_cast<std::int64_t>(Hull(Design(_design).hull).sizeUnits) * Neuron::FIXED_ONE);

  for (std::size_t index = 0; index < SPAWN_SLOTS; ++index)
  {
    const Neuron::Vec2 offset = RingSlotOffset(index, spacing);
    const Neuron::Vec2 candidate = ClampToPlayArea(Neuron::Vec2{.x = base.x + offset.x, .y = base.y + offset.y});

    // OUTSIDE THE MODULE CIRCLE, like the spawn point itself: a ring slot on the station's side of it would put
    // the ship back among the modules.
    bool free = Neuron::LengthSquared(candidate - _station.position) >= (outside * outside);
    for (std::size_t slot = 0; (slot < _world.SlotCount()) && free; ++slot)
    {
      if (!_world.IsSlotAlive(slot))
      {
        continue;
      }
      const Entity& other = _world.EntityInSlot(slot);
      const std::int64_t keepOut =
        (shipHalf + (static_cast<std::int64_t>(Hull(Design(other.design).hull).sizeUnits) / 2)) * Neuron::FIXED_ONE;
      free = Neuron::LengthSquared(other.position - candidate) >= (keepOut * keepOut);
    }
    if (free)
    {
      return candidate;
    }
  }
  return base;
}

BuildRejection BuildSystem::Start(World& _world, PlayerId _player, DesignId _design, std::uint32_t _buildRateMultiplierPercent) noexcept
{
  if (!Holds(_player))
  {
    return BuildRejection::NoPlayer;
  }
  if (static_cast<std::size_t>(_design) >= Designs().size())
  {
    return BuildRejection::UnknownDesign;
  }
  if (!Design(_design).buildable)
  {
    return BuildRejection::NotBuildable;
  }
  if (StationOf(_world, _player) == nullptr)
  {
    return BuildRejection::NoStation;
  }

  const std::uint32_t cost = Derive(_design).cost;
  return Commit(_player, BuildItem{.active = true,
                                   .design = _design,
                                   .ticksElapsed = 0,
                                   .ticksRequired = TicksToBuild(_design, _buildRateMultiplierPercent),
                                   .multiplierPercent = (_buildRateMultiplierPercent == 0) ? 100u : _buildRateMultiplierPercent,
                                   .creditsSpent = cost});
}

BuildRejection BuildSystem::StartModule(World& _world, PlayerId _player, DesignId _design, const Neuron::Vec2& _site,
                                        std::uint32_t _buildRateMultiplierPercent) noexcept
{
  if (!Holds(_player))
  {
    return BuildRejection::NoPlayer;
  }
  if (static_cast<std::size_t>(_design) >= Designs().size())
  {
    return BuildRejection::UnknownDesign;
  }
  if (!IsModule(_design))
  {
    return BuildRejection::NotBuildable;
  }
  if (!IsPlacedLevel(_design))
  {
    return BuildRejection::NotUpgradeable;
  }
  const Entity* station = StationOf(_world, _player);
  if (station == nullptr)
  {
    return BuildRejection::NoStation;
  }

  // **THE SITE BEFORE THE QUEUE** (M2.11): refused here, nothing has been refunded, charged or cancelled. **A
  // placement already in the queue, or building, counts as if it were built** (Q80), so two queued modules can
  // never overlap and the cap of four counts what is on its way.
  std::vector<PlacedModule> placed = ModulesOf(_world, _player);
  const BuildItem& current = m_items[static_cast<std::size_t>(_player)];
  if (current.active && IsModule(current.design) && !current.upgrade.IsValid())
  {
    placed.push_back(PlacedModule{.position = current.site, .design = current.design});
  }
  for (const BuildItem& waiting : m_queued[static_cast<std::size_t>(_player)])
  {
    if (IsModule(waiting.design) && !waiting.upgrade.IsValid())
    {
      placed.push_back(PlacedModule{.position = waiting.site, .design = waiting.design});
    }
  }
  if (!CheckModuleSite(station->position, station->design, placed, _site, _design).Legal())
  {
    return BuildRejection::IllegalSite;
  }

  const std::uint32_t cost = Derive(_design).cost;
  return Commit(_player, BuildItem{.active = true,
                                   .design = _design,
                                   .ticksElapsed = 0,
                                   .ticksRequired = TicksToBuild(_design, _buildRateMultiplierPercent),
                                   .multiplierPercent = (_buildRateMultiplierPercent == 0) ? 100u : _buildRateMultiplierPercent,
                                   .creditsSpent = cost,
                                   .site = _site});
}

BuildRejection BuildSystem::StartUpgrade(World& _world, PlayerId _player, EntityId _module, DesignId _level,
                                         std::uint32_t _buildRateMultiplierPercent) noexcept
{
  if (!Holds(_player))
  {
    return BuildRejection::NoPlayer;
  }
  if (static_cast<std::size_t>(_level) >= Designs().size())
  {
    return BuildRejection::UnknownDesign;
  }
  if (StationOf(_world, _player) == nullptr)
  {
    return BuildRejection::NoStation;
  }

  // **YOURS, ALIVE, AND A LEVEL THAT BECOMES THIS ONE** -- all before the queue is touched, as a site is.
  const Entity* module = _world.Find(_module);
  if ((module == nullptr) || (module->owner != _player) || !UpgradesTo(module->design, _level))
  {
    return BuildRejection::NotUpgradeable;
  }

  // **ONE UPGRADE A MODULE AT A TIME** (Q80): a second, queued behind the first, would upgrade a level the
  // module no longer has.
  const BuildItem& current = m_items[static_cast<std::size_t>(_player)];
  bool pending = current.active && (current.upgrade == _module);
  for (const BuildItem& waiting : m_queued[static_cast<std::size_t>(_player)])
  {
    pending = pending || (waiting.upgrade == _module);
  }
  if (pending)
  {
    return BuildRejection::NotUpgradeable;
  }

  const std::uint32_t cost = UpgradeCostCredits(module->design, _level);
  return Commit(_player, BuildItem{.active = true,
                                   .design = _level,
                                   .ticksElapsed = 0,
                                   .ticksRequired = TicksForCost(cost, _buildRateMultiplierPercent),
                                   .multiplierPercent = (_buildRateMultiplierPercent == 0) ? 100u : _buildRateMultiplierPercent,
                                   .creditsSpent = cost,
                                   .site = module->position,
                                   .upgrade = _module});
}

BuildRejection BuildSystem::Commit(PlayerId _player, const BuildItem& _item) noexcept
{
  BuildItem& item = m_items[static_cast<std::size_t>(_player)];
  std::uint32_t& credits = m_credits[static_cast<std::size_t>(_player)];

  // **AN ORDER THE PLAYER CANNOT PAY FOR TOUCHES NOTHING** (the 2026-09-23 review, m1). `Interface.md` section 6
  // keeps an unaffordable button lit with its cost reddened -- "save up" -- and money is the queue's only limit
  // (Q80), so this is the one check between a tap and the queue.
  if (credits < _item.creditsSpent)
  {
    return BuildRejection::Unaffordable;
  }
  credits -= _item.creditsSpent;

  // **BEHIND WHAT IS BUILDING, NOT IN PLACE OF IT** (Q80; until 2026-09-24 this replaced it, Q35).
  if (item.active)
  {
    m_queued[static_cast<std::size_t>(_player)].push_back(_item);
  }
  else
  {
    item = _item;
  }
  return BuildRejection::None;
}

bool BuildSystem::Cancel(PlayerId _player) noexcept
{
  if (!Holds(_player))
  {
    return false;
  }

  // **THE NEWEST FIRST** (Q80): the last item queued, and only with nothing queued the one in progress. Full, and
  // from what was taken rather than from the catalog (Q35).
  std::vector<BuildItem>& queued = m_queued[static_cast<std::size_t>(_player)];
  if (!queued.empty())
  {
    m_credits[static_cast<std::size_t>(_player)] += queued.back().creditsSpent;
    queued.pop_back();
    return true;
  }

  BuildItem& item = m_items[static_cast<std::size_t>(_player)];
  if (!item.active)
  {
    return false;
  }
  m_credits[static_cast<std::size_t>(_player)] += item.creditsSpent;
  item = BuildItem{};
  return true;
}

void BuildSystem::Advance(World& _world) noexcept
{
  // In player order, which is the order everything else in this tree walks players in -- and the
  // order two completions on one tick reach the store in (R16).
  for (std::size_t player = 1; player <= m_playerCount; ++player)
  {
    BuildItem& item = m_items[player];

    // **THE NEXT IN LINE MOVES UP** (Q80) on the tick after the one before it finished, and builds on that same
    // tick -- so the station never stands idle between two queued items.
    std::vector<BuildItem>& queued = m_queued[player];
    if (!item.active && !queued.empty())
    {
      item = queued.front();
      queued.erase(queued.begin());
    }
    if (!item.active)
    {
      continue;
    }

    // **AN UPGRADE WHOSE MODULE HAS DIED IS REFUNDED ON THE TICK IT DIED** (the 2026-09-23 review, m7), not
    // when the item would have finished: the panel would otherwise show a destroyed module upgrading for the
    // rest of the build, and the credits would sit locked in it.
    if (item.upgrade.IsValid())
    {
      const Entity* module = _world.Find(item.upgrade);
      if ((module == nullptr) || (module->owner != static_cast<PlayerId>(player)))
      {
        m_credits[player] += item.creditsSpent;
        item = BuildItem{};
        ++m_stranded;
        continue;
      }
    }

    // **THE RATE IN FORCE NOW, NOT THE ONE IT STARTED AT** (M3.8b, Q77's first item): a shipyard lost mid-build slows
    // the item on the tick it dies, and one finished speeds it up. What is left is rescaled in integers, rounding up
    // as `TicksForCost` does, so a shipyard never builds faster than its stated rate (R16).
    const std::uint32_t rate = BuildRateMultiplierPercent(_world, static_cast<PlayerId>(player));
    if ((rate != item.multiplierPercent) && (rate != 0) && (item.multiplierPercent != 0))
    {
      const std::uint64_t remainingWork =
        static_cast<std::uint64_t>(item.ticksRequired - std::min(item.ticksElapsed, item.ticksRequired)) * item.multiplierPercent;
      item.ticksRequired = item.ticksElapsed + static_cast<std::uint32_t>((remainingWork + rate - 1) / rate);
      item.multiplierPercent = rate;
    }

    ++item.ticksElapsed;
    if (item.ticksElapsed < item.ticksRequired)
    {
      continue;
    }

    const Entity* station = StationOf(_world, static_cast<PlayerId>(player));
    if (station == nullptr)
    {
      // NOWHERE TO PUT IT. A station destroyed eliminates its owner (`GameDesign.md` section 2) and
      // M3 is what writes that; until then the honest answer is to give the credits back rather than
      // to drop a ship at the origin.
      m_credits[player] += item.creditsSpent;
      item = BuildItem{};
      ++m_stranded;
      continue;
    }

    // **AN UPGRADE CHANGES THE MODULE IN PLACE** (Q54): the same entity, the same identity and position, a
    // new level. The hull it has taken stays taken -- both levels carry the frame's hull points. The module
    // is alive and this player's: the check at the top of the loop ran this tick.
    if (item.upgrade.IsValid())
    {
      _world.Find(item.upgrade)->design = item.design;
      item = BuildItem{};
      continue;
    }

    // Copied before `Create`, which may reallocate the store out from under the pointer above. **A module
    // appears where it was placed** (M2.11) and faces the way the station does; a ship at the spawn point.
    const bool isModule = IsModule(item.design);
    const Neuron::Vec2 spawn = isModule ? item.site : FreeSpawnPoint(_world, *station, item.design);
    const Neuron::Angle heading = station->heading;

    static_cast<void>(_world.Create(spawn, heading, item.design, static_cast<PlayerId>(player)));
    item = BuildItem{};
  }
}

std::uint32_t BuildSystem::Credits(PlayerId _player) const noexcept
{
  return Holds(_player) ? m_credits[static_cast<std::size_t>(_player)] : 0;
}

void BuildSystem::Grant(PlayerId _player, std::uint32_t _credits) noexcept
{
  if (Holds(_player))
  {
    m_credits[static_cast<std::size_t>(_player)] += _credits;
  }
}

const BuildItem& BuildSystem::Item(PlayerId _player) const noexcept
{
  static constexpr BuildItem NOTHING{};
  return Holds(_player) ? m_items[static_cast<std::size_t>(_player)] : NOTHING;
}

std::span<const BuildItem> BuildSystem::Queued(PlayerId _player) const noexcept
{
  if (!Holds(_player))
  {
    return {};
  }
  return m_queued[static_cast<std::size_t>(_player)];
}

std::uint8_t BuildSystem::WireBuildingDesign(PlayerId _player) const noexcept
{
  const BuildItem& item = Item(_player);
  const std::uint8_t designPlusOne = item.active ? static_cast<std::uint8_t>(static_cast<std::uint8_t>(item.design) + 1) : 0;
  return PackBuilding(designPlusOne, static_cast<std::uint32_t>(Queued(_player).size()));
}

std::uint8_t BuildSystem::WireProgressPercent(PlayerId _player) const noexcept
{
  const BuildItem& item = Item(_player);
  if (!item.active || (item.ticksRequired == 0))
  {
    return 0;
  }

  const std::uint64_t percent = (static_cast<std::uint64_t>(item.ticksElapsed) * 100) / item.ticksRequired;
  return (percent > 99) ? 99 : static_cast<std::uint8_t>(percent);
}

} // namespace Outpost
