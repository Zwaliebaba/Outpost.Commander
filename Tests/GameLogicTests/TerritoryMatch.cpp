#include "pch.h"

#include "TerritoryMatch.h"

namespace GameLogicTests
{
TerritoryMatch::TerritoryMatch(bool _content, std::optional<std::uint32_t> _restartSeconds)
  : m_map(MapFor(_content)),
    m_server(Outpost::LoadTuning(ReadRepositoryTuning()), m_map, {.seed = 3})
{
  if (_restartSeconds.has_value())
    World().UseWorldRules(*_restartSeconds);
  World().PlaceStartingBases(m_server.MapData());
}

Outpost::Map TerritoryMatch::MapFor(bool _content)
{
  Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
  if (!_content)
  {
    map.outpostRules.clear();
    map.derelictRules.clear();
  }
  return map;
}

Outpost::Simulation& TerritoryMatch::World() noexcept
{
  return m_server.World();
}

const Outpost::SectorPlacement& TerritoryMatch::Placement(std::int32_t _sector) const
{
  return *std::ranges::find(m_map.sectors, _sector, &Outpost::SectorPlacement::id);
}

Outpost::PlanePosition TerritoryMatch::AsteroidIn(std::int32_t _sector) const
{
  const Outpost::SectorPlacement& sector = Placement(_sector);
  const std::vector<Outpost::OreAsteroidPlacement>& asteroids = m_server.MapData().oreAsteroids;
  const auto found = std::ranges::find_if(asteroids, [&sector](const Outpost::OreAsteroidPlacement& _asteroid)
                                          { return sector.Contains(_asteroid.position); });
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(found != asteroids.end(), L"no asteroid in the sector");
  return found->position;
}

const Outpost::Tuning& TerritoryMatch::TuningData() const noexcept
{
  return m_server.TuningData();
}

Outpost::PlanePosition TerritoryMatch::Start(Outpost::PlayerId _player) const
{
  return m_server.MapData().starts[_player.value - 1];
}

const std::vector<Outpost::OutpostPlacement>& TerritoryMatch::Outposts() const noexcept
{
  return m_server.MapData().outposts;
}

Outpost::EntityId TerritoryMatch::Structure(Outpost::PlayerId _owner, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
{
  const Outpost::StructureTuning& tuning = *std::ranges::find(m_server.TuningData().structures, _kind, &Outpost::StructureTuning::kind);
  return World().SpawnStructure(_owner, _kind, _position, static_cast<float>(tuning.footprintRadiusMeters),
                                tuning.hitPoints * Outpost::HUNDREDTHS, tuning.armor * Outpost::HUNDREDTHS);
}

Outpost::EntityId TerritoryMatch::Relay(Outpost::PlayerId _owner, std::int32_t _sector)
{
  return Structure(_owner, Outpost::StructureKind::Relay, Placement(_sector).node);
}

Outpost::EntityId TerritoryMatch::Warship(Outpost::PlayerId _owner, Outpost::PlanePosition _position)
{
  return World().SpawnShip(_owner, World().FindDesign(_owner, {Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{1}})->id,
                           _position);
}

std::vector<Outpost::EntityId> TerritoryMatch::Constructors(Outpost::PlayerId _player)
{
  std::vector<Outpost::EntityId> constructors;
  for (const Outpost::Entity& entity : World().Entities())
  {
    if (entity.owner == _player && entity.kind == Outpost::EntityKind::Ship && entity.role == Outpost::ShipRole::Constructor)
      constructors.push_back(entity.id);
  }
  return constructors;
}

Outpost::CommandResult TerritoryMatch::Build(Outpost::PlayerId _player, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
{
  const Outpost::BuildStructureCommand build{.constructors = {Constructors(_player).front()}, .structure = _kind, .position = _position};
  return World().Tick({{.player = _player, .order = build}}).front();
}

Outpost::SectorView TerritoryMatch::Sector(Outpost::PlayerId _player, std::int32_t _sector)
{
  const Outpost::Snapshot snapshot = World().BuildSnapshot(_player);
  const auto found = std::ranges::find(snapshot.sectors, _sector, &Outpost::SectorView::id);
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(found != snapshot.sectors.end());
  return *found;
}

std::int32_t TerritoryMatch::Income(Outpost::PlayerId _player)
{
  return World().BuildSnapshot(_player).oreIncomeHundredthsPerSecond;
}

bool TerritoryMatch::Sees(Outpost::PlayerId _player, Outpost::EntityId _entity)
{
  const Outpost::Snapshot snapshot = World().BuildSnapshot(_player);
  const auto found = std::ranges::find(snapshot.entities, _entity, &Outpost::EntityView::id);
  return found != snapshot.entities.end() && !found->remembered;
}

void TerritoryMatch::Run(std::uint32_t _ticks)
{
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
    (void)World().Tick({});
}

void TerritoryMatch::FightToTheEnd(Outpost::PlayerId _owner, std::vector<Outpost::EntityId> _ships)
{
  (void)World().Tick(
    {{.player = _owner, .order = Outpost::SetRetreatCommand{.ships = std::move(_ships), .retreat = Outpost::RetreatThreshold::Never}}});
}
} // namespace GameLogicTests
