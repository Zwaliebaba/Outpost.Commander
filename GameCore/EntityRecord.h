#pragma once

#include "Entity.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>

namespace Outpost
{

/// A packed wire identity: a 16-bit index in the low bits and an 8-bit generation above it, in the
/// low twenty-four bits of this type. Three bytes on the wire (ADR-024); a `std::uint32_t` in memory,
/// because there is no 24-bit integer and a struct of three bytes would be a type nobody can compare.
///
/// **ZERO IS NO IDENTITY.** Generation zero is never live (`GameCore/Entity.h`), so a packed zero
/// cannot name an entity and the client uses it for "nothing tapped".
using WireIdentity = std::uint32_t;

inline constexpr WireIdentity NO_WIRE_IDENTITY = 0;

/// ADR-024's twelve bytes. Every record is a **self-contained fact** about one entity at the
/// update's tick -- whose it is, where, facing where, how damaged, built as what -- and nothing in it
/// depends on any other record or on the datagram being complete. That is the property the whole
/// replication design rests on: any subset of an update is meaningful by itself.
///
/// | field           | bytes |
/// |-----------------|-------|
/// | identity        |     3 | index 16, generation 8 -- see below
/// | owner           |     1 | a `PlayerId`; it left the flags byte, which could name four players
/// | position x, y   |     4 | two `std::int16_t`, a quarter of a world unit per step
/// | heading         |     1 | 256 steps, 1.4 degrees; the simulation's heading is sixteen bits
/// | hull remaining  |     1 | percent
/// | design identity |     1 | its own byte, because R24 has a design being an identity that
/// |                 |       | research and a designer extend -- two bits was four designs forever
/// | flags           |     1 | state 3, cargo 3, two spare -- cargo took a spare bit at M2.7 (Q53)
///
/// R8: a wire record, so plain fields and brace initialization.
struct EntityRecord
{
  static constexpr std::size_t SIZE_BYTES = 12;

  WireIdentity identity = NO_WIRE_IDENTITY;
  PlayerId owner = NO_PLAYER;
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

/// THE SPLIT INSIDE THE IDENTITY. **SIXTEEN AND EIGHT SINCE ADR-024; TEN AND SIX BEFORE IT.**
///
/// Sixteen bits of index is the store's own width (`GameCore/Entity.h`), so every slot the store can
/// hand out can be named on the wire and there is no longer an index the encoder has to refuse. Ten
/// bits was 1,024 slots, which a hundred-player match passes five times over.
///
/// Eight bits of generation is 256 values, and what it has to survive is wider than it was. Under the
/// full snapshot a client saw every entity every tick, so aliasing needed one slot reused 64 times in
/// fifty milliseconds. Under ADR-024 a distant entity may go a whole sweep unrefreshed, so a slot freed
/// and refilled inside that window must still read as a different entity. 256 reuses of one slot
/// inside a sweep is not a thing twenty ticks a second can do.
///
/// The store's generation is wider still -- a `std::uint16_t` -- and that is deliberate: R16's
/// simulation is not obliged to the wire's precision, and the wire is sized by what the client uses.
inline constexpr std::uint32_t WIRE_INDEX_BITS = 16;
inline constexpr std::uint32_t WIRE_GENERATION_BITS = 8;
inline constexpr std::uint32_t WIRE_INDEX_MASK = (std::uint32_t{1} << WIRE_INDEX_BITS) - 1;
inline constexpr std::uint32_t WIRE_GENERATION_MASK = (std::uint32_t{1} << WIRE_GENERATION_BITS) - 1;

[[nodiscard]] constexpr WireIdentity PackIdentity(std::uint16_t _index, std::uint16_t _generation) noexcept
{
  return static_cast<WireIdentity>((static_cast<std::uint32_t>(_index) & WIRE_INDEX_MASK) |
                                   ((static_cast<std::uint32_t>(_generation) & WIRE_GENERATION_MASK) << WIRE_INDEX_BITS));
}

[[nodiscard]] constexpr std::uint16_t IndexOf(WireIdentity _identity) noexcept
{
  return static_cast<std::uint16_t>(_identity & WIRE_INDEX_MASK);
}

[[nodiscard]] constexpr std::uint16_t GenerationOf(WireIdentity _identity) noexcept
{
  return static_cast<std::uint16_t>((_identity >> WIRE_INDEX_BITS) & WIRE_GENERATION_MASK);
}

/// The flags byte's layout. **The state field says what a ship is visibly doing** (`OpenQuestions.md` Q81): two
/// of its eight values are taken, by the two things a miner does that the owner has to be able to see, and the
/// rest are unassigned. **The team bits are gone** -- the owner has its own byte (ADR-024).
///
/// **CARGO IS THREE BITS SINCE M2.7** (`OpenQuestions.md` Q53): the selection panel draws four chips, and
/// empty through full is five states, which two bits could not say. It took one of three spare bits, so
/// the byte -- and the record -- did not grow.
inline constexpr std::uint8_t FLAGS_STATE_SHIFT = 0;
inline constexpr std::uint8_t FLAGS_STATE_MASK = 0x07;
inline constexpr std::uint8_t FLAGS_CARGO_SHIFT = 3;
inline constexpr std::uint8_t FLAGS_CARGO_MASK = 0x07;
inline constexpr std::uint8_t FLAGS_SPARE_SHIFT = 6;

/// **WHAT A SHIP IS VISIBLY DOING** (Q81), in the flags' state field: enough for a client to draw a mining beam
/// without being told the order behind it (R19). Firing is not here -- a shot is a `FireEvent` (ADR-004).
enum class Activity : std::uint8_t
{
  None = 0,

  /// Holding the rock and taking ore from it this tick. A miner waiting its turn at a worked rock (Q62) is not.
  Extracting = 1,

  /// Alongside a station or refinery and emptying the hold into it.
  Unloading = 2
};

/// The activity a record's flags carry. A value nothing assigns reads as `None`, so a newer host's third
/// activity draws as nothing on an older client rather than as the wrong beam.
[[nodiscard]] constexpr Activity ActivityOf(std::uint8_t _flags) noexcept
{
  const std::uint8_t state = static_cast<std::uint8_t>((_flags >> FLAGS_STATE_SHIFT) & FLAGS_STATE_MASK);
  return (state <= static_cast<std::uint8_t>(Activity::Unloading)) ? static_cast<Activity>(state) : Activity::None;
}

/// _flags with its state field replaced by _activity.
[[nodiscard]] constexpr std::uint8_t WithActivity(std::uint8_t _flags, Activity _activity) noexcept
{
  return static_cast<std::uint8_t>((_flags & ~(FLAGS_STATE_MASK << FLAGS_STATE_SHIFT)) |
                                   ((static_cast<std::uint8_t>(_activity) & FLAGS_STATE_MASK) << FLAGS_STATE_SHIFT));
}

/// The most chips a hold lights: the selection panel's four (`design_handoff_hud`).
inline constexpr std::uint8_t CARGO_CHIP_COUNT = 4;

/// **HOW MANY OF THE FOUR CHIPS A HOLD LIGHTS** (Q53): none when empty, all four when full, and quarters
/// **rounded up** between -- so any ore at all lights one chip, and "carrying something" never reads as
/// empty. A capacity of zero is a design that carries no ore, and lights nothing.
///
/// Integer throughout: in thousandths of ore a hold is at most a few hundred thousand, and four times that
/// is nowhere near 32 bits.
[[nodiscard]] constexpr std::uint8_t CargoChips(std::uint32_t _cargoMilliOre, std::uint32_t _capacityMilliOre) noexcept
{
  if ((_capacityMilliOre == 0) || (_cargoMilliOre == 0))
  {
    return 0;
  }
  if (_cargoMilliOre >= _capacityMilliOre)
  {
    return CARGO_CHIP_COUNT;
  }
  return static_cast<std::uint8_t>(((CARGO_CHIP_COUNT * _cargoMilliOre) + _capacityMilliOre - 1) / _capacityMilliOre);
}

/// The chips a record's flags carry, 0 to 4.
[[nodiscard]] constexpr std::uint8_t CargoChipsOf(std::uint8_t _flags) noexcept
{
  return static_cast<std::uint8_t>((_flags >> FLAGS_CARGO_SHIFT) & FLAGS_CARGO_MASK);
}

/// _flags with its cargo field replaced by _chips, clamped to four so a caller cannot write the three
/// states the field has room for and nothing means.
[[nodiscard]] constexpr std::uint8_t WithCargoChips(std::uint8_t _flags, std::uint8_t _chips) noexcept
{
  const std::uint8_t clamped = (_chips > CARGO_CHIP_COUNT) ? CARGO_CHIP_COUNT : _chips;
  return static_cast<std::uint8_t>((_flags & ~(FLAGS_CARGO_MASK << FLAGS_CARGO_SHIFT)) | (clamped << FLAGS_CARGO_SHIFT));
}

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

/// The simulation's sixteen-bit heading onto ADR-003's byte, and back.
///
/// **M0.19 PUT THE WIDENING IN `GameClient` AND SAID WHY IT WOULD MOVE**: only the client needed it
/// then, and "its inverse belongs beside `QuantizePosition` when the host needs it". M1.3 is when
/// the host needs it -- the host's encoder was doing the shift by hand -- so both halves live here
/// with the rest of the wire's rounding rules.
///
/// **THE LOSS IS ADR-003's AND IS NOT A ROUNDING CHOICE.** 256 steps of 1.4 degrees is a rendering
/// quantity; the simulation keeps sixteen bits because movement and turning are computed in them.
/// The quantizer truncates rather than rounding, because rounding up from 0xFF80 would wrap a
/// heading past north and a ship pointing very slightly west would draw pointing east.
[[nodiscard]] constexpr std::uint8_t QuantizeWireHeading(Neuron::Angle _heading) noexcept
{
  return static_cast<std::uint8_t>(_heading >> 8);
}

[[nodiscard]] constexpr Neuron::Angle DequantizeWireHeading(std::uint8_t _wireHeading) noexcept
{
  return static_cast<Neuron::Angle>(static_cast<Neuron::Angle>(_wireHeading) << 8);
}

/// Hull points to ADR-003's percentage byte, and back.
///
/// **ROUNDED TO NEAREST, AND THE TWO ENDS ARE EXACT.** A ship on its last point must not read as
/// dead and an undamaged one must not read as damaged, so zero maps to zero and full maps to a
/// hundred whatever the rounding would otherwise do -- everything between is nearest, which is what
/// keeps the round trip inside one percent.
///
/// A maximum of zero is a design with no hull points, which the catalog does not contain; it reads
/// as a hundred rather than dividing.
///
/// **AND NOTHING STILL ALIVE READS AS ZERO.** One point of a `Station`'s eight thousand rounds to
/// nought, and a client that draws an empty bar over a base which is still firing has been told a
/// worse lie than the one percent this floor tells instead.
[[nodiscard]] std::uint8_t QuantizeHullPercent(std::uint32_t _remaining, std::uint32_t _maximum) noexcept;

/// The percentage back to points. **It cannot be exact and is not meant to be**: a hundred buckets
/// over a `Station`'s 8,000 points is 80 points a bucket, which is the resolution ADR-003 bought
/// when it spent one byte on this. The client draws a bar with it and the host never reads it.
[[nodiscard]] std::uint32_t DequantizeHullPoints(std::uint8_t _percent, std::uint32_t _maximum) noexcept;

} // namespace Outpost
