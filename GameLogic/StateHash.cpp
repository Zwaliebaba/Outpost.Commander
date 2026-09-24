#include "pch.h"

#include "StateHash.h"

namespace Outpost
{

namespace
{
/// FNV-1a, 64 bit. Chosen because it is four lines and has no lookup table, no seed and no
/// endianness of its own -- every byte that reaches it was placed there deliberately by the code
/// below. It is not a cryptographic hash and does not need to be: this detects two hosts drifting
/// apart, not an attacker forging a match.
inline constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
inline constexpr std::uint64_t FNV_PRIME = 1099511628211ull;

void FoldByte(std::uint64_t& _hash, std::uint8_t _value) noexcept
{
  _hash ^= _value;
  _hash *= FNV_PRIME;
}

/// Little end first, always, whatever the machine is. An architecture's own byte order never
/// reaches this function, which is the point: x64 and ARM64 are both little-endian today and the
/// hash must not depend on that staying true.
void FoldUInt16(std::uint64_t& _hash, std::uint16_t _value) noexcept
{
  FoldByte(_hash, static_cast<std::uint8_t>(_value & 0xFFu));
  FoldByte(_hash, static_cast<std::uint8_t>((_value >> 8) & 0xFFu));
}

void FoldUInt32(std::uint64_t& _hash, std::uint32_t _value) noexcept
{
  FoldUInt16(_hash, static_cast<std::uint16_t>(_value & 0xFFFFu));
  FoldUInt16(_hash, static_cast<std::uint16_t>((_value >> 16) & 0xFFFFu));
}

void FoldUInt64(std::uint64_t& _hash, std::uint64_t _value) noexcept
{
  FoldUInt32(_hash, static_cast<std::uint32_t>(_value & 0xFFFFFFFFull));
  FoldUInt32(_hash, static_cast<std::uint32_t>((_value >> 32) & 0xFFFFFFFFull));
}

void FoldIdentity(std::uint64_t& _hash, EntityId _id) noexcept
{
  FoldUInt16(_hash, _id.index);
  FoldUInt16(_hash, _id.generation);
}

/// Through the unsigned width, so the representation folded is the bit pattern and not a
/// conversion the standard leaves room in.
void FoldFixed(std::uint64_t& _hash, Neuron::Fixed _value) noexcept
{
  FoldUInt32(_hash, static_cast<std::uint32_t>(_value));
}
} // namespace

std::uint64_t StateHash(const World& _world) noexcept
{
  std::uint64_t hash = FNV_OFFSET_BASIS;

  const std::size_t slotCount = _world.SlotCount();
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }

    const Entity& entity = _world.EntityInSlot(slot);

    // ADR-002's fields, and in a fixed order. Adding a field here is a change to what "the same
    // match" means, so it belongs with the ADR rather than with whoever needed another.
    FoldIdentity(hash, entity.id);
    FoldFixed(hash, entity.position.x);
    FoldFixed(hash, entity.position.y);
    FoldUInt16(hash, entity.heading);
    // `HullId` is `std::uint8_t`-backed, so M1.1 turning it from a typedef into an enumeration changed
    // the type and not the value (R16, ADR-002).
    FoldByte(hash, static_cast<std::uint8_t>(entity.hull));

    // **THE 2026-09-23 REVIEW'S M6**: what the arithmetic moves, not only where it puts things. Hull points
    // are what M3's damage writes; the owner is set once and costs a byte to be sure of; the mine order is
    // the economy's state, cargo to the thousandth of ore.
    FoldUInt16(hash, entity.hullRemaining);
    FoldByte(hash, entity.owner);
    const MineOrder& mine = _world.MineInSlot(slot);
    FoldByte(hash, static_cast<std::uint8_t>(mine.phase));
    FoldUInt16(hash, mine.rock);
    FoldUInt32(hash, mine.cargoMilliOre);

    // **M3.2: WHAT THE WEAPONS ARE OWED AND WHAT THE SHIP IS ATTACKING** (ADR-014). The remainders are the
    // fractions of damage not yet taken off a hull, so a build that rounds them differently diverges here on the
    // tick it does rather than a kill later.
    const WeaponState& weapons = _world.WeaponsInSlot(slot);
    FoldIdentity(hash, weapons.target);
    for (const std::uint32_t remainder : weapons.remainders)
    {
      FoldUInt32(hash, remainder);
    }
    const AttackOrder& attack = _world.AttackInSlot(slot);
    FoldByte(hash, attack.active ? 1 : 0);
    FoldIdentity(hash, attack.target);
    FoldIdentity(hash, mine.unloadTarget);
  }

  return hash;
}

std::uint64_t MatchHash(const World& _world, const BuildSystem& _build, const Economy& _economy) noexcept
{
  std::uint64_t hash = StateHash(_world);

  for (std::size_t slot = 1; slot <= _build.PlayerCount(); ++slot)
  {
    const PlayerId player = static_cast<PlayerId>(slot);
    FoldUInt32(hash, _build.Credits(player));
    FoldUInt64(hash, _economy.PendingMilliCreditHundredths(player));

    const BuildItem& item = _build.Item(player);
    FoldByte(hash, static_cast<std::uint8_t>(item.active ? 1 : 0));
    FoldByte(hash, static_cast<std::uint8_t>(item.design));
    FoldUInt32(hash, item.ticksElapsed);
    FoldUInt32(hash, item.ticksRequired);
    FoldUInt32(hash, item.creditsSpent);
    FoldFixed(hash, item.site.x);
    FoldFixed(hash, item.site.y);
    FoldIdentity(hash, item.upgrade);
  }

  return hash;
}

} // namespace Outpost
