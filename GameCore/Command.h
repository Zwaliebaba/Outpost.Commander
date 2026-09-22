#pragma once

#include "Entity.h"
#include "EntityRecord.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// What a player asked for. ZERO IS NOT A TYPE, for the reason PacketType gives: a command nobody
/// set, a zero-filled buffer and a zero-length packet all decode to zero, and none of them should
/// look like an order.
enum class CommandType : std::uint8_t
{
  /// The selected entities go to a point. The target is a point.
  MoveTo = 1,
  /// The selected entities attack an entity. The target is an identity, in targetX.
  Attack = 2,

  /// **Build a design at this player's station** (M1.6, `GameDesign.md` section 5). The design
  /// identity is the low byte of targetX and **the selection must be empty**: a player has exactly
  /// one station, so naming it would be a second thing to validate for nothing -- and an empty
  /// selection is what keeps a malformed build order from smuggling 600 identities through the
  /// amplification path Q24 is about.
  Build = 3,

  /// Cancel what is building, at a full refund (Q35). No target and no selection.
  CancelBuild = 4
};

[[nodiscard]] constexpr bool IsKnown(CommandType _type) noexcept
{
  return (_type == CommandType::MoveTo) || (_type == CommandType::Attack) || (_type == CommandType::Build) ||
         (_type == CommandType::CancelBuild);
}

/// True for the types that act on a selection. **The two that do not are the station's**, and the
/// distinction is what the intake's empty-selection check turns on: an empty selection is malformed
/// for a move and required for a build.
[[nodiscard]] constexpr bool ActsOnSelection(CommandType _type) noexcept
{
  return (_type == CommandType::MoveTo) || (_type == CommandType::Attack);
}

/// An order, as `TechnicalDesign.md` section 4 upstream describes it: a type, a target point or
/// entity, the identities of the selected ships, and a per-player sequence number.
///
/// EIGHT FIXED BYTES, and the target is four of them whichever kind it is. A point spends them as
/// two wire positions; an entity spends two on an identity and leaves the other two zero. That is
/// why there is one target field rather than a point and an identity side by side -- a command
/// carries one target, and a record with room for both would let an encoder write a command that
/// means two things.
///
/// R8: a wire record, so plain fields and brace initialization.
struct Command
{
  /// sequence 2, type 1, target 4, selection count 1.
  static constexpr std::size_t FIXED_BYTES = 8;

  /// The largest selection the count byte can name. ADR-003's peak is 110 entities, so this is
  /// not a limit anything reaches -- it is the limit the FORMAT imposes, and the host bounds the
  /// selection much harder than this at intake (Q24).
  static constexpr std::size_t MAX_SELECTION = 255;

  /// Per player, and it wraps. The host compares it as a signed difference rather than with "at
  /// or below", which inverts at the wrap -- see CommandIntake.
  std::uint16_t sequence = 0;

  CommandType type = CommandType::MoveTo;

  /// A wire position when the type takes a point; the low field is a packed identity when it
  /// takes an entity, and its **low byte is a design identity** for `Build`. Wire units, so a target
  /// is bounded by what an `int16_t` can say before anything validates it -- which does not make the
  /// host's clamp unnecessary, only cheap.
  std::int16_t targetX = 0;
  std::int16_t targetY = 0;

  /// Packed wire identities, as an EntityRecord's identity is packed.
  std::vector<std::uint16_t> selection;

  [[nodiscard]] std::uint16_t TargetEntity() const noexcept
  {
    return static_cast<std::uint16_t>(targetX);
  }

  /// The design a `Build` names. **The low byte only**, so a client that left rubbish in the high
  /// byte still names the design it meant rather than one the host has to reject.
  [[nodiscard]] std::uint8_t TargetDesign() const noexcept
  {
    return static_cast<std::uint8_t>(static_cast<std::uint16_t>(targetX) & 0xFF);
  }

  [[nodiscard]] friend bool operator==(const Command&, const Command&) noexcept = default;
};

/// A packet of them. ADR-003: commands are repeated in every outgoing packet until the snapshot's
/// `lastCommandSeqApplied` reaches them, so a packet holds however many are outstanding -- and
/// `TechnicalDesign.md` section 4 bounds that structurally by filling OLDEST-FIRST and stopping
/// when the next one will not fit.
struct CommandPacket
{
  /// The transport's six, plus the player identity and the command count.
  static constexpr std::size_t HEADER_BYTES = Neuron::PacketHeader::SIZE_BYTES + 2;

  static constexpr std::size_t MAX_COMMANDS = 255;

  std::uint16_t sequence = 0;
  PlayerId player = NO_PLAYER;
  std::vector<Command> commands;
};

enum class CommandFault : std::uint8_t
{
  None,
  Truncated,
  VersionMismatch,
  /// A well-formed packet that is not a command packet.
  WrongType,
  Fragmented,
  /// A type this build does not know, or a count the rest of the datagram cannot satisfy.
  Malformed
};

[[nodiscard]] std::size_t EncodedSize(const Command& _command) noexcept;
[[nodiscard]] std::size_t EncodedSize(const CommandPacket& _packet) noexcept;

/// False when the buffer will not hold it, when a count exceeds what the wire can name, or when
/// the type is not one this build knows.
[[nodiscard]] bool Encode(const CommandPacket& _packet, Neuron::ByteWriter& _writer) noexcept;

/// On anything but CommandFault::None, _outPacket is left alone.
[[nodiscard]] CommandFault Decode(Neuron::ByteReader& _reader, CommandPacket& _outPacket) noexcept;

/// OLDEST FIRST, STOPPING WHEN THE NEXT ONE WILL NOT FIT. `TechnicalDesign.md` section 4: what
/// reaches a deep retransmit window is not a fast player -- touch cannot issue five orders in
/// 200 ms -- it is a stalled acknowledgment, which is exactly the load under which a fragmented
/// command packet is worst. Filling this way makes the bound structural rather than a constant to
/// tune: the packet cannot exceed the payload, no order is ever dropped, and the sequence never
/// gains a gap, which matters because the host applies in sequence order and discards a gap
/// rather than waiting for it.
///
/// Returns how many of _outstanding were packed. The rest wait one packet, fifty milliseconds --
/// and they were already waiting on the stall that made the window deep.
[[nodiscard]] std::size_t FillOldestFirst(const std::vector<Command>& _outstanding, std::size_t _payloadBytes, CommandPacket& _outPacket);

/// The play area's half extent in `Fixed`, which is ADR-001's 16,384-unit square: 8,192 world
/// units either side of the origin, at 256 steps to a unit.
inline constexpr Neuron::Fixed PLAY_AREA_HALF_EXTENT = 2097152;

/// Q24's fourth check. THE WIRE ALREADY BOUNDS A TARGET -- a wire position is an `int16_t` and
/// sixteen bits times the 64-unit step is exactly this extent -- so a point arriving from a
/// decoder is in the play area before anything looks at it. This is written anyway, because the
/// check that is only true by a coincidence of two field widths is the check that stops being
/// true when one of them moves.
[[nodiscard]] Neuron::Vec2 ClampToPlayArea(const Neuron::Vec2& _point) noexcept;

} // namespace Outpost
