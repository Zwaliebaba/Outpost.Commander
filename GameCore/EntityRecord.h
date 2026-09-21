#pragma once

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>

namespace Outpost
{

/// ADR-003's ten bytes, and this file is where its widths stop being prose and become facts
/// (`README.md` F7). The ADR names the contents and gives the total; only an encoder settles how
/// wide each field is, and what this produces is what the design's table gets corrected against.
///
/// | field           | bytes |
/// |-----------------|-------|
/// | identity        |     2 | index and generation packed -- see below
/// | position x, y   |     4 | two `std::int16_t`, a quarter of a world unit per step
/// | heading         |     1 | 256 steps, 1.4 degrees; the simulation's heading is sixteen bits
/// | hull remaining  |     1 | percent
/// | design identity |     1 | its own byte, because R24 has a design being an identity that
/// |                 |       | research and a designer extend -- two bits was four designs forever
/// | flags           |     1 | team 2, state 3, cargo 2, one spare
///
/// R8: a wire record, so plain fields and brace initialization.
struct EntityRecord
{
  static constexpr std::size_t SIZE_BYTES = 10;

  std::uint16_t identity = 0;
  std::int16_t positionX = 0;
  std::int16_t positionY = 0;
  std::uint8_t heading = 0;
  std::uint8_t hullPercentRemaining = 0;
  std::uint8_t designIdentity = 0;
  std::uint8_t flags = 0;

  [[nodiscard]] bool Write(Neuron::ByteWriter& _writer) const noexcept;
  [[nodiscard]] static bool Read(Neuron::ByteReader& _reader, EntityRecord& _outRecord) noexcept;

  [[nodiscard]] friend constexpr bool operator==(const EntityRecord&, const EntityRecord&) noexcept = default;
};

/// THE SPLIT INSIDE THE IDENTITY, WHICH THE DESIGN DID NOT STATE AND THIS STEP HAD TO SETTLE.
/// ADR-003 says two bytes holding "index and generation packed" and stops there.
///
/// Ten bits of index is 1,024 slots against the MVP's 110 entities and 220 after M2 -- four times
/// the largest count the design contemplates. Six bits of generation is 64 values, and what that
/// has to survive is narrower than it looks: the generation exists so the client can tell a new
/// occupant of a slot from the old one between two snapshots fifty milliseconds apart, and so the
/// host can reject a stale identity arriving on a command. Aliasing needs one slot reused 64 times
/// inside that window, which twenty ticks a second does not reach.
///
/// The store's own identity is wider -- two `std::uint16_t` (`GameCore/Entity.h`) -- and that is
/// deliberate: R16's simulation is not obliged to the wire's precision, and the wire is sized by
/// what the client can actually use.
inline constexpr std::uint16_t WIRE_INDEX_BITS = 10;
inline constexpr std::uint16_t WIRE_GENERATION_BITS = 6;
inline constexpr std::uint16_t WIRE_INDEX_MASK = (std::uint16_t{1} << WIRE_INDEX_BITS) - 1;
inline constexpr std::uint16_t WIRE_GENERATION_MASK = (std::uint16_t{1} << WIRE_GENERATION_BITS) - 1;

/// True when an index can be named on the wire at all. An index past this is not truncated
/// quietly -- the encoder refuses, because a truncated identity resolves to the wrong entity.
[[nodiscard]] constexpr bool FitsWireIdentity(std::uint16_t _index) noexcept
{
  return _index <= WIRE_INDEX_MASK;
}

[[nodiscard]] constexpr std::uint16_t PackIdentity(std::uint16_t _index, std::uint16_t _generation) noexcept
{
  return static_cast<std::uint16_t>((_index & WIRE_INDEX_MASK) |
                                    static_cast<std::uint16_t>((_generation & WIRE_GENERATION_MASK) << WIRE_INDEX_BITS));
}

[[nodiscard]] constexpr std::uint16_t IndexOf(std::uint16_t _identity) noexcept
{
  return static_cast<std::uint16_t>(_identity & WIRE_INDEX_MASK);
}

[[nodiscard]] constexpr std::uint16_t GenerationOf(std::uint16_t _identity) noexcept
{
  return static_cast<std::uint16_t>((_identity >> WIRE_INDEX_BITS) & WIRE_GENERATION_MASK);
}

/// The flags byte's layout, recorded here because `TechnicalDesign.md` section 4 states it and
/// nothing else in the tree does. THE SEMANTICS ARE NOT SETTLED HERE: there are no teams, no
/// entity states and no cargo buckets at M0, and inventing enumerators for them would be this
/// file deciding things the game design owns. The bit positions are the format; what goes in them
/// arrives with the systems that have something to say.
inline constexpr std::uint8_t FLAGS_TEAM_SHIFT = 0;
inline constexpr std::uint8_t FLAGS_TEAM_MASK = 0x03;
inline constexpr std::uint8_t FLAGS_STATE_SHIFT = 2;
inline constexpr std::uint8_t FLAGS_STATE_MASK = 0x07;
inline constexpr std::uint8_t FLAGS_CARGO_SHIFT = 5;
inline constexpr std::uint8_t FLAGS_CARGO_MASK = 0x03;
inline constexpr std::uint8_t FLAGS_SPARE_SHIFT = 7;

/// ONE WIRE STEP IS A QUARTER OF A WORLD UNIT, and this is that quarter expressed in the
/// simulation's own units: `Fixed` carries 256 steps to a world unit and the wire carries four, so
/// 64 of the first make one of the second. The 16,384-unit square over 65,536 steps is an exact
/// fit, which is why there is no slack to find in this field.
inline constexpr Neuron::Fixed POSITION_WIRE_STEP = 64;

/// Rounded, not truncated, and symmetric about zero -- a quantizer that pulled toward the origin
/// would drift every ship in the match inward by an eighth of a unit on average.
///
/// The positive edge saturates. The play area's half extent is 2,097,152, which is exactly 32,768
/// wire steps, and an `int16_t` stops one short at 32,767; the negative edge reaches -32,768
/// exactly. One step of asymmetry at the very corner of the map, stated rather than discovered.
[[nodiscard]] std::int16_t QuantizePosition(Neuron::Fixed _value) noexcept;

[[nodiscard]] Neuron::Fixed DequantizePosition(std::int16_t _value) noexcept;

} // namespace Outpost
