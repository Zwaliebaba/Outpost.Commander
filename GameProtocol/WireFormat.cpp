#include "pch.h"
#include "WireFormat.h"

#include <bit>
#include <concepts>
#include <limits>
#include <tuple>
#include <utility>

namespace Outpost
{
namespace
{
// Each type's fields, in the order it declares them, which is the order they cross the wire in. A structured binding has
// to name every field, so a field added to one of these types and not here does not compile (ADR-060).
template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, PlanePosition>
auto Fields(Self& _value)
{
  auto& [xMeters, zMeters] = _value;
  return std::tie(xMeters, zMeters);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, MoveCommand>
auto Fields(Self& _value)
{
  auto& [ships, destination] = _value;
  return std::tie(ships, destination);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, AttackCommand>
auto Fields(Self& _value)
{
  auto& [ships, target] = _value;
  return std::tie(ships, target);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, AttackMoveCommand>
auto Fields(Self& _value)
{
  auto& [ships, destination] = _value;
  return std::tie(ships, destination);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, StopCommand>
auto Fields(Self& _value)
{
  auto& [ships] = _value;
  return std::tie(ships);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, BuildStructureCommand>
auto Fields(Self& _value)
{
  auto& [constructors, structure, position] = _value;
  return std::tie(constructors, structure, position);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, RepairCommand>
auto Fields(Self& _value)
{
  auto& [constructors, target] = _value;
  return std::tie(constructors, target);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, QueueShipCommand>
auto Fields(Self& _value)
{
  auto& [producer, design] = _value;
  return std::tie(producer, design);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, StartResearchCommand>
auto Fields(Self& _value)
{
  auto& [lab, topic] = _value;
  return std::tie(lab, topic);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, SaveDesignCommand>
auto Fields(Self& _value)
{
  auto& [design, nameUtf8, hull, drive, weapon, module] = _value;
  return std::tie(design, nameUtf8, hull, drive, weapon, module);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, HoldSectorCommand>
auto Fields(Self& _value)
{
  auto& [ships, position] = _value;
  return std::tie(ships, position);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, PatrolCommand>
auto Fields(Self& _value)
{
  auto& [ships, destination] = _value;
  return std::tie(ships, destination);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, UpgradeStructureCommand>
auto Fields(Self& _value)
{
  auto& [structure] = _value;
  return std::tie(structure);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, Command>
auto Fields(Self& _value)
{
  auto& [player, order] = _value;
  return std::tie(player, order);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, JobView>
auto Fields(Self& _value)
{
  auto& [role, design] = _value;
  return std::tie(role, design);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, EntityView>
auto Fields(Self& _value)
{
  auto& [id, kind, owner, design, hull, drive, weapon, module, role, structure, position, headingRadians, radiusMeters, hitPointsHundredths,
         maxHitPointsHundredths, builtPermille, level, upgradePermille, shipyardNumber, shipsBuilt, queue, research, jobPermille,
         secondJobPermille, remembered, sightMeters, standing, oreReserveHundredths] = _value;
  return std::tie(id, kind, owner, design, hull, drive, weapon, module, role, structure, position, headingRadians, radiusMeters,
                  hitPointsHundredths, maxHitPointsHundredths, builtPermille, level, upgradePermille, shipyardNumber, shipsBuilt, queue,
                  research, jobPermille, secondJobPermille, remembered, sightMeters, standing, oreReserveHundredths);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, HullView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, hitPointsHundredths, armorHundredths, speedMetersPerSecond, turnRateDegreesPerSecond, footprintRadiusMeters, cost,
         buildSeconds, available, shipyardLevel] = _value;
  return std::tie(id, nameUtf8, hitPointsHundredths, armorHundredths, speedMetersPerSecond, turnRateDegreesPerSecond, footprintRadiusMeters,
                  cost, buildSeconds, available, shipyardLevel);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DriveView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, speedFactor, hitPointsFactor, turnRateFactor, cost, available] = _value;
  return std::tie(id, nameUtf8, speedFactor, hitPointsFactor, turnRateFactor, cost, available);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, WeaponView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, damageHundredths, fireIntervalSeconds, rangeMeters, splashRadiusMeters, cost, available] = _value;
  return std::tie(id, nameUtf8, damageHundredths, fireIntervalSeconds, rangeMeters, splashRadiusMeters, cost, available);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ModuleView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, sightMeters, speedFactor, cost, available] = _value;
  return std::tie(id, nameUtf8, sightMeters, speedFactor, cost, available);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ResearchTopicView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, effectUtf8, cost, researchSeconds, prerequisites, researched, unlocksHull, unlocksDrive, unlocksWeapon, tier] =
    _value;
  return std::tie(id, nameUtf8, effectUtf8, cost, researchSeconds, prerequisites, researched, unlocksHull, unlocksDrive, unlocksWeapon,
                  tier);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DesignView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, hull, drive, weapon, module, cost] = _value;
  return std::tie(id, nameUtf8, hull, drive, weapon, module, cost);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, StructureLevelView>
auto Fields(Self& _value)
{
  auto& [cost, buildSeconds, maxHitPointsHundredths, opensTier, researchSlots, prerequisites, nodes, guns] = _value;
  return std::tie(cost, buildSeconds, maxHitPointsHundredths, opensTier, researchSlots, prerequisites, nodes, guns);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, StructureTypeView>
auto Fields(Self& _value)
{
  auto& [structure, nameUtf8, radiusMeters, buildable, cost, levels] = _value;
  return std::tie(structure, nameUtf8, radiusMeters, buildable, cost, levels);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, SectorView>
auto Fields(Self& _value)
{
  auto& [id, nameUtf8, minXMeters, maxXMeters, minZMeters, maxZMeters, node, adjacent, holder, suppressed, cutOff] = _value;
  return std::tie(id, nameUtf8, minXMeters, maxXMeters, minZMeters, maxZMeters, node, adjacent, holder, suppressed, cutOff);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, TicketsView>
auto Fields(Self& _value)
{
  auto& [player, tickets] = _value;
  return std::tie(player, tickets);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ShotView>
auto Fields(Self& _value)
{
  auto& [shooter, target, weapon, from, to, splashRadiusMeters, gun] = _value;
  return std::tie(shooter, target, weapon, from, to, splashRadiusMeters, gun);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, DestroyedView>
auto Fields(Self& _value)
{
  auto& [id, kind, structure, owner, hull, position, headingRadians, radiusMeters] = _value;
  return std::tie(id, kind, structure, owner, hull, position, headingRadians, radiusMeters);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, Snapshot>
auto Fields(Self& _value)
{
  auto& [tick, player, entities, shots, destroyed, ore, oreIncomeHundredthsPerSecond, designs, mapSizeMeters, structureTypes,
         constructorCost, hulls, drives, weapons, modules, research, shipyardBuildSpeedFactor, researchTier, nodeCap, matchOver, winner,
         matchEndedTick, ending, fogOfWar, sectors, tickets, startingTickets] = _value;
  return std::tie(tick, player, entities, shots, destroyed, ore, oreIncomeHundredthsPerSecond, designs, mapSizeMeters, structureTypes,
                  constructorCost, hulls, drives, weapons, modules, research, shipyardBuildSpeedFactor, researchTier, nodeCap, matchOver,
                  winner, matchEndedTick, ending, fogOfWar, sectors, tickets, startingTickets);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, HelloMessage>
auto Fields(Self& _value)
{
  auto& [protocolVersion, player] = _value;
  return std::tie(protocolVersion, player);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, WelcomeMessage>
auto Fields(Self& _value)
{
  auto& [player] = _value;
  return std::tie(player);
}

// A type whose fields are listed above.
template <typename T>
concept WireRecord = requires(T& _value) { Fields(_value); };

// Each enumeration's last enumerator: a value past it is not one the enumeration has. An enumerator added after it has to
// be named here too, or a message that carries it is refused.
constexpr EntityKind LastOf(EntityKind) noexcept
{
  return EntityKind::AsteroidField;
}

constexpr ShipRole LastOf(ShipRole) noexcept
{
  return ShipRole::Constructor;
}

constexpr StructureKind LastOf(StructureKind) noexcept
{
  return StructureKind::Relay;
}

constexpr StandingOrder LastOf(StandingOrder) noexcept
{
  return StandingOrder::Patrol;
}

constexpr MatchEnding LastOf(MatchEnding) noexcept
{
  return MatchEnding::Domination;
}

// Where T is among Message's alternatives, which is the first byte of every message.
template <typename T, typename... Ts> consteval std::uint8_t KindOf(std::type_identity<std::variant<Ts...>>)
{
  constexpr std::array<bool, sizeof...(Ts)> MATCHES{std::same_as<T, Ts>...};
  return static_cast<std::uint8_t>(std::ranges::find(MATCHES, true) - MATCHES.begin());
}

[[noreturn]] void Malformed(std::string_view _what)
{
  throw Neuron::Exception(std::format("A message from the network is malformed: {}.", _what));
}

// Writes values as they cross the wire: integers little-endian in their own width, floats as their bits, a count before
// the elements of a string or a vector, a flag before an optional value, and the alternative's index before a variant's.
class WireWriter
{
public:
  [[nodiscard]] std::vector<std::byte> Take() noexcept
  {
    return std::move(m_bytes);
  }

  void Put(bool _value)
  {
    PutBits(static_cast<std::uint8_t>(_value ? 1 : 0));
  }

  template <std::integral T> void Put(T _value)
  {
    PutBits(static_cast<std::make_unsigned_t<T>>(_value));
  }

  void Put(float _value)
  {
    PutBits(std::bit_cast<std::uint32_t>(_value));
  }

  void Put(double _value)
  {
    PutBits(std::bit_cast<std::uint64_t>(_value));
  }

  template <typename E>
    requires std::is_enum_v<E>
  void Put(E _value)
  {
    Put(std::to_underlying(_value));
  }

  template <typename Tag> void Put(Id<Tag> _value)
  {
    Put(_value.value);
  }

  void Put(const std::string& _value)
  {
    PutCount(_value.size());
    for (const char character : _value)
      PutBits(static_cast<std::uint8_t>(character));
  }

  template <typename T> void Put(const std::vector<T>& _values)
  {
    PutCount(_values.size());
    for (const T& value : _values)
      Put(value);
  }

  template <typename T> void Put(const std::optional<T>& _value)
  {
    Put(_value.has_value());
    if (_value)
      Put(*_value);
  }

  template <typename... Ts> void Put(const std::variant<Ts...>& _value)
  {
    Put(static_cast<std::uint8_t>(_value.index()));
    std::visit([this](const auto& _alternative) { Put(_alternative); }, _value);
  }

  template <WireRecord T> void Put(const T& _value)
  {
    std::apply([this](const auto&... _fields) { (Put(_fields), ...); }, Fields(_value));
  }

private:
  template <std::unsigned_integral T> void PutBits(T _bits)
  {
    for (std::size_t i = 0; i < sizeof(T); ++i)
      m_bytes.push_back(static_cast<std::byte>(_bits >> (8 * i)));
  }

  void PutCount(std::size_t _count)
  {
    if (_count > std::numeric_limits<std::uint32_t>::max())
      throw Neuron::Exception(std::format("A message cannot hold {} elements in one field.", _count));
    PutBits(static_cast<std::uint32_t>(_count));
  }

  std::vector<std::byte> m_bytes;
};

// Reads what WireWriter wrote, and refuses anything it could not have written.
class WireReader
{
public:
  explicit WireReader(std::span<const std::byte> _bytes) noexcept
    : m_bytes(_bytes)
  {
  }

  void Get(bool& _value)
  {
    std::uint8_t bits = 0;
    GetBits(bits);
    if (bits > 1)
      Malformed("a flag is neither 0 nor 1");
    _value = bits == 1;
  }

  template <std::integral T> void Get(T& _value)
  {
    std::make_unsigned_t<T> bits = 0;
    GetBits(bits);
    _value = static_cast<T>(bits);
  }

  void Get(float& _value)
  {
    std::uint32_t bits = 0;
    GetBits(bits);
    _value = std::bit_cast<float>(bits);
  }

  void Get(double& _value)
  {
    std::uint64_t bits = 0;
    GetBits(bits);
    _value = std::bit_cast<double>(bits);
  }

  template <typename E>
    requires std::is_enum_v<E>
  void Get(E& _value)
  {
    std::underlying_type_t<E> raw{};
    Get(raw);
    if (raw > std::to_underlying(LastOf(E{})))
      Malformed(std::format("{} is not a value of its enumeration", raw));
    _value = static_cast<E>(raw);
  }

  template <typename Tag> void Get(Id<Tag>& _value)
  {
    Get(_value.value);
  }

  void Get(std::string& _value)
  {
    const std::span<const std::byte> bytes = Take(GetCount());
    _value.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  }

  // The elements are added as they are read, so a count the bytes cannot back runs out of bytes before it runs out of
  // memory.
  template <typename T> void Get(std::vector<T>& _values)
  {
    const std::size_t count = GetCount();
    _values.clear();
    for (std::size_t i = 0; i < count; ++i)
      Get(_values.emplace_back());
  }

  template <typename T> void Get(std::optional<T>& _value)
  {
    bool present = false;
    Get(present);
    if (present)
      Get(_value.emplace());
    else
      _value.reset();
  }

  template <typename... Ts> void Get(std::variant<Ts...>& _value)
  {
    std::uint8_t index = 0;
    Get(index);
    if (index >= sizeof...(Ts))
      Malformed(std::format("{} is not an alternative of its variant", index));
    GetAlternative(_value, index, std::index_sequence_for<Ts...>{});
  }

  template <WireRecord T> void Get(T& _value)
  {
    std::apply([this](auto&... _fields) { (Get(_fields), ...); }, Fields(_value));
  }

  void Finish() const
  {
    if (!m_bytes.empty())
      Malformed(std::format("{} bytes run on after it", m_bytes.size()));
  }

private:
  template <typename... Ts, std::size_t... Indices>
  void GetAlternative(std::variant<Ts...>& _value, std::size_t _index, std::index_sequence<Indices...>)
  {
    (void)((Indices == _index && (Get(_value.template emplace<Indices>()), true)) || ...);
  }

  template <std::unsigned_integral T> void GetBits(T& _bits)
  {
    const std::span<const std::byte> bytes = Take(sizeof(T));
    _bits = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i)
      _bits = static_cast<T>(_bits | (static_cast<T>(bytes[i]) << (8 * i)));
  }

  // A count of elements, each of which takes a byte at least, so it is never more than the bytes left.
  [[nodiscard]] std::size_t GetCount()
  {
    std::uint32_t count = 0;
    GetBits(count);
    if (count > m_bytes.size())
      Malformed(std::format("it counts {} elements with {} bytes left", count, m_bytes.size()));
    return count;
  }

  [[nodiscard]] std::span<const std::byte> Take(std::size_t _count)
  {
    if (_count > m_bytes.size())
      Malformed("it ends early");
    const std::span<const std::byte> taken = m_bytes.first(_count);
    m_bytes = m_bytes.subspan(_count);
    return taken;
  }

  std::span<const std::byte> m_bytes;
};

template <typename T> std::vector<std::byte> Encode(const T& _message)
{
  WireWriter writer;
  writer.Put(KindOf<T>(std::type_identity<Message>{}));
  writer.Put(_message);
  return writer.Take();
}
} // namespace

std::vector<std::byte> EncodeMessage(const HelloMessage& _hello)
{
  return Encode(_hello);
}

std::vector<std::byte> EncodeMessage(const WelcomeMessage& _welcome)
{
  return Encode(_welcome);
}

std::vector<std::byte> EncodeMessage(const Command& _command)
{
  return Encode(_command);
}

std::vector<std::byte> EncodeMessage(const Snapshot& _snapshot)
{
  return Encode(_snapshot);
}

Message DecodeMessage(std::span<const std::byte> _bytes)
{
  WireReader reader(_bytes);
  Message message;
  reader.Get(message);
  reader.Finish();
  return message;
}
} // namespace Outpost
