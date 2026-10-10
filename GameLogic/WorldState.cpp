#include "pch.h"
#include "WorldState.h"

#include <array>

#include "ByteFormat.h"
#include "WireFields.h"

namespace Outpost
{
// The fields of the server's own types a save holds, in the order each declares them (ADR-077). As in WireFields.h, a
// structured binding names every field, so a field added to one of these types and not here does not compile. The
// simulation's private types list theirs where they are declared.

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, Entity>
auto Fields(Self& _value)
{
  auto& [id, kind, owner, design, hull, role, structure, position, headingRadians, radiusMeters, oreYield, oreReserveHundredths,
         reserveRemainder, site, speedMetersPerSecond, turnRateRadiansPerSecond, destination, path, laneEnd, cruiseSpeedMetersPerSecond,
         closestMeters, stalledTicks, hitPointsHundredths, maxHitPointsHundredths, armorHundredths, order, attackTarget, target,
         reloadMilliticks, chasedPosition, chaseTick, workTarget, standing, standingGroup, holdSector, standingFrom, standingTo,
         standingOutward, retreat, retreating, repairer, buildWorkDone, buildWorkNeeded, extraGunReloadMilliticks, level, upgradeWorkDone,
         upgradeWorkNeeded, structureWeapon, queue, researchQueue, jobWorkDone, jobWorkNeeded, secondJobWorkDone, secondJobWorkNeeded,
         shipyardNumber, shipsBuilt, salvageOre, salvageTopic, salvageWorkDone, salvageWorkNeeded] = _value;
  return std::tie(id, kind, owner, design, hull, role, structure, position, headingRadians, radiusMeters, oreYield, oreReserveHundredths,
                  reserveRemainder, site, speedMetersPerSecond, turnRateRadiansPerSecond, destination, path, laneEnd,
                  cruiseSpeedMetersPerSecond, closestMeters, stalledTicks, hitPointsHundredths, maxHitPointsHundredths, armorHundredths,
                  order, attackTarget, target, reloadMilliticks, chasedPosition, chaseTick, workTarget, standing, standingGroup, holdSector,
                  standingFrom, standingTo, standingOutward, retreat, retreating, repairer, buildWorkDone, buildWorkNeeded,
                  extraGunReloadMilliticks, level, upgradeWorkDone, upgradeWorkNeeded, structureWeapon, queue, researchQueue, jobWorkDone,
                  jobWorkNeeded, secondJobWorkDone, secondJobWorkNeeded, shipyardNumber, shipsBuilt, salvageOre, salvageTopic,
                  salvageWorkDone, salvageWorkNeeded);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ShipMovement>
auto Fields(Self& _value)
{
  auto& [speedMetersPerSecond, turnRateRadiansPerSecond, radiusMeters] = _value;
  return std::tie(speedMetersPerSecond, turnRateRadiansPerSecond, radiusMeters);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DesignComponents>
auto Fields(Self& _value)
{
  auto& [hull, drive, weapon, module] = _value;
  return std::tie(hull, drive, weapon, module);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DesignStats>
auto Fields(Self& _value)
{
  auto& [movement, hitPointsHundredths, armorHundredths, cost, buildSeconds, damageHundredths, fireIntervalSeconds, rangeMeters,
         splashRadiusMeters, moduleSightMeters] = _value;
  return std::tie(movement, hitPointsHundredths, armorHundredths, cost, buildSeconds, damageHundredths, fireIntervalSeconds, rangeMeters,
                  splashRadiusMeters, moduleSightMeters);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ShipDesign>
auto Fields(Self& _value)
{
  auto& [id, owner, name, components, stats, retreat] = _value;
  return std::tie(id, owner, name, components, stats, retreat);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, Obstacle>
auto Fields(Self& _value)
{
  auto& [center, radiusMeters] = _value;
  return std::tie(center, radiusMeters);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, PlaneVector>
auto Fields(Self& _value)
{
  auto& [xMeters, zMeters] = _value;
  return std::tie(xMeters, zMeters);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, GroupRoutes::Band>
auto Fields(Self& _value)
{
  auto& [middle, across] = _value;
  return std::tie(middle, across);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, GroupRoutes::Route>
auto Fields(Self& _value)
{
  auto& [corners, clearanceMeters, bands] = _value;
  return std::tie(corners, clearanceMeters, bands);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, SectorPlacement>
auto Fields(Self& _value)
{
  auto& [id, name, minXMeters, maxXMeters, minZMeters, maxZMeters, node, adjacent, kind] = _value;
  return std::tie(id, name, minXMeters, maxXMeters, minZMeters, maxZMeters, node, adjacent, kind);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DerelictPlacement>
auto Fields(Self& _value)
{
  auto& [position, ore, topic, hull, radiusMeters] = _value;
  return std::tie(position, ore, topic, hull, radiusMeters);
}

// The last enumerator of each of the server's own enumerations a save holds, as WireFields.h gives the protocol's.
constexpr OreYield LastOf(OreYield) noexcept
{
  return OreYield::Rich;
}

constexpr TargetRule LastOf(TargetRule) noexcept
{
  return TargetRule::Weakest;
}
} // namespace Outpost

namespace
{
// What a save starts with, so that no other file is taken for one: "OCWORLD" and a zero byte, read as a little-endian
// number.
constexpr std::uint64_t SAVE_KIND = 0x00444C524F57434Full;
// A save ends with its checksum, which covers everything before it.
constexpr std::size_t CHECKSUM_BYTES = sizeof(std::uint64_t);
constexpr std::string_view SAVE_SOURCE = "A saved world";

constexpr std::uint64_t FNV_OFFSET = 0xCBF29CE484222325ull;
constexpr std::uint64_t FNV_PRIME = 0x100000001B3ull;

std::uint64_t Fnv1a(std::uint64_t _hash, std::span<const std::byte> _bytes) noexcept
{
  for (const std::byte byte : _bytes)
    _hash = (_hash ^ static_cast<std::uint64_t>(byte)) * FNV_PRIME;
  return _hash;
}

// The bytes of a whole save before its checksum, once the checksum has been checked.
std::span<const std::byte> CheckedBody(std::span<const std::byte> _bytes)
{
  if (_bytes.size() < CHECKSUM_BYTES)
    throw Neuron::Exception(std::format("{} is malformed: it ends early.", SAVE_SOURCE));
  const std::span<const std::byte> body = _bytes.first(_bytes.size() - CHECKSUM_BYTES);
  std::uint64_t stored = 0;
  for (std::size_t i = 0; i < CHECKSUM_BYTES; ++i)
    stored |= static_cast<std::uint64_t>(_bytes[body.size() + i]) << (8 * i);
  if (stored != Fnv1a(FNV_OFFSET, body))
    throw Neuron::Exception(std::format("{} is malformed: its checksum does not match, so it is cut short or altered.", SAVE_SOURCE));
  return body;
}

Outpost::SaveHeader GetHeader(Outpost::ByteReader& _reader)
{
  std::uint64_t kind = 0;
  _reader.Get(kind);
  if (kind != SAVE_KIND)
    _reader.Malformed("it is not a saved world");
  std::uint32_t version = 0;
  _reader.Get(version);
  if (version != Outpost::WORLD_STATE_VERSION)
  {
    throw Neuron::Exception(
      std::format("{} holds state version {}, and this build reads version {} only.", SAVE_SOURCE, version, Outpost::WORLD_STATE_VERSION));
  }
  std::uint64_t layoutHash = 0;
  _reader.Get(layoutHash);
  if (layoutHash != Outpost::SaveLayoutHash())
    throw Neuron::Exception(std::format("{} was written by a build that lays its state out otherwise.", SAVE_SOURCE));
  Outpost::SaveHeader header;
  _reader.Get(header.identity.seed);
  _reader.Get(header.identity.ticksPerSecond);
  _reader.Get(header.identity.dataHash);
  _reader.Get(header.tick);
  return header;
}
} // namespace

template <typename Self, typename Parts> auto Outpost::Simulation::SavedFields(Self& _simulation, Parts& _parts)
{
  // Not saved: the tick rate and what follows from it, which the world's identity names; the pathfinder, whose obstacles
  // are saved in _parts and whose graphs follow from them; the observer, which is null between ticks; whether this tick
  // planned paths, which is false between ticks; the tuning data, which the save names by its hash, and what it gives a
  // player who has researched nothing; a battle matchup's end, since a matchup is never a world (ADR-083); and the last
  // tick's shots, destructions and events, which only that tick's snapshots show.
  [[maybe_unused]] auto& [ticksPerSecond, secondsPerTick, stallLimitTicks, tick, entities, lastEntityId, designs, lastDesignId, players,
                          targetRule, random, pathfinder, observer, plannedOrders, plannedThisTick, lastStandingGroup, scheduledOrders,
                          lastScheduledOrder, mapObstacles, mapHalfSizeMeters, sectors, outposts, tuning, unresearched, basePlayers,
                          matchOver, winner, matchEndedTick, ending, fog, worldRules, restartSeconds, starts, matchupEnd, shots, destroyed,
                          events] = _simulation;
  return std::tie(tick, entities, lastEntityId, designs, lastDesignId, players, targetRule, _parts.random, _parts.obstacles, plannedOrders,
                  lastStandingGroup, scheduledOrders, lastScheduledOrder, mapObstacles, mapHalfSizeMeters, sectors, outposts, basePlayers,
                  matchOver, winner, matchEndedTick, ending, fog, worldRules, restartSeconds, starts);
}

void Outpost::Simulation::SaveState(ByteWriter& _writer) const
{
  const SavedParts parts{.random = m_random.State(), .obstacles = m_pathfinder.Obstacles()};
  std::apply([&_writer](const auto&... _fields) { (_writer.Put(_fields), ...); }, SavedFields(*this, parts));
}

void Outpost::Simulation::LoadState(ByteReader& _reader)
{
  if (!m_tuning)
    throw Neuron::Exception("Simulation: a saved world loads only into a simulation given its tuning data");
  SavedParts parts;
  std::apply([&_reader](auto&... _fields) { (_reader.Get(_fields), ...); }, SavedFields(*this, parts));
  m_random.SetState(parts.random);
  // A new pathfinder over the saved obstacles: each graph is built whole when it is next needed, which is the very graph
  // the running world kept up to date (ADR-054 decision 7).
  m_pathfinder = Pathfinder();
  m_pathfinder.SetObstacles(std::move(parts.obstacles), m_mapHalfSizeMeters);
  for (PlayerState& player : m_players)
    player.researchEffects = EffectsFrom(*m_tuning, player.researched);
  m_plannedThisTick = false;
  m_shots.clear();
  m_destroyed.clear();
  m_events.clear();
}

std::string Outpost::Simulation::StateLayout()
{
  using Tied = decltype(SavedFields(std::declval<const Simulation&>(), std::declval<const SavedParts&>()));
  std::string layout;
  [&layout]<std::size_t... Indices>(std::index_sequence<Indices...>) {
    ((ByteLayout::Describe<std::tuple_element_t<Indices, Tied>>(layout), layout += ';'), ...);
  }(std::make_index_sequence<std::tuple_size_v<Tied>>{});
  return layout;
}

std::uint64_t Outpost::DataHash(std::span<const std::string_view> _texts) noexcept
{
  std::uint64_t hash = FNV_OFFSET;
  for (const std::string_view text : _texts)
  {
    std::array<std::byte, sizeof(std::uint64_t)> length{};
    for (std::size_t i = 0; i < length.size(); ++i)
      length[i] = static_cast<std::byte>(static_cast<std::uint64_t>(text.size()) >> (8 * i));
    hash = Fnv1a(hash, length);
    hash = Fnv1a(hash, std::as_bytes(std::span(text)));
  }
  return hash;
}

std::vector<std::byte> Outpost::EncodeWorld(const Simulation& _simulation, const WorldIdentity& _identity, const SeatReports& _reports)
{
  ByteWriter writer;
  writer.Put(SAVE_KIND);
  writer.Put(WORLD_STATE_VERSION);
  writer.Put(SaveLayoutHash());
  writer.Put(_identity.seed);
  writer.Put(_identity.ticksPerSecond);
  writer.Put(_identity.dataHash);
  writer.Put(_simulation.CurrentTick());
  _simulation.SaveState(writer);
  writer.Put(_reports);
  std::vector<std::byte> bytes = writer.Take();
  const std::uint64_t checksum = Fnv1a(FNV_OFFSET, bytes);
  for (std::size_t i = 0; i < CHECKSUM_BYTES; ++i)
    bytes.push_back(static_cast<std::byte>(checksum >> (8 * i)));
  return bytes;
}

Outpost::SaveHeader Outpost::ReadSaveHeader(std::span<const std::byte> _bytes)
{
  ByteReader reader(CheckedBody(_bytes), SAVE_SOURCE);
  return GetHeader(reader);
}

Outpost::SeatReports Outpost::DecodeWorld(std::span<const std::byte> _bytes, const WorldIdentity& _identity, Simulation& _simulation)
{
  ByteReader reader(CheckedBody(_bytes), SAVE_SOURCE);
  const SaveHeader header = GetHeader(reader);
  if (header.identity != _identity)
  {
    throw Neuron::Exception(std::format("{} is of another world: seed {}, {} ticks a second and data {:016x}, where this one has seed {}, "
                                        "{} ticks a second and data {:016x}.",
                                        SAVE_SOURCE, header.identity.seed, header.identity.ticksPerSecond, header.identity.dataHash,
                                        _identity.seed, _identity.ticksPerSecond, _identity.dataHash));
  }
  _simulation.LoadState(reader);
  SeatReports reports;
  reader.Get(reports);
  reader.Finish();
  if (_simulation.CurrentTick() != header.tick)
    reader.Malformed(std::format("its header names tick {} and its state tick {}", header.tick, _simulation.CurrentTick()));
  return reports;
}

std::string Outpost::SaveLayout()
{
  std::string layout = Simulation::StateLayout();
  ByteLayout::Describe<SeatReports>(layout);
  layout += ';';
  return layout;
}

std::uint64_t Outpost::SaveLayoutHash()
{
  static const std::uint64_t LAYOUT_HASH = []
  {
    const std::string layout = SaveLayout();
    const std::array<std::string_view, 1> texts{layout};
    return DataHash(texts);
  }();
  return LAYOUT_HASH;
}
