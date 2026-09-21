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

    // ADR-002's four, and in a fixed order. Adding a field here is a change to what "the same
    // match" means, so it belongs with the ADR rather than with whoever needed a fifth.
    FoldUInt16(hash, entity.id.index);
    FoldUInt16(hash, entity.id.generation);
    FoldFixed(hash, entity.position.x);
    FoldFixed(hash, entity.position.y);
    FoldUInt16(hash, entity.heading);
    FoldByte(hash, entity.hull);
  }

  return hash;
}

} // namespace Outpost
