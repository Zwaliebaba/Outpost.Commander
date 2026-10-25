#include "pch.h"

#include "MatchArena.h"

namespace GameLogicTests
{
MatchArena::MatchArena(std::optional<std::int32_t> _reserveOre)
  : m_tuning(Outpost::LoadTuning(ReadRepositoryTuning())),
    m_simulation(11, TICKS_PER_SECOND)
{
  m_simulation.PlaceMap({.sizeMeters = 4000.0f,
                         .minimumGapMeters = 60.0f,
                         .starts = {},
                         .oreAsteroids = {{HOME_ASTEROID, ASTEROID_RADIUS_METERS, Outpost::OreYield::Home, _reserveOre},
                                          {CONTESTED_ASTEROID, ASTEROID_RADIUS_METERS, Outpost::OreYield::Contested, _reserveOre}},
                         .asteroidFields = {}});
  m_simulation.UseTuning(m_tuning);
  for (const Outpost::PlayerId player : {BLUE, RED})
  {
    m_simulation.AddPlayer(player, m_tuning.rules.startingOre);
    m_simulation.SaveStartingDesigns(player, m_tuning);
  }
}

Outpost::Simulation& MatchArena::World() noexcept
{
  return m_simulation;
}

const Outpost::Tuning& MatchArena::TuningData() const noexcept
{
  return m_tuning;
}

const Outpost::StructureTuning& MatchArena::StructureData(Outpost::StructureKind _kind) const
{
  return *std::ranges::find(m_tuning.structures, _kind, &Outpost::StructureTuning::kind);
}

Outpost::EntityId MatchArena::Structure(Outpost::PlayerId _owner, Outpost::StructureKind _kind, Outpost::PlanePosition _position,
                                        std::int32_t _level)
{
  const Outpost::StructureTuning& tuning = StructureData(_kind);
  return m_simulation.SpawnStructure(_owner, _kind, _position, static_cast<float>(tuning.footprintRadiusMeters),
                                     m_simulation.StructureHitPoints(_owner, tuning, _level), tuning.armor * Outpost::HUNDREDTHS, _level);
}

Outpost::EntityId MatchArena::Ship(Outpost::PlayerId _owner, Outpost::HullId _hull, Outpost::WeaponId _weapon,
                                   Outpost::PlanePosition _position)
{
  const Outpost::ShipDesign* design = m_simulation.FindDesign(_owner, {_hull, Outpost::DriveId{1}, _weapon});
  return m_simulation.SpawnShip(_owner, design->id, _position);
}

Outpost::DesignId MatchArena::Design(Outpost::PlayerId _owner, Outpost::HullId _hull, Outpost::WeaponId _weapon) const
{
  return m_simulation.FindDesign(_owner, {_hull, Outpost::DriveId{1}, _weapon})->id;
}

std::vector<Outpost::CommandResult> MatchArena::Tick(const std::vector<Outpost::Command>& _commands)
{
  return m_simulation.Tick(_commands);
}

void MatchArena::Run(std::uint32_t _ticks)
{
  for (std::uint32_t i = 0; i < _ticks; ++i)
    (void)m_simulation.Tick({});
}

const Outpost::Entity& MatchArena::Get(Outpost::EntityId _id) const
{
  const Outpost::Entity* entity = m_simulation.FindEntity(_id);
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsNotNull(entity, L"the entity is gone");
  return *entity;
}

std::vector<const Outpost::Entity*> MatchArena::Owned(Outpost::PlayerId _owner, Outpost::EntityKind _kind) const
{
  std::vector<const Outpost::Entity*> owned;
  for (const Outpost::Entity& entity : m_simulation.Entities())
  {
    if (entity.owner == _owner && entity.kind == _kind)
      owned.push_back(&entity);
  }
  return owned;
}

Outpost::Command Order(Outpost::PlayerId _player, Outpost::Order _order)
{
  return {.player = _player, .order = std::move(_order)};
}
} // namespace GameLogicTests
