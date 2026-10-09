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
  // sector is free and empty, as the tests of the territory rules want it. With _restartSeconds, it plays by a world's rules,
  // a lost seat restarting that long after (Phase 5 design §8).
  explicit TerritoryMatch(bool _content = false, std::optional<std::uint32_t> _restartSeconds = std::nullopt);

  [[nodiscard]] static Outpost::Map MapFor(bool _content);

  [[nodiscard]] Outpost::Simulation& World() noexcept;

  [[nodiscard]] const Outpost::SectorPlacement& Placement(std::int32_t _sector) const;

  // An ore asteroid the match's seed placed in the sector (ADR-072).
  [[nodiscard]] Outpost::PlanePosition AsteroidIn(std::int32_t _sector) const;

  [[nodiscard]] const Outpost::Tuning& TuningData() const noexcept;

  // A player's start, where its Command Station stood at first.
  [[nodiscard]] Outpost::PlanePosition Start(Outpost::PlayerId _player) const;

  // The pirates' outposts the match's seed placed, if it has pirates (ADR-073).
  [[nodiscard]] const std::vector<Outpost::OutpostPlacement>& Outposts() const noexcept;

  // A finished structure of the tuning data's numbers.
  Outpost::EntityId Structure(Outpost::PlayerId _owner, Outpost::StructureKind _kind, Outpost::PlanePosition _position);

  // A finished Relay on the sector's node.
  Outpost::EntityId Relay(Outpost::PlayerId _owner, std::int32_t _sector);

  Outpost::EntityId Warship(Outpost::PlayerId _owner, Outpost::PlanePosition _position);

  [[nodiscard]] std::vector<Outpost::EntityId> Constructors(Outpost::PlayerId _player);

  Outpost::CommandResult Build(Outpost::PlayerId _player, Outpost::StructureKind _kind, Outpost::PlanePosition _position);

  // The sector as _player's newest snapshot shows it.
  [[nodiscard]] Outpost::SectorView Sector(Outpost::PlayerId _player, std::int32_t _sector);

  [[nodiscard]] std::int32_t Income(Outpost::PlayerId _player);

  [[nodiscard]] bool Sees(Outpost::PlayerId _player, Outpost::EntityId _entity);

  void Run(std::uint32_t _ticks);

  // Sets _ships never to retreat (ADR-075), for a test of a fight to the end. It takes a tick.
  void FightToTheEnd(Outpost::PlayerId _owner, std::vector<Outpost::EntityId> _ships);

private:
  Outpost::Map m_map;
  Outpost::InProcessServer m_server;
};
} // namespace GameLogicTests
