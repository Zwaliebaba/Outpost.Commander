#pragma once

#include "Tick.h"
#include "World.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// `GameDesign.md` section 5's station, building. **A design is selected, credits are deducted WHEN THE
/// ITEM STARTS, and the ship appears at the station's spawn point when it finishes.**
///
/// **THERE IS NO QUEUE** (Q21). `Interface.md` had specified a cancellable queue that no wire record
/// could feed, and the MVP cut the queue rather than inventing a format for it -- so the snapshot
/// carries the item currently building and its progress, two bytes per player, and this class holds
/// exactly that much state. **There is no rally point either**: a new ship sits where it appears.
///
/// R16 reaches this file. Integers throughout, no clock, and the tick is the only thing that advances
/// anything.

/// `GameDesign.md` section 4: "a station begins with 1,000 credits".
inline constexpr std::uint32_t STARTING_CREDITS = 1000;

/// Why a build order was refused. Distinct values for the reason `CommandRejection` has them: a counter
/// that says "refused" tells an operator nothing, and a test names the case it is pinning.
enum class BuildRejection : std::uint8_t
{
  None,
  /// `NO_PLAYER`, or a player this match does not have.
  NoPlayer,
  /// A design identity this build does not know.
  UnknownDesign,
  /// A design no player can order -- the station, and from M2 the modules, which are placed by tap
  /// rather than queued (`GameCore/Design.h`).
  NotBuildable,
  /// **REFUSED AT THE HOST AND NOT AT THE CLIENT ALONE**, which is M1.6's exit criterion: R19 makes
  /// the host authoritative, and a client that checked affordability and a host that did not would
  /// disagree the first time two orders raced.
  Unaffordable,
  /// The player has no station to build at. **M3's elimination supersedes this**; until then it is
  /// only reachable by a test that kills one.
  NoStation,
  /// **A PLACEMENT `CheckModuleSite` REFUSES** (M2.11): past the cap, outside the radius, or overlapping the
  /// station or a module. Refused before the current item is touched, so **a refused placement spends
  /// nothing and cancels nothing** -- which is the step's exit criterion.
  IllegalSite,
  /// A placement naming a level that only comes about by upgrade (Q54), or an upgrade naming a module this
  /// player does not own, one that is gone, or a level it does not upgrade to.
  NotUpgradeable
};

/// What one player is building. Two bytes on the wire (ADR-003's per-player block) and four fields here,
/// because the wire carries a percentage and the simulation counts ticks (R16).
///
/// R8: a public aggregate.
struct BuildItem
{
  bool active = false;
  DesignId design = DesignId::Miner;

  /// **MONOTONIC, AND IT REACHES `ticksRequired` EXACTLY.** One per tick, no rounding, no clock. The
  /// percentage the wire carries is derived from these two and is never the thing that advances.
  std::uint32_t ticksElapsed = 0;
  std::uint32_t ticksRequired = 0;

  /// What was deducted when it started, kept so a cancel can give back **exactly** what was taken
  /// (Q35). Recomputing the cost at cancel time would be right today and wrong the day a catalog
  /// change lands mid-match.
  std::uint32_t creditsSpent = 0;

  /// **WHERE A MODULE GOES** (M2.11): the point the placement named, and where the module appears when the
  /// item finishes. Unused for a ship, which appears at the spawn point.
  Neuron::Vec2 site{};

  /// **THE MODULE AN UPGRADE BECOMES** (Q54), or `NO_ENTITY` for anything else. The item's design is then
  /// the level it becomes, which is what the wire's design byte shows while it builds.
  EntityId upgrade{};

  [[nodiscard]] friend constexpr bool operator==(const BuildItem&, const BuildItem&) noexcept = default;
};

class BuildSystem
{
public:
  /// `GameCore/Entity.h`'s, which `CommandIntake` also uses -- one statement of the capacity rather
  /// than two that have to agree.
  static constexpr std::size_t MAX_PLAYERS = Outpost::MAX_PLAYERS;

  /// **HOW FAST A STATION BUILDS, IN CREDITS OF COST A SECOND** -- and `OpenQuestions.md` Q47 is the
  /// row this is, because the design states no build time anywhere. `GameCore/DerivedStats.h` said so
  /// in as many words: ADR-006 names build time alongside cost and no figure for it exists, and M1.6
  /// is the step that first observes one.
  ///
  /// **TWENTY, AND THE DERIVATION IS THE OPENING.** A station starts with 1,000 credits
  /// (`GameDesign.md` section 4) and a running economy is about six miners at 150 each. At twenty a
  /// second that opening bank takes fifty seconds to spend, against a match of five minutes -- so the
  /// first minute is spending what you started with and after that income paces you, which is the
  /// shape the economy section describes. At ten it would be a hundred seconds with credits piling up
  /// unspent, which is a third of the match spent waiting.
  ///
  /// **IT SITS JUST ABOVE THE INCOME RATE ON PURPOSE.** Six miners on the six nearest home rocks earn
  /// 19.8 credits a second (measured, `OpenQuestions.md` Q62), so building is very slightly faster than earning -- which is what leaves the shipyard multiplier
  /// something to do (`GameDesign.md` section 5). Set it far above and credits always bind and the
  /// multiplier is dead; far below and the station is the bottleneck and mining stops mattering.
  static constexpr std::uint32_t BUILD_RATE_CREDITS_PER_SECOND = 20;

  /// `Tick.h`'s, named through this class because the build rate is stated in seconds and every use
  /// of it here is in ticks.
  static constexpr std::uint32_t TICKS_PER_SECOND = Outpost::TICKS_PER_SECOND;

  /// How long a design takes, in ticks, at a given build-rate multiplier in hundredths.
  ///
  /// **A HUNDRED IS NO SHIPYARD.** `GameDesign.md` section 5 has `ShipyardL1` at 150 and `ShipyardL2`
  /// at 200; since M2.12 the intake passes the player's own (`ModuleEffects.h`). **The ticks round up**
  /// (`OpenQuestions.md` Q56), so a shipyard never builds faster than its stated rate.
  ///
  /// **AT LEAST ONE TICK.** A design that costs nothing would otherwise complete before it started,
  /// and the catalog contains rows with no cost.
  [[nodiscard]] static std::uint32_t TicksToBuild(DesignId _design, std::uint32_t _multiplierPercent = 100) noexcept;

  /// The same, for a cost rather than a design -- which an upgrade needs, because what it builds is the
  /// difference between two designs (Q54).
  [[nodiscard]] static std::uint32_t TicksForCost(std::uint32_t _costCredits, std::uint32_t _multiplierPercent = 100) noexcept;

  /// Starts a match: every player on `STARTING_CREDITS` and building nothing.
  void Begin(std::size_t _playerCount) noexcept;

  /// Orders a design. Deducts its cost **now**, when it is queued (`GameDesign.md` section 5, Q80).
  ///
  /// **AN ORDER WHILE SOMETHING IS BUILDING JOINS THE QUEUE BEHIND IT** (`OpenQuestions.md` Q80, the owner's
  /// ruling of 2026-09-24). Until then it replaced the item in progress at a full refund (Q35). **Money is the
  /// only limit**: an order the balance cannot cover changes nothing, and the queue has no other cap.
  [[nodiscard]] BuildRejection Start(World& _world, PlayerId _player, DesignId _design,
                                     std::uint32_t _buildRateMultiplierPercent = 100) noexcept;

  /// **A MODULE, AT A POINT** (M2.11). The site is checked with `CheckModuleSite` against this player's station
  /// and modules **before anything else is touched**, so a refused placement leaves the credits and whatever
  /// is building exactly as they were. Past that it is `Start`: the same queue slot, the same refund-first
  /// replacement, the cost deducted now. The module appears at the site when the item finishes.
  ///
  /// **ONLY A PLACED LEVEL** (Q54): an L2 comes about by `StartUpgrade` and is refused here.
  [[nodiscard]] BuildRejection StartModule(World& _world, PlayerId _player, DesignId _design, const Neuron::Vec2& _site,
                                           std::uint32_t _buildRateMultiplierPercent = 100) noexcept;

  /// **AN UPGRADE IN PLACE** (Q54): one of this player's modules becomes _level when the item finishes, at the
  /// difference in cost and the build time of that difference. The module is checked before anything is
  /// touched, as a placement's site is.
  [[nodiscard]] BuildRejection StartUpgrade(World& _world, PlayerId _player, EntityId _module, DesignId _level,
                                            std::uint32_t _buildRateMultiplierPercent = 100) noexcept;

  /// Cancels **the newest item first** (Q80): the last one queued, or with nothing queued the item in progress,
  /// refunding **in full** (Q35). False when nothing was building, which is not an error -- a tap on a cancel
  /// target that has already completed is ordinary.
  bool Cancel(PlayerId _player) noexcept;

  /// One tick of every player's item, and the ship that finishes. **Called after movement**, so a ship
  /// that appears this tick does not also move on it.
  void Advance(World& _world) noexcept;

  [[nodiscard]] std::uint32_t Credits(PlayerId _player) const noexcept;

  /// **M2's INCOME, AND ITS ONE WAY IN.** `Economy::Credit` calls it with what a tick's unloads earned
  /// (M2.7), so the credit balance has exactly one owner rather than two places that both add to it.
  void Grant(PlayerId _player, std::uint32_t _credits) noexcept;

  [[nodiscard]] const BuildItem& Item(PlayerId _player) const noexcept;

  /// What waits behind the item in progress, oldest first (Q80). Empty when the item is the last.
  [[nodiscard]] std::span<const BuildItem> Queued(PlayerId _player) const noexcept;

  /// ADR-003's per-player block, both bytes. **The design byte carries the queue's length in its high four bits
  /// since Q80** (`GameCore/Update.h`, `PackBuilding`).
  ///
  /// **ZERO MEANS NOTHING IS BUILDING, SO A DESIGN IS ITS IDENTITY PLUS ONE** -- and that is the one
  /// place the wire's design byte is not `EntityRecord::designIdentity`'s raw value. The asymmetry is
  /// deliberate: an entity always has a design and a player usually has nothing building, so the
  /// sentinel is worth a byte here and would be a wasted value there.
  [[nodiscard]] std::uint8_t WireBuildingDesign(PlayerId _player) const noexcept;

  /// **0 TO 99 WHILE BUILDING, AND NEVER 100.** The item is gone on the tick it completes, so the
  /// client's last sight of it is whatever the tick before showed; a hundred would mean "finished and
  /// still here", which is not a state this system has.
  [[nodiscard]] std::uint8_t WireProgressPercent(PlayerId _player) const noexcept;

  /// How many players this match seats, which is how far `MatchHash` walks.
  [[nodiscard]] std::size_t PlayerCount() const noexcept
  {
    return m_playerCount;
  }

  /// Items that completed and had nowhere to go, refunded. See `BuildRejection::NoStation`.
  [[nodiscard]] std::uint64_t StrandedCount() const noexcept
  {
    return m_stranded;
  }

private:
  [[nodiscard]] bool Holds(PlayerId _player) const noexcept;

  /// The half of every start that is the same: if the balance covers _item's cost, charge it and hold _item --
  /// as the item in progress when nothing is building, and at the back of the queue otherwise (Q80).
  /// **Otherwise touch nothing** (the 2026-09-23 review, m1). Only called once everything about the order has
  /// been checked.
  [[nodiscard]] BuildRejection Commit(PlayerId _player, const BuildItem& _item) noexcept;

  /// Where a finished ship appears: **in front of the station, clear of both hulls.** The offset is
  /// half the station's size plus half the ship's (Q37's catalog figures), so the two never overlap
  /// and nothing invents a distance. The station faces the center of the map
  /// (`GameCore/Layout.h`), so ships appear on the side a player is looking toward.
  [[nodiscard]] static Neuron::Vec2 SpawnPoint(const Entity& _station, DesignId _design) noexcept;

  /// Index 0 is `NO_PLAYER` and is never used; players are numbered from one, as `CommandIntake` does.
  std::array<std::uint32_t, MAX_PLAYERS + 1> m_credits{};
  std::array<BuildItem, MAX_PLAYERS + 1> m_items{};

  /// Q80: what waits behind each player's item, oldest first. Paid for already.
  std::array<std::vector<BuildItem>, MAX_PLAYERS + 1> m_queued{};

  std::size_t m_playerCount = 0;
  std::uint64_t m_stranded = 0;
};

} // namespace Outpost
