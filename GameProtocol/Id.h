#pragma once

namespace Outpost
{
// A typed identifier: a number that names one thing of one kind. The tag makes each kind a distinct type, so an entity's
// identifier cannot be passed where a player's is expected. Plain data, so it crosses a transport as the number alone
// (ADR-002). Zero is no identifier.
template <typename Tag> struct Id
{
  std::uint32_t value = 0;

  [[nodiscard]] constexpr bool IsValid() const noexcept
  {
    return value != 0;
  }

  friend constexpr auto operator<=>(const Id&, const Id&) = default;
};

// A ship, a structure or an asteroid, for the whole match. The server assigns them and never reuses one.
using EntityId = Id<struct EntityTag>;
using PlayerId = Id<struct PlayerTag>;

// A saved ship design (design §7). The server assigns them when a design is first saved.
using DesignId = Id<struct DesignTag>;

// Components and research topics, as the tuning data numbers them (implementation plan, task 3.1).
using HullId = Id<struct HullTag>;
using DriveId = Id<struct DriveTag>;
using WeaponId = Id<struct WeaponTag>;
using ResearchTopicId = Id<struct ResearchTopicTag>;
} // namespace Outpost
