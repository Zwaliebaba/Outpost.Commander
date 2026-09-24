#pragma once

#include "BuildSystem.h"
#include "ModuleEffects.h"
#include "RingAssignment.h"
#include "World.h"

#include <array>
#include <cstdint>

namespace Outpost
{

/// Why a command was not applied. Distinct values because a test names the case it is pinning,
/// and because a counter that says "rejected" tells an operator nothing about which of Q24's
/// checks is firing. **Every value but `AlreadyApplied`, `UnknownType` and `Empty` is acknowledged**:
/// the host understood the order and refused it, and a sequence that did not advance would have the
/// client resend it forever.
enum class CommandRejection : std::uint8_t
{
  None,
  /// At or below what has already been applied. THE ORDINARY CASE, not a fault: ADR-003 repeats
  /// every command in every packet until an update acknowledges it, so most arrivals of most
  /// commands are this.
  AlreadyApplied,
  /// A type this build does not know.
  UnknownType,
  /// The selection names more identities than the sender owns entities.
  SelectionTooLong,
  /// An identity the sender does not own. **Refuses the whole order**, where a dead or reused one is
  /// skipped (Q24 as amended after the 2026-09-23 review).
  NotOwned,
  /// No player, or a selection that is empty when the type needs one -- or carries identities when
  /// the type does not (`GameCore/Command.h`).
  Empty,
  /// A `Mine` naming a rock the field does not have (Q52).
  NoSuchRock,
  /// An `Attack` naming nothing alive, nobody's, or the sender's own (M3.2, Q67). **Own is refused rather than
  /// followed**: an attack on your own ship would otherwise be a free follow verb nobody designed.
  NoSuchTarget,
  /// A `Build`, `CancelBuild`, `PlaceModule` or `UpgradeModule` the build system refused. **The reason is `BuildSystem`'s and stays
  /// there**: duplicating `BuildRejection` into this enumeration would be two lists to keep in step
  /// for a distinction only the build suite ever asserts.
  BuildRefused
};

/// The host's intake: apply in sequence order, ignore anything at or below what has been applied,
/// and validate before applying.
///
/// VALIDATION IS CORRECTNESS, NOT SECURITY (Q24), and the distinction is not pedantry. Section 5
/// declines authentication and any defense against a hostile client, and that exclusion silently
/// covered ownership and bounds checks too -- which are a different category. A 1,232-byte command
/// packet holds 404 identities against a peak of 110, a 3.7x amplification into a single-threaded
/// loop, and it is reachable from an ordinary bug or a reordered packet with no attacker anywhere.
///
/// AT M0 THERE IS ONE ENTITY AND THE OWNERSHIP CHECK HAS NOTHING TO REJECT. It is written anyway:
/// thirty lines now against a retrofit into a loop that has since grown.
class CommandIntake
{
public:
  /// The largest player number this holds state for: every `PlayerId` there is (`GameCore/Entity.h`), so
  /// the player count stays a runtime value at any size a stress run asks for (ADR-023).
  static constexpr std::size_t MAX_PLAYERS = Outpost::MAX_PLAYERS;

  /// Validates and applies one command on behalf of _player. The world is touched only when the
  /// answer is CommandRejection::None.
  ///
  /// **IT TAKES THE BUILD SYSTEM BECAUSE M1.6 MADE A BUILD AN ORDER.** `GameDesign.md` section 5
  /// has a design selected at the station, and an order is how a client says so -- which puts the
  /// affordability check here, on the host, rather than at the client alone.
  [[nodiscard]] CommandRejection Apply(World& _world, BuildSystem& _build, PlayerId _player, const Command& _command) noexcept;

  /// Every command in the packet, in the order it arrived. The packet's own player is used, not
  /// a caller's -- a packet says who sent it.
  [[nodiscard]] std::size_t ApplyPacket(World& _world, BuildSystem& _build, const CommandPacket& _packet) noexcept;

  /// What goes in the update's own block as `lastCommandSeqApplied`, which is the whole
  /// acknowledgment channel (ADR-003). Zero for a player who has sent nothing.
  [[nodiscard]] std::uint16_t LastAppliedSequence(PlayerId _player) const noexcept;

private:
  [[nodiscard]] static bool IsNewer(std::uint16_t _sequence, std::uint16_t _lastApplied) noexcept;

  /// Index 0 is NO_PLAYER and is never used; players are numbered from one.
  std::array<std::uint16_t, MAX_PLAYERS + 1> m_lastApplied{};
  std::array<bool, MAX_PLAYERS + 1> m_hasApplied{};
};

/// A packed wire identity to the store's own, or NO_ENTITY when it does not resolve. THE
/// GENERATION IS CHECKED AGAINST ITS LOW EIGHT BITS, which is all the wire carries -- see
/// `GameCore/EntityRecord.h` for why six is enough and what would have to happen to alias.
[[nodiscard]] EntityId ResolveWireIdentity(const World& _world, WireIdentity _wireIdentity) noexcept;

} // namespace Outpost
