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

/// The design's slot count (`GameDesign.md` section 2: "a match is four slots"). **Here rather than on
/// one of the systems that needs it**, because two of them do -- the command intake and the build
/// system -- and two constants that must agree is a defect waiting for somebody to move one.
inline constexpr std::size_t MAX_PLAYERS = 4;

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
/// The first four fields are exactly what ADR-002's state hash covers: identity, position, heading
/// and hull.
struct Entity
{
  EntityId id{};
  Neuron::Vec2 position{};
  Neuron::Angle heading = 0;

  /// **HASHED.** ADR-002 names four fields and this is the fourth.
  HullId hull = HullId::Scout;

  /// What this entity was built as (R24, ADR-006). **The hull above is DERIVED from it and stored
  /// beside it**, which is the one piece of redundant state in this struct and is deliberate:
  /// ADR-002's hash covers the hull and the wire carries the DESIGN, so both are read on a hot
  /// path and neither should be a table lookup away. `World::Create` sets them together from the
  /// design and a test asserts they cannot disagree -- nothing else may write either.
  DesignId design = DesignId::Miner;

  /// Hull points remaining, in the simulation's own units rather than the wire's percentage.
  /// `GameDesign.md` section 7 does damage in points; the percentage is a rendering quantity and
  /// the encoder is where it becomes one.
  std::uint16_t hullRemaining = 0;

  /// NOT hashed. ADR-002 names four fields and this is not one of them, which is correct: a state
  /// hash detects two hosts drifting apart, and ownership is set once at creation and never moves.
  PlayerId owner = NO_PLAYER;
};

} // namespace Outpost
