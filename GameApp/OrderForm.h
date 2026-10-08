#pragma once

namespace Outpost
{
// A field of the orders window's form, which its arrows step (Phase 5 design §7).
enum class OrderField : std::uint8_t
{
  Trigger,
  Hour,
  Minute,
  TriggerSector,
  Action,
  ActionSector,
  Target,
  Condition
};

// The orders window's form (Phase 5 design §7, §11, ADR-080): the scheduled order it gives the selection, a field at a time.
// Its trigger is a time of day on the player's clock, or an event in a sector the player holds; its action goes to a
// sector's node, or takes a target in that sector, an enemy for an attack and an ore asteroid for a Mining Rig; and its
// condition is the most command points of enemy warships it goes ahead against, or none. It is client state, kept while
// the window is closed, as the designer's picks are.
class OrderForm
{
public:
  // A time of day's minutes step by this many; the condition by this many command points, up to the most.
  static constexpr int MINUTE_STEP = 5;
  static constexpr std::int32_t CONDITION_STEP = 2;
  static constexpr std::int32_t MOST_COMMAND_POINTS = 60;

  // Steps _field to its next value, or its previous for a negative _step, round the end: a sector among those _newest
  // offers, and a target among those the player knows of, _entities, in the action's sector. The condition steps from none
  // up to MOST_COMMAND_POINTS and back.
  void Step(OrderField _field, int _step, const Snapshot& _newest, std::span<const EntityView> _entities);
  // Keeps the sectors and the target among those offered: one that is gone gives way to the first.
  void Update(const Snapshot& _newest, std::span<const EntityView> _entities);

  [[nodiscard]] ScheduledTriggerKind Trigger() const noexcept
  {
    return m_trigger;
  }
  [[nodiscard]] int Hour() const noexcept
  {
    return m_hour;
  }
  [[nodiscard]] int Minute() const noexcept
  {
    return m_minute;
  }
  // The sector an event trigger watches, and the action's; zero for none.
  [[nodiscard]] std::int32_t TriggerSector() const noexcept
  {
    return m_triggerSector;
  }
  [[nodiscard]] ScheduledActionKind Action() const noexcept
  {
    return m_action;
  }
  [[nodiscard]] std::int32_t ActionSector() const noexcept
  {
    return m_actionSector;
  }
  [[nodiscard]] EntityId Target() const noexcept
  {
    return m_target;
  }
  [[nodiscard]] std::optional<std::int32_t> Condition() const noexcept
  {
    return m_condition;
  }

  // Whether the action takes a target, rather than a sector's node.
  [[nodiscard]] bool TakesTarget() const noexcept
  {
    return m_action == ScheduledActionKind::Attack || m_action == ScheduledActionKind::BuildRig;
  }

  // The sectors an event trigger may watch: those the player holds.
  [[nodiscard]] static std::vector<const SectorView*> WatchedSectors(const Snapshot& _newest);
  // The targets the action may take among _entities, in the action's sector, or anywhere on a map without sectors: for an
  // attack the enemy's structures and then its ships, and for a rig the ore asteroids with Ore left, each as they come.
  [[nodiscard]] std::vector<const EntityView*> Targets(const Snapshot& _newest, std::span<const EntityView> _entities) const;
  // Of _selected, the ships the order is for: Constructors for a rig, and warships for anything else.
  [[nodiscard]] std::vector<EntityId> ShipsFor(std::span<const EntityId> _selected, std::span<const EntityView> _entities) const;

  // Why the form cannot give its order to _selected yet, in capitals, as a button's note; empty when it can.
  [[nodiscard]] std::string Missing(std::span<const EntityId> _selected, const Snapshot& _newest,
                                    std::span<const EntityView> _entities) const;

  // The order for _selected, when the form can give it: a time of day as the next moment after _now that _clock reads it.
  [[nodiscard]] std::optional<ScheduleOrderCommand> Command(std::span<const EntityId> _selected, std::chrono::sys_seconds _now,
                                                            const PlayerClock& _clock, const Snapshot& _newest,
                                                            std::span<const EntityView> _entities) const;

  // A trigger's and an action's names as the form steps through them.
  [[nodiscard]] static std::string_view NameOf(ScheduledTriggerKind _trigger) noexcept;
  [[nodiscard]] static std::string_view NameOf(ScheduledActionKind _action) noexcept;
  // A target as the form names it: an enemy structure's kind or a ship's hull, the pirates' so marked, or an asteroid and
  // its Ore as far as the player knows it.
  [[nodiscard]] static std::string NameOf(const EntityView& _target, const Snapshot& _newest);

private:
  ScheduledTriggerKind m_trigger = ScheduledTriggerKind::TimeOfDay;
  // The design's own example, an attack at 02:00 (design §7).
  int m_hour = 2;
  int m_minute = 0;
  std::int32_t m_triggerSector = 0;
  ScheduledActionKind m_action = ScheduledActionKind::AttackMove;
  std::int32_t m_actionSector = 0;
  EntityId m_target;
  std::optional<std::int32_t> m_condition;
};

// A scheduled order in words, as the orders window lists it and a ship's panel names it: "at 02:00, attack-move to North,
// unless over 8 CP", its time of day read on _clock.
[[nodiscard]] std::string DescribeScheduledOrder(const ScheduledOrderView& _order, const Snapshot& _newest, const PlayerClock& _clock);

// The selection panel's line for the scheduled orders the ships of _selected wait on (design §11): the one, "Scheduled: at
// 02:00, attack-move to North", or how many; nothing when none waits.
[[nodiscard]] std::optional<std::string> PendingOrderLine(std::span<const EntityId> _selected, std::span<const EntityView> _entities,
                                                          const Snapshot& _newest, const PlayerClock& _clock);
} // namespace Outpost
