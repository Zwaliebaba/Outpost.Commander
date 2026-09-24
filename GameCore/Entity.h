#pragma once

#include "NeuronCore.h"

#include <compare>
#include <cstdint>

// M1.1: the hull identity is the catalog's. M1.3: so is the design.
#include "Catalog.h"
#include "Design.h"

namespace Outpost
{

/// An entity's name, and ADR-002 fixes its shape: an index into the store and a generation that
/// counts how many times that index has been handed out. The pair is what makes a stale reference
/// detectable -- an index alone would silently resolve to whatever took the slot next, which is
/// the bug this format exists to make impossible.
///
/// GENERATION ZERO IS NOT A LIVE ENTITY. A default-constructed EntityId is therefore invalid
/// rather than pointing at slot zero, and the first allocation of any slot is generation one.
/// That is worth the one wasted value: the alternative is a zeroed struct that resolves.
///
/// R8: a public aggregate, so plain fields and brace initialization.
struct EntityId
{
  std::uint16_t index = 0;
  std::uint16_t generation = 0;

  /// A TOTAL ORDER, AND IT IS LOAD-BEARING RATHER THAN CONVENIENCE. ADR-002 requires every query
  /// whose result reaches an outcome to sort its candidates by entity identity, and every tie --
  /// two targets at equal distance -- to break on identity rather than on which grid cell was
  /// visited first. That rule needs an ordering to exist before there is anything to sort, and
  /// this is it. Index first, generation second, so the order is the store's own.
  [[nodiscard]] friend constexpr auto operator<=>(const EntityId&, const EntityId&) noexcept = default;
  [[nodiscard]] friend constexpr bool operator==(const EntityId&, const EntityId&) noexcept = default;

  [[nodiscard]] constexpr bool IsValid() const noexcept
  {
    return generation != 0;
  }
};

/// The identity no entity has.
inline constexpr EntityId NO_ENTITY{};

/// Which player an entity belongs to. Zero is nobody -- a neutral thing, or a fixture in a test --
/// and players are numbered from one, so a default-constructed Entity is owned by no one rather
/// than by player zero.
///
/// It rides the wire as its own byte on every record (`GameCore/EntityRecord.h`, ADR-024) -- two
/// team bits in the flags until then, which could name four players and no more. It is on the
/// replicated record rather than beside it because the client draws ownership, and the host validates
/// commands against it (ADR-003, Q24).
using PlayerId = std::uint8_t;

inline constexpr PlayerId NO_PLAYER = 0;

/// **THE GAME'S NUMBER: A MATCH IS FOUR SLOTS** (`GameDesign.md` section 2). A host seats more only in a
/// stress configuration (ADR-023), and nothing about a real match is designed past this.
inline constexpr std::size_t MATCH_PLAYERS = 4;

/// **THE MOST A HOST CAN SEAT, WHICH IS A CAPACITY AND NOT A DESIGN.** A `PlayerId` is one byte and zero
/// is nobody, so 254 is its own ceiling -- and the wire carries any of them, since ADR-024 put the owner on
/// every record. It was four until ADR-023, when it was the design's slot count doing both jobs.
///
/// **THE PER-PLAYER STATE IS SIZED TO THIS, NOT TO THE MATCH.** The command intake and the build system
/// keep an array entry per possible player -- a few kilobytes at 255 entries -- rather than a vector sized
/// when the match begins. That is ADR-023 amended: an array needs no count threaded into a constructor, no
/// "was `Begin` called" to assert, and cannot be indexed past by any `PlayerId` there is.
inline constexpr std::size_t MAX_PLAYERS = 254;

// HullId IS THE CATALOG'S NOW (M1.1). This was `using HullId = std::uint8_t` with a note saying
// "at M0 there is no catalog to index into and this is a number that rides along" -- the catalog
// exists, so the placeholder is gone and `Catalog.h` owns the identity. Nothing about the width
// changed: the enumeration is `std::uint8_t`-backed, so the state hash folds the same byte it
// always did (R16, ADR-002).

/// THE REPLICATED RECORD, AND ONLY THAT. Everything here is state the client is sent and draws
/// from; nothing here is a host-only decision. An order's destination, the speed a drive works out
/// to, a target -- none of those belong in this struct, because R19 has the client linking
/// `GameCore` and not the simulation, and a host-only field on a shared record is an invitation to
/// read it on the side that never has it. `GameLogic`'s World carries that state alongside.
///
/// ADR-002's state hash covers every field but the design, which the hull is derived from: identity,
/// position, heading, hull, hull points and owner (hull points and owner since the 2026-09-23 review).
struct Entity
{
  EntityId id{};
  Neuron::Vec2 position{};
  Neuron::Angle heading = 0;

  /// **HASHED**, as ADR-002 first named it.
  HullId hull = HullId::Scout;

  /// What this entity was built as (R24, ADR-006). **The hull above is DERIVED from it and stored
  /// beside it**, which is the one piece of redundant state in this struct and is deliberate:
  /// ADR-002's hash covers the hull and the wire carries the DESIGN, so both are read on a hot
  /// path and neither should be a table lookup away. `World::Create` sets them together from the
  /// design and a test asserts they cannot disagree -- nothing else may write either.
  DesignId design = DesignId::Miner;

  /// Hull points remaining, in the simulation's own units rather than the wire's percentage.
  /// **HASHED** since the 2026-09-23 review (M6): it is what M3's damage writes.
  /// `GameDesign.md` section 7 does damage in points; the percentage is a rendering quantity and
  /// the encoder is where it becomes one.
  std::uint16_t hullRemaining = 0;

  /// **HASHED** since the 2026-09-23 review (M6). It is set once at creation and never moves, so folding it
  /// costs a byte and catches a creation that went to the wrong player.
  PlayerId owner = NO_PLAYER;
};

} // namespace Outpost
