#pragma once

#include "RepositoryData.h"

namespace GameLogicTests
{
// Milestone 4's tests: two players on open ground with the repository's tuning data, a home ore asteroid north of the
// origin and a contested one east of it, the starting Ore and the starting designs, and nothing else until a test
// places it. The asteroids never run out unless a test gives them a reserve (Phase 1 design §8).
class MatchArena
{
public:
  static constexpr Outpost::PlayerId BLUE{1};
  static constexpr Outpost::PlayerId RED{2};
  static constexpr std::uint32_t TICKS_PER_SECOND = 20;
  static constexpr Outpost::PlanePosition HOME_ASTEROID{.xMeters = 0.0f, .zMeters = 600.0f};
  static constexpr Outpost::PlanePosition CONTESTED_ASTEROID{.xMeters = 900.0f, .zMeters = 0.0f};
  static constexpr float ASTEROID_RADIUS_METERS = 45.0f;

  explicit MatchArena(std::optional<std::int32_t> _reserveOre = std::nullopt);

  [[nodiscard]] Outpost::Simulation& World() noexcept;

  [[nodiscard]] const Outpost::Tuning& TuningData() const noexcept;

  [[nodiscard]] const Outpost::StructureTuning& StructureData(Outpost::StructureKind _kind) const;

  // A built structure of the tuning data's footprint, hit points, armor and gun, at _level with its hit points (Phase 3
  // design §4).
  Outpost::EntityId Structure(Outpost::PlayerId _owner, Outpost::StructureKind _kind, Outpost::PlanePosition _position,
                              std::int32_t _level = 1);

  // A warship of _owner's starting design of these components.
  Outpost::EntityId Ship(Outpost::PlayerId _owner, Outpost::HullId _hull, Outpost::WeaponId _weapon, Outpost::PlanePosition _position);

  [[nodiscard]] Outpost::DesignId Design(Outpost::PlayerId _owner, Outpost::HullId _hull, Outpost::WeaponId _weapon) const;

  // Runs one tick with these commands and returns what became of each.
  std::vector<Outpost::CommandResult> Tick(const std::vector<Outpost::Command>& _commands = {});

  // Runs _ticks ticks without commands.
  void Run(std::uint32_t _ticks);

  [[nodiscard]] const Outpost::Entity& Get(Outpost::EntityId _id) const;

  // The entities of _owner of this kind, newest last.
  [[nodiscard]] std::vector<const Outpost::Entity*> Owned(Outpost::PlayerId _owner, Outpost::EntityKind _kind) const;

private:
  Outpost::Tuning m_tuning;
  Outpost::Simulation m_simulation;
};

Outpost::Command Order(Outpost::PlayerId _player, Outpost::Order _order);
} // namespace GameLogicTests
