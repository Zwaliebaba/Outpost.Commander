#pragma once

#include "NeuronCore.h"

#include <compare>
#include <cstdint>

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

/// Which hull an entity is built on. R24 has a built thing as a composition -- a hull, an optional
/// drive, and a component per slot -- and nothing in the simulation knows a ship type by name. At
/// M0 there is no catalog to index into and this is a number that rides along so the state hash
/// has a fourth field to cover; M0.9 settles the width it goes on the wire as.
using HullId = std::uint8_t;

/// THE REPLICATED RECORD, AND ONLY THAT. Everything here is state the client is sent and draws
/// from; nothing here is a host-only decision. An order's destination, the speed a drive works out
/// to, a target -- none of those belong in this struct, because R19 has the client linking
/// `GameCore` and not the simulation, and a host-only field on a shared record is an invitation to
/// read it on the side that never has it. `GameLogic`'s World carries that state alongside.
///
/// The fields are exactly what ADR-002's state hash covers: identity, position, heading and hull.
struct Entity
{
  EntityId id{};
  Neuron::Vec2 position{};
  Neuron::Angle heading = 0;
  HullId hull = 0;
};

} // namespace Outpost
