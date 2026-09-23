#pragma once

#include "Camera.h"
#include "HitTest.h"
#include "HudLayout.h"

#include "GameCore.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// M2.11: **placing a module, and upgrading one, from the client's side** (`Interface.md` section 6, *Placing a
/// module*; `OpenQuestions.md` Q54, Q57). Choosing a module in the build panel arms a placement; while armed, a
/// tap on the plane is the placement's rather than a move order's. Everything here is pure and has a suite over
/// it (R20): the executable holds only the order the calls happen in.
///
/// **THE PREVIEW IS THE HOST'S OWN RULE**, `CheckModuleSite` from `GameCore`, evaluated on the records this client
/// was sent -- which is what R19 permits and what R23 already does for the field. The host decides.

/// Which module is armed, if one is. **A second tap on the armed button disarms it** (`Interface.md` section 6);
/// a tap on a different module button moves the arming to it.
class ModuleArming
{
public:
  /// Arms _design, or disarms when _design is already armed.
  void Toggle(DesignId _design) noexcept;

  void Disarm() noexcept
  {
    m_armed = false;
  }

  [[nodiscard]] bool IsArmed() const noexcept
  {
    return m_armed;
  }

  /// Meaningful only while armed.
  [[nodiscard]] DesignId Armed() const noexcept
  {
    return m_design;
  }

private:
  DesignId m_design = DesignId::ModuleShipyardL1;
  bool m_armed = false;
};

/// What an armed tap does.
enum class PlacementAction : std::uint8_t
{
  /// A tap the placement consumes and does nothing with: outside the radius, on the station, on a module that
  /// is not the one an armed upgrade wants, or anywhere the preview refuses. **The arming stays**, so the next
  /// tap can try again.
  Nothing,
  /// Send a `PlaceModule` at `site`.
  Place,
  /// Send an `UpgradeModule` naming `module`.
  Upgrade,
  /// **A tap on one of your own ships is still a selection** and not the placement's: it replaces the selection,
  /// which takes the build panel and the arming with it.
  FallThrough
};

/// R8: a public aggregate.
struct PlacementOutcome
{
  PlacementAction action = PlacementAction::Nothing;

  /// Where a `Place` goes, **already on the wire's grid** -- quantized and back -- so the preview judged the
  /// same point the host will.
  Neuron::Vec2 site{};

  /// The module an `Upgrade` names.
  WireIdentity module = NO_WIRE_IDENTITY;

  /// Why the preview refused an empty-space tap, for the log. `None` for anything else.
  ModuleSiteFault fault = ModuleSiteFault::None;
};

/// This player's modules, as `CheckModuleSite` takes them, from the records this client holds.
[[nodiscard]] std::vector<PlacedModule> OwnModules(std::span<const EntityRecord> _entities, PlayerId _player);

/// This player's station record. False when the client holds none -- before the first snapshot, say.
[[nodiscard]] bool OwnStation(std::span<const EntityRecord> _entities, PlayerId _player, EntityRecord& _outStation) noexcept;

/// **ONE ARMED TAP, RESOLVED.** Under the tap first, as every tap is (`Interface.md` section 4): an own ship falls
/// through to selection; an own module is an upgrade when the armed level upgrades it (Q57) and nothing
/// otherwise; anything else under the finger is nothing. Empty space is a placement when the armed design is a
/// placed level and the site is legal.
[[nodiscard]] PlacementOutcome ResolvePlacementTap(const CameraPose& _pose, const HitTestRequest& _request,
                                                   std::span<const EntityRecord> _entities, std::span<const RockPickPoint> _rocks,
                                                   DesignId _armed);

[[nodiscard]] Command BuildPlaceModuleCommand(std::uint16_t _sequence, const Neuron::Vec2& _site, DesignId _design) noexcept;

[[nodiscard]] Command BuildUpgradeModuleCommand(std::uint16_t _sequence, WireIdentity _module, DesignId _level) noexcept;

/// **THE PLACEMENT RADIUS, AS THE INTERFACE PASS CAN DRAW IT** (`design_handoff_hud`: world space, r = 400, 48
/// segments with 24 drawn, 2 px). The interface draws axis-aligned rectangles and nothing else, so each drawn
/// segment is projected onto the screen and stepped as 2 x 2 squares -- a dashed ellipse, which is what a world
/// circle is from a tilted camera. **In authored pixels**, so the interface's fit transform places it (ADR-016).
/// A segment with an end behind the camera is left out.
[[nodiscard]] std::vector<HudRect> PlacementRingSquares(const CameraPose& _pose, float _aspectRatio, float _authoredWidth,
                                                        float _authoredHeight, const Neuron::Vec2& _center);

/// The handoff's figures for the ring.
inline constexpr std::size_t PLACEMENT_RING_SEGMENTS = 48;
inline constexpr std::int32_t PLACEMENT_RING_STROKE_PIXELS = 2;

} // namespace Outpost
