#pragma once

#include "RepositoryData.h"

namespace GameLogicTests
{
// Milestone 4's tests: two players on open ground with the repository's tuning data, a home ore asteroid north of the
// origin and a contested one east of it, the starting Ore and the starting designs, and nothing else until a test
// places it.
class MatchArena
{
public:
  static constexpr Outpost::PlayerId BLUE{1};
  static constexpr Outpost::PlayerId RED{2};
  static constexpr std::uint32_t TICKS_PER_SECOND = 20;
  static constexpr Outpost::PlanePosition HOME_ASTEROID{.xMeters = 0.0f, .zMeters = 600.0f};
  static constexpr Outpost::PlanePosition CONTESTED_ASTEROID{.xMeters = 900.0f, .zMeters = 0.0f};
  static constexpr float ASTEROID_RADIUS_METERS = 45.0f;

  MatchArena()
    : m_tuning(Outpost::LoadTuning(ReadRepositoryTuning())),
      m_simulation(11, TICKS_PER_SECOND)
  {
    m_simulation.PlaceMap({.sizeMeters = 4000.0f, .minimumGapMeters = 60.0f, .starts = {},
                           .oreAsteroids = {{HOME_ASTEROID, ASTEROID_RADIUS_METERS, Outpost::OreYield::Home},
                                            {CONTESTED_ASTEROID, ASTEROID_RADIUS_METERS, Outpost::OreYield::Contested}},
                           .asteroidFields = {}});
    m_simulation.UseTuning(m_tuning);
    for (const Outpost::PlayerId player : {BLUE, RED})
    {
      m_simulation.AddPlayer(player, m_tuning.rules.startingOre);
      m_simulation.SaveStartingDesigns(player, m_tuning);
    }
  }

  [[nodiscard]] Outpost::Simulation& World() noexcept
  {
    return m_simulation;
  }

  [[nodiscard]] const Outpost::Tuning& TuningData() const noexcept
  {
    return m_tuning;
  }

  [[nodiscard]] const Outpost::StructureTuning& StructureData(Outpost::StructureKind _kind) const
  {
    return *std::ranges::find(m_tuning.structures, _kind, &Outpost::StructureTuning::kind);
  }

  // A built structure of the tuning data's footprint, hit points, armor and gun.
  Outpost::EntityId Structure(Outpost::PlayerId _owner, Outpost::StructureKind _kind, Outpost::PlanePosition _position)
  {
    const Outpost::StructureTuning& tuning = StructureData(_kind);
    return m_simulation.SpawnStructure(_owner, _kind, _position, static_cast<float>(tuning.footprintRadiusMeters),
                                       tuning.hitPoints * Outpost::HUNDREDTHS, tuning.armor * Outpost::HUNDREDTHS);
  }

  // A warship of _owner's starting design of these components.
  Outpost::EntityId Ship(Outpost::PlayerId _owner, Outpost::HullId _hull, Outpost::WeaponId _weapon, Outpost::PlanePosition _position)
  {
    const Outpost::ShipDesign* design = m_simulation.FindDesign(_owner, {_hull, Outpost::DriveId{1}, _weapon});
    return m_simulation.SpawnShip(_owner, design->id, _position);
  }

  [[nodiscard]] Outpost::DesignId Design(Outpost::PlayerId _owner, Outpost::HullId _hull, Outpost::WeaponId _weapon) const
  {
    return m_simulation.FindDesign(_owner, {_hull, Outpost::DriveId{1}, _weapon})->id;
  }

  // Runs one tick with these commands and returns what became of each.
  std::vector<Outpost::CommandResult> Tick(const std::vector<Outpost::Command>& _commands = {})
  {
    return m_simulation.Tick(_commands);
  }

  // Runs _ticks ticks without commands.
  void Run(std::uint32_t _ticks)
  {
    for (std::uint32_t i = 0; i < _ticks; ++i)
      (void)m_simulation.Tick({});
  }

  [[nodiscard]] const Outpost::Entity& Get(Outpost::EntityId _id) const
  {
    const Outpost::Entity* entity = m_simulation.FindEntity(_id);
    Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsNotNull(entity, L"the entity is gone");
    return *entity;
  }

  // The entities of _owner of this kind, newest last.
  [[nodiscard]] std::vector<const Outpost::Entity*> Owned(Outpost::PlayerId _owner, Outpost::EntityKind _kind) const
  {
    std::vector<const Outpost::Entity*> owned;
    for (const Outpost::Entity& entity : m_simulation.Entities())
    {
      if (entity.owner == _owner && entity.kind == _kind)
        owned.push_back(&entity);
    }
    return owned;
  }

private:
  Outpost::Tuning m_tuning;
  Outpost::Simulation m_simulation;
};

inline Outpost::Command Order(Outpost::PlayerId _player, Outpost::Order _order)
{
  return {.player = _player, .order = std::move(_order)};
}
} // namespace GameLogicTests