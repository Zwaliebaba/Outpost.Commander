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
  /// The selected entities attack an entity. The target is a packed identity across both target
  /// fields -- see `Command::TargetEntity`.
  Attack = 2,

  /// **Build a design at this player's station** (M1.6, `GameDesign.md` section 5). The design
  /// identity is the low byte of targetX and **the selection must be empty**: a player has exactly
  /// one station, so naming it would be a second thing to validate for nothing -- and an empty
  /// selection is what keeps a malformed build order from smuggling 600 identities through the
  /// amplification path Q24 is about.
  Build = 3,

  /// Cancel what is building, at a full refund (Q35). No target and no selection.
  CancelBuild = 4,

  /// **The selected miners mine an asteroid, and keep mining until told otherwise** (M2.6, `GameDesign.md`
  /// section 4) -- the one standing order. The target is the rock's **index in `GenerateField`'s order**,
  /// in `targetX`, with `targetY` zero (`OpenQuestions.md` Q52): asteroids are not world entities before
  /// M3, and both sides derive the same list from the same seed and count, so an index names the same
  /// rock on both.
  Mine = 5,

  /// **Build a module at a point** (M2.11, `OpenQuestions.md` Q55). The point is the target at full wire
  /// precision, and the design is **a ninth fixed byte that only this type carries** -- the target's four
  /// bytes are all spent on the point. The selection is empty, as a `Build`'s is. The host validates the
  /// site with `CheckModuleSite` before anything is spent.
  PlaceModule = 6,

  /// **Upgrade one of this player's modules in place** (M2.11, Q54, Q55). The target is the module's
  /// packed identity, as an `Attack`'s is, and **the level it becomes is `targetY`'s high byte**, which an
  /// identity leaves spare. Eight bytes, an empty selection, and the host pays the difference.
  UpgradeModule = 7
};

[[nodiscard]] constexpr bool IsKnown(CommandType _type) noexcept
{
  return (_type == CommandType::MoveTo) || (_type == CommandType::Attack) || (_type == CommandType::Build) ||
         (_type == CommandType::CancelBuild) || (_type == CommandType::Mine) || (_type == CommandType::PlaceModule) ||
         (_type == CommandType::UpgradeModule);
}

/// **THE ONE TYPE WHOSE FIXED PART IS NINE BYTES** (Q55): a `PlaceModule` carries a design byte after the
/// target. Every other type is eight, which is why a count is still bounded by `Command::FIXED_BYTES`.
[[nodiscard]] constexpr bool CarriesDesignByte(CommandType _type) noexcept
{
  return _type == CommandType::PlaceModule;
}

/// True for the types that act on a selection. **The four that do not are the station's**, and the
/// distinction is what the intake's empty-selection check turns on: an empty selection is malformed
/// for a move and required for a build.
[[nodiscard]] constexpr bool ActsOnSelection(CommandType _type) noexcept
{
  return (_type == CommandType::MoveTo) || (_type == CommandType::Attack) || (_type == CommandType::Mine);
}

/// An order, as `TechnicalDesign.md` section 4 upstream describes it: a type, a target point or
/// entity, the identities of the selected ships, and a per-player sequence number.
///
/// EIGHT FIXED BYTES, and the target is four of them whichever kind it is. A point spends them as
/// two wire positions; an entity spends three on a packed identity -- the index in `targetX`, the
/// generation in the low byte of `targetY` -- and leaves the fourth zero (ADR-024). That is
/// why there is one target field rather than a point and an identity side by side -- a command
/// carries one target, and a record with room for both would let an encoder write a command that
/// means two things. **`PlaceModule` is the one exception**, a ninth byte for a design beside a point
/// (M2.11, Q55), and it is written only for that type.
///
/// R8: a wire record, so plain fields and brace initialization.
struct Command
{
  /// sequence 2, type 1, target 4, selection count 1.
  static constexpr std::size_t FIXED_BYTES = 8;

  /// A selected identity on the wire: three bytes since ADR-024, as a record's is.
  static constexpr std::size_t IDENTITY_BYTES = 3;

  /// The largest selection the count byte can name. ADR-003's peak is 110 entities, so this is
  /// not a limit anything reaches -- it is the limit the FORMAT imposes, and the host bounds the
  /// selection much harder than this at intake (Q24).
  static constexpr std::size_t MAX_SELECTION = 255;

  /// Per player, and it wraps. The host compares it as a signed difference rather than with "at
  /// or below", which inverts at the wrap -- see CommandIntake.
  std::uint16_t sequence = 0;

  CommandType type = CommandType::MoveTo;

  /// A wire position when the type takes a point; the two together are a packed identity when it
  /// takes an entity, and `targetX`'s **low byte is a design identity** for `Build`. Wire units, so a target
  /// is bounded by what an `int16_t` can say before anything validates it -- which does not make the
  /// host's clamp unnecessary, only cheap.
  std::int16_t targetX = 0;
  std::int16_t targetY = 0;

  /// **A `PlaceModule`'s design, and on the wire for no other type** (Q55). Zero and ignored everywhere
  /// else, so a decoder that did not read it leaves it zero and a round trip compares equal.
  std::uint8_t placedDesign = 0;

  /// Packed wire identities, as an EntityRecord's identity is packed.
  std::vector<WireIdentity> selection;

  /// The entity an `Attack` names: the index from `targetX` and the generation from `targetY`'s low
  /// byte, which is the split `PackIdentity` makes.
  [[nodiscard]] WireIdentity TargetEntity() const noexcept
  {
    return PackIdentity(static_cast<std::uint16_t>(targetX), static_cast<std::uint16_t>(static_cast<std::uint16_t>(targetY) & 0xFF));
  }

  /// The inverse, so that nothing outside this struct has to know which half of the identity went in
  /// which field.
  void AimAt(WireIdentity _identity) noexcept
  {
    targetX = static_cast<std::int16_t>(IndexOf(_identity));
    targetY = static_cast<std::int16_t>(GenerationOf(_identity));
  }

  /// The rock a `Mine` names, as an index into the generated field. **Unsigned**, so a negative `targetX`
  /// reads as a large index the host refuses rather than as a small one it would accept.
  [[nodiscard]] std::uint16_t TargetRock() const noexcept
  {
    return static_cast<std::uint16_t>(targetX);
  }

  /// The inverse.
  void AimAtRock(std::uint16_t _rockIndex) noexcept
  {
    targetX = static_cast<std::int16_t>(_rockIndex);
    targetY = 0;
  }

  /// The module an `UpgradeModule` names, and the level it becomes in `targetY`'s spare high byte.
  void AimAtUpgrade(WireIdentity _module, DesignId _level) noexcept
  {
    AimAt(_module);
    targetY = static_cast<std::int16_t>(static_cast<std::uint16_t>(GenerationOf(_module) | (static_cast<std::uint16_t>(_level) << 8)));
  }

  /// The inverse's second half: the level an `UpgradeModule` asks for.
  [[nodiscard]] std::uint8_t UpgradeLevel() const noexcept
  {
    return static_cast<std::uint8_t>(static_cast<std::uint16_t>(targetY) >> 8);
  }

  /// The design a `Build` names. **The low byte only**, so a client that left rubbish in the high
  /// byte still names the design it meant rather than one the host has to reject.
  [[nodiscard]] std::uint8_t TargetDesign() const noexcept
  {
    return static_cast<std::uint8_t>(static_cast<std::uint16_t>(targetX) & 0xFF);
  }

  [[nodiscard]] friend bool operator==(const Command&, const Command&) noexcept = default;
};

/// A packet of them. ADR-003: commands are repeated in every outgoing packet until the update's
/// `lastCommandSeqApplied` reaches them, so a packet holds however many are outstanding -- and
/// `TechnicalDesign.md` section 4 bounds that structurally by filling OLDEST-FIRST and stopping
/// when the next one will not fit.
///
/// **IT ALSO CARRIES WHAT THE CLIENT IS LOOKING AT** (ADR-024): the view's center as two wire positions
/// and its radius in whole world units, which is what the host's accumulator scores relevance against.
/// A packet with no commands in it is therefore not empty -- it is how a client reports its view when
/// it has nothing to order, and the client sends one on a cadence for exactly that.
struct CommandPacket
{
  /// The transport's four, the player identity, the command count, and the view: center 4, radius 2.
  static constexpr std::size_t HEADER_BYTES = Neuron::PacketHeader::SIZE_BYTES + 2 + 6;

  static constexpr std::size_t MAX_COMMANDS = 255;

  std::uint16_t sequence = 0;
  PlayerId player = NO_PLAYER;

  /// Where the client's camera is looking, on the wire's own grid, and how far around it is on screen.
  /// **Zero radius means no view**: a client that has not reported one is scored as looking at nothing,
  /// which is what a client at the join is.
  std::int16_t viewX = 0;
  std::int16_t viewY = 0;
  std::uint16_t viewRadiusUnits = 0;

  std::vector<Command> commands;
};

enum class CommandFault : std::uint8_t
{
  None,
  Truncated,
  VersionMismatch,
  /// A well-formed packet that is not a command packet.
  WrongType,
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
/// reaches a deep retransmit window is not a fast player -- touch cannot issue three orders in
/// 150 ms -- it is a stalled acknowledgment, which is exactly the load under which a fragmented
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
