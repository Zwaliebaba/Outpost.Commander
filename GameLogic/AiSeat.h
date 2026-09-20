#pragma once

#include "AiBlackboard.h"
#include "AiDesigner.h"

#include <cstdint>

// The scripted commander of the vertical slice (TechnicalDesign.md §7; m1-vertical-slice/S12).
// Stage 11 of the tick. It is the M2 planner's skeleton rather than a throwaway: the observation,
// the blackboard and the behaviour loop are what M2 keeps, and what M2 replaces is the hard-coded
// list below with a table of personalities and weights.
//
// ── THE BEHAVIOUR LIST, FIXED FOR M1 ────────────────────────────────────────────────────────
//
// Behaviours 1 to 6 are evaluated in order and the FIRST one with something to do emits its orders;
// the rest wait for the next decision. That is what keeps a scripted commander from spending its
// whole stockpile on the first tick it can afford five things. ATTACK is outside that queue and is
// evaluated every decision, because idle fighters are not a resource the base is competing for -
// behind the queue it never ran at all, since a factory with something to produce answers first
// every decision for the whole match.
//
//  0. ABANDON     A plan whose ground will no longer take it - CheckPlacement refuses the footprint
//                 it was placed on, because a neighbouring plan began construction and marked those
//                 cells: cancel it. First, because the blackboard counts what a commander holds
//                 "built or building", so an uncancelled dead plan counts as the thing it was going
//                 to be and every behaviour below reasons about a count this one corrects.
//  1. DESIGN      No builder design, or no fighter design, or the enemy composition it has seen has
//                 changed since it last designed: AiDesigner picks the best of each by expected
//                 damage per power against what it has actually seen, and a SaveDesign order saves
//                 it. A design is saved before anything is produced, because production names one.
//  2. FINISH      A site placed and not finished: walk an idle builder to a cell BESIDE it, because
//                 a plan nobody is standing next to never goes up - and because a structure occupies
//                 its cells from the tick construction begins, so a builder sent onto the site itself
//                 is a builder the planner refuses, stops, and is sent there again on the next
//                 decision for the rest of the match. EVERY unfinished site is tried and not just
//                 the oldest: one nobody can be got to is passed over, which is what lets the slack
//                 below actually be used. Nothing new is placed while there are already more
//                 unfinished sites than builders to work them, with a slack of one: a site no
//                 builder can reach - a deposit on a summit no drive can climb - would otherwise
//                 stop the commander placing anything else for the rest of the match, and one spare
//                 slot lets the base go up around it.
//
//  2b. REBUILD    No command post: place one. It is first because everything else needs one, and
//                 because GameDesign.md §5 lets a commander with none build exactly this.
//  3. SERVE       At least one extractor and no generator, or an extractor nobody serves: place a
//                 generator. BEFORE the next extractor, and that order is the whole of
//                 GameDesign.md §4 - an extractor earns nothing until a generator reaches it, so a
//                 commander that claimed four deposits first would have spent four extractors'
//                 worth of power on an income of nothing.
//
//  4. EXPAND      Fewer extractors than free deposits within reach: place one on the nearest. A
//                 deposit it has never SEEN cannot be built on - CheckPlacement refuses an
//                 unexplored footprint, which is GameDesign.md §5's rule - so a builder that is
//                 standing still walks to it first and the extractor follows on a later decision.
//                 A generator goes up when an extractor of its own is unserved; forty-eight cells
//                 is the service range (GameDesign.md §4), so it goes near the base rather than
//                 beside each extractor.
//  5. INDUSTRY    No factory: place one. Then no lab: place one. Research is the seat's own
//                 autoResearch flag (m1-vertical-slice/S6), so there is no research behaviour here
//                 and an AI seat MUST BE SET UP WITH IT ON. That is a requirement on whoever fills
//                 the lobby and not a preference: without it a scripted commander never leaves the
//                 machine gun, which is the only weapon unlocked from the first tick, and a machine
//                 gun deals EXACTLY NOTHING to a command post - eight damage taken to two by the
//                 anti-light matrix's thirty percent against Hard, then all of it by the post's
//                 twenty kinetic armour, with a floor of a third of two that is nought in whole
//                 points. It can sweep the field and never finish the match.
//                 Measured on the slice landscape at seed 1 (m1-vertical-slice/S15): with the flag
//                 off, fifty simulated minutes, the enemy army destroyed, the enemy command post at
//                 full health and 153 machine guns standing round it. With it on, the same match is
//                 decided in 12,874 ticks. OutpostCommander/App.cpp's lobby had it off, and every
//                 unit suite passed throughout because Tests/SimTests/AiTests.cpp's fixture sets it.
//  6. PRODUCE     An idle factory and fewer devices than the composition wants: queue one. The
//                 composition is two builders and then fighters to the device cap.
//  7. ATTACK      More idle fighters than ATTACK_GROUP_SIZE: attack-move the whole group to the
//                 nearest known enemy structure, which the blackboard remembers from its ghost
//                 store after it has gone out of sight - and, when it has seen nothing yet, to the
//                 START POSITION furthest from its own base. The landscape's definition is public
//                 to every commander from the first tick (§5.2), so that is what a human knows too,
//                 and without it two commanders on opposite corners never meet.
//
// ── WHAT MAKES IT DETERMINISTIC ─────────────────────────────────────────────────────────────
//
// No wall time, no float and no thread (AGENTS.md R16): every decision is a function of the world,
// the seat and the tick. Every list it walks is in ascending object id, every tie breaks on a row
// or a cell, and the orders go through the ORDINARY queue for tick t plus a delay - the same door a
// client's orders come through, so an AI seat is indistinguishable from a human one below stage 1.

namespace Outpost
{

class Sim;

/// How often a scripted commander decides. Ten ticks is twice a second, which is faster than a
/// human clicks and slow enough that eight of them are a fraction of the tick budget. Seats are
/// staggered by index so that two never decide on the same tick.
inline constexpr std::uint32_t AI_DECISION_INTERVAL_TICKS = 10;

/// The delay §7 asks for: an AI's orders are submitted for a later tick, exactly as a client's
/// arrive for one. Two ticks is a tenth of a second, and it stops a scripted commander reacting
/// inside the tick it saw something.
inline constexpr std::uint32_t AI_ORDER_DELAY_TICKS = 2;

/// The army it produces to, before the device cap: two builders, and fighters after that.
inline constexpr std::uint32_t AI_WANTED_BUILDERS = 2;

/// How many idle fighters it gathers before it attacks, and how many go in one group. A group
/// rather than everything standing still, because the planner's budget is 2,000 nodes a tick
/// shared across every request: thirty cross-map searches at once each get seventy nodes and none
/// of them finishes, so an army ordered all at once stands where it was built.
inline constexpr std::uint32_t ATTACK_GROUP_SIZE = 4;

/// How far short of what it is attacking a group is sent. The cell a commander starts on is where
/// his command post stands and a structure's cells are impassable, so a route to the middle of a
/// base comes back unreachable and the group stands where it was built; short of it is also where
/// a group meets what is defending the base.
///
/// FOUR IS NOT FAR ENOUGH ON ITS OWN, and that was the whole of m1-vertical-slice/S14's first half.
/// A base GROWS: by the time a commander has four idle fighters the enemy has a dozen buildings
/// round his start, and four cells back is still inside them. Measured on the slice landscape at
/// seed 1, the group was sent to (38,90), which is impassable, on tick 5,623; the route came back
/// unreachable one tick later, the seat re-issued it every ten ticks for the rest of the match, and
/// the two commanders fired no shot in twenty thousand ticks. This number is now where the search
/// STARTS rather than where the group is sent - ATTACK_APPROACH_RINGS says how far it looks.
inline constexpr std::uint32_t APPROACH_CELLS = 4;

/// How far out from that cell a group will look for ground it can actually stand on. Sixteen is
/// wider than any base M1 can build - six structures at three cells each, packed - so a group that
/// finds nothing inside it is looking at something other than a base, and standing still is then
/// the honest answer rather than a march to a cell chosen at random.
inline constexpr std::uint32_t ATTACK_APPROACH_RINGS = 16;

/// How near a deposit a builder is walked when it goes to look at one. Scouting is about the fog
/// and not about building: CheckPlacement refuses an unexplored footprint, and a device sees 16 to
/// 20 cells on the chassis the tables ship, so two is far inside the sight of whatever is doing the
/// looking and leaves the deposit's own cell - which may be a summit no drive can hold - free.
inline constexpr std::uint32_t SCOUT_APPROACH_CELLS = 2;

/// How far from its base a scripted commander looks for somewhere to put a structure. Twenty-four
/// rings is a fifth of a Small landscape: a base on rough ground needs it, and one on flat ground
/// finds a spot in the first two.
inline constexpr std::uint32_t AI_BASE_SEARCH_RINGS = 24;

/// How many extractors it will run out to claim. Beyond this the walk is longer than the deposit
/// is worth on a Small landscape, and expanding further is M2's decision to take with a threat map.
inline constexpr std::uint32_t AI_WANTED_EXTRACTORS = 4;

/// Stage 11: every scripted seat observes and decides, within its budget.
void AdvanceAiSeats(Sim& _sim);

/// One seat's decision, exposed so that a test can make it happen on a tick of its choosing rather
/// than waiting for the interval to come round.
void DecideForSeat(Sim& _sim, std::uint8_t _seat);

/// The drive class a device's design walks on, or Wheels when the design or the row is missing -
/// which is what the cluster graph is asked about before every order this file sends.
///
/// EXPOSED SO THAT A TEST ASKS THE SAME QUESTION THE SEAT DID (m1-vertical-slice/S14). The rule
/// that an attack group is never sent to ground it cannot stand on is only worth asserting against
/// the class the AI itself used; a test that worked the class out for itself would pass a seat
/// that had looked up the wrong one.
[[nodiscard]] DriveClass DriveOf(const Sim& _sim, std::uint8_t _seat, const Device& _device) noexcept;

} // namespace Outpost
