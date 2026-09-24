#pragma once

#include "HitTest.h"
#include "TapOrder.h"

#include "GameCore.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// What is selected, and what `Interface.md` section 4's table makes of a tap now that there are
/// teams and designs to make it of.
///
/// **M0.21 RESOLVED THE TIER AND STOPPED**, because M0 had no teams, no ore and no build panel. This
/// is the verb table, and the tier order it rests on is unchanged.

/// The verb a tap produced.
enum class OrderVerb : std::uint8_t
{
  /// A tap on empty space with nothing selected. **Stated rather than absent**: "nothing happens" is
  /// a decision in section 4 and not a gap.
  None,
  /// Empty space with something selected.
  MoveTo,
  /// A hostile ship or station.
  Attack,
  /// An asteroid with ore. **A standing order** -- the miner shuttles until told otherwise
  /// (`GameDesign.md` section 4) -- which is why it is its own verb rather than a move.
  Mine,
  /// Your own station. **The selection is unchanged**, which is the one row of the table that is
  /// about the interface rather than about the world.
  OpenBuildPanel,
  /// One of your own ships. **Replaces** the selection with that ship.
  Select
};

/// One tap, all the way through.
///
/// R8: a public aggregate.
struct SelectionOutcome
{
  OrderVerb verb = OrderVerb::None;

  /// Meaningful for `MoveTo`, in world units.
  float worldX = 0.0f;
  float worldY = 0.0f;

  /// Meaningful for `Attack`, `OpenBuildPanel` and `Select`: the packed wire identity.
  WireIdentity target = NO_WIRE_IDENTITY;

  /// Meaningful for `Mine` (M2.8): the rock's field index, and `worldX`/`worldY` are its place on the plane
  /// -- where the part of the selection that cannot mine is sent.
  std::uint16_t rock = 0;

  /// True when the selection changed, so a caller knows whether to redraw the panel.
  bool selectionChanged = false;
};

/// The player's current selection, and what a tap does to it.
///
/// **THE SELECTION IS PACKED WIRE IDENTITIES AND NOT POINTERS.** A snapshot replaces the entity
/// records every fifty milliseconds and a pointer into the last one is a pointer into a buffer that
/// has moved -- and an identity carries its generation, so a slot reused under the selection resolves
/// to nothing rather than to the new occupant.
class Selection
{
public:
  /// What is selected now.
  [[nodiscard]] std::span<const WireIdentity> Identities() const noexcept
  {
    return m_identities;
  }

  [[nodiscard]] bool IsEmpty() const noexcept
  {
    return m_identities.empty();
  }

  [[nodiscard]] std::size_t Count() const noexcept
  {
    return m_identities.size();
  }

  [[nodiscard]] bool Contains(WireIdentity _identity) const noexcept;

  /// **THE ONLY WAY TO DESELECT** is the panel's clear target (`Interface.md` section 4): a tap on
  /// empty space is already a move order, so there is no gesture for it.
  void Clear() noexcept
  {
    m_identities.clear();
  }

  /// Replaces the selection with one entity, which is what a tap on your own ship does.
  void ReplaceWith(WireIdentity _identity);

  /// Adds without duplicating, which is what M1.11's expansion needs.
  void Add(WireIdentity _identity);

  /// **DROPS EVERYTHING THE NEWEST SNAPSHOT NO LONGER CARRIES.** A selected ship that died is a
  /// selection the player cannot act on and a panel that counts wrong; a slot reused since is worse,
  /// because the identity would resolve to somebody else's ship. Returns how many were dropped.
  std::size_t RetainLiving(std::span<const EntityRecord> _entities);

  /// **WHERE THE SELECTION IS, FOR A HOLD** (ADR-018 decision 7; the 2026-09-23 review, m9): the newest known
  /// position of every selected entity, in world units, in selection order. An identity _entities does not
  /// carry is skipped, whole identity compared as `RetainLiving` does. _outX and _outY are cleared first; both
  /// empty is "nothing selected", which `Recenter` sends to the station.
  void PositionsOf(std::span<const EntityRecord> _entities, std::vector<float>& _outX, std::vector<float>& _outY) const;

  /// One tap, against the camera, the snapshot and this selection.
  ///
  /// **IT DOES NOT SEND ANYTHING.** The verb and the target come back and the caller builds the
  /// command, because R19 makes the host authoritative over what an order does and this class is
  /// presentation.
  ///
  /// _rocks is the field as drawn (M2.8), so a tap can land on an asteroid.
  [[nodiscard]] SelectionOutcome Tap(const CameraPose& _pose, const HitTestRequest& _request, std::span<const EntityRecord> _entities,
                                     std::span<const RockPickPoint> _rocks = {});

private:
  std::vector<WireIdentity> m_identities;
};

/// The verb a resolved pick becomes. Separated from `Selection::Tap` so the table itself is a pure
/// function a test can walk row by row.
///
/// _hasSelection is what makes the empty-space row two rows.
[[nodiscard]] OrderVerb VerbForPick(const TapOutcome& _outcome, bool _hasSelection) noexcept;

} // namespace Outpost
