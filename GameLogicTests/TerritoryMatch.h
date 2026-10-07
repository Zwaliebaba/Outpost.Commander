#pragma once

#include "RepositoryData.h"

namespace GameLogicTests
{
// A match on the repository's map and data, with both bases placed and fog of war, as the game plays it but for the
// pirates and derelicts, which a test asks for (Phase 2 design §4–§8): Blue's home in the southwest and Red's in the northeast. A
// warship is of the starting design Small+Ion+Mass Driver.
class TerritoryMatch
{
public:
  static constexpr Outpost::PlayerId BLUE{1};
  static constexpr Outpost::PlayerId RED{2};
  static constexpr std::uint32_t TICKS_PER_SECOND = 20;
  // With _content, the seed places the pirates' outposts and the derelicts as in a match (ADR-073, ADR-074); without, every
  // sector is free and empty, as the tests of the territory rules want it.
  explicit TerritoryMatch(bool _content = false)
    : m_map(MapFor(_content)),
      m_server(Outpost::LoadTuning(ReadRepositoryTuning()), m_map, {.seed = 3})
  {
    World().PlaceStartingBases(m_server.MapData());
  }

  [[nodiscard]] static Outpost::Map MapFor(bool _content)
  {
    Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    if (!_content)
    {
      map.outpostRules.clear();
      map.derelictRules.clear();
    }
    return map;
  }

  [[nodiscard]] Outpost::Simulation& World() noexcept
  {
    return m_server.World();
  }

  [[nodiscard]] const Outpost::SectorPlacement& Placement(std::int32_t _sector) const
  {
    return *std::ranges::find(m_map.sectors, _sector, &Outpost::SectorPlacement::id);
  }

  // An ore asteroid the match's seed placed in the sector (ADR-072).
  [[nodiscard]] Outpost::PlanePosition AsteroidIn(std::int32_t _sector) const
  {
    const Outpost::SectorPlacement& sector = Placement(_sector);
    const std::vector<Outpost::OreAsteroidPlacement>& asteroids = m_server.MapData().oreAsteroids;
    const auto found = std::ranges::find_if(asteroids, [&sector](const Outpost::OreAsteroidPlacement& _asteroid)
                                            { return sector.Contains(_asteroid.position); });
    Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(found != asteroids.end(), L"no asteroid in the sector");
    return found->position;
  }

  [[nodiscard]] const Outpost::Tuning& TuningData() const noexcept
  {
    return m_server.TuningData();
  }

  // The pirates' outposts the match's seed placed, if it has pirates (ADR-073).
  [[nodiscard]] const std::vector<Outpost::OutpostPlacement>& Outposts() const noexcept
  {
    return m_server.MapData().outposts;
  }

  // A finished structure of the tuning data's numbers.
  Outpost::EntityId Structure(Outpost::PlayerId _owner, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
  {
    const Outpost::StructureTuning& tuning = *std::ranges::find(m_server.TuningData().structures, _kind, &Outpost::StructureTuning::kind);
    return World().SpawnStructure(_owner, _kind, _position, static_cast<float>(tuning.footprintRadiusMeters),
                                  tuning.hitPoints * Outpost::HUNDREDTHS, tuning.armor * Outpost::HUNDREDTHS);
  }

  // A finished Relay on the sector's node.
  Outpost::EntityId Relay(Outpost::PlayerId _owner, std::int32_t _sector)
  {
    return Structure(_owner, Outpost::StructureKind::Relay, Placement(_sector).node);
  }

  Outpost::EntityId Warship(Outpost::PlayerId _owner, Outpost::PlanePosition _position)
  {
    return World().SpawnShip(_owner, World().FindDesign(_owner, {Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{1}})->id,
                             _position);
  }

  [[nodiscard]] std::vector<Outpost::EntityId> Constructors(Outpost::PlayerId _player)
  {
    std::vector<Outpost::EntityId> constructors;
    for (const Outpost::Entity& entity : World().Entities())
    {
      if (entity.owner == _player && entity.kind == Outpost::EntityKind::Ship && entity.role == Outpost::ShipRole::Constructor)
        constructors.push_back(entity.id);
    }
    return constructors;
  }

  Outpost::CommandResult Build(Outpost::PlayerId _player, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
  {
    const Outpost::BuildStructureCommand build{.constructors = {Constructors(_player).front()}, .structure = _kind, .position = _position};
    return World().Tick({{.player = _player, .order = build}}).front();
  }

  // The sector as _player's newest snapshot shows it.
  [[nodiscard]] Outpost::SectorView Sector(Outpost::PlayerId _player, std::int32_t _sector)
  {
    const Outpost::Snapshot snapshot = World().BuildSnapshot(_player);
    const auto found = std::ranges::find(snapshot.sectors, _sector, &Outpost::SectorView::id);
    Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(found != snapshot.sectors.end());
    return *found;
  }

  [[nodiscard]] std::int32_t Income(Outpost::PlayerId _player)
  {
    return World().BuildSnapshot(_player).oreIncomeHundredthsPerSecond;
  }

  [[nodiscard]] bool Sees(Outpost::PlayerId _player, Outpost::EntityId _entity)
  {
    const Outpost::Snapshot snapshot = World().BuildSnapshot(_player);
    const auto found = std::ranges::find(snapshot.entities, _entity, &Outpost::EntityView::id);
    return found != snapshot.entities.end() && !found->remembered;
  }

  void Run(std::uint32_t _ticks)
  {
    for (std::uint32_t tick = 0; tick < _ticks; ++tick)
      (void)World().Tick({});
  }

private:
  Outpost::Map m_map;
  Outpost::InProcessServer m_server;
};
} // namespace GameLogicTests
