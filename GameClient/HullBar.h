#pragma once

#include "Camera.h"

#include "GameCore.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// The handoff's bar: an 18 x 6 keyline, with a 16 x 4 inner bar one pixel in.
inline constexpr std::int32_t HULL_BAR_WIDTH_PIXELS = 18;
inline constexpr std::int32_t HULL_BAR_HEIGHT_PIXELS = 6;
inline constexpr std::int32_t HULL_BAR_INNER_WIDTH_PIXELS = 16;

/// How far above the hull's projected top the keyline's top sits (`geometry.json`: `py - halfH - 14`).
inline constexpr std::int32_t HULL_BAR_LIFT_PIXELS = 14;

/// One bar, in authored pixels. R8: a public aggregate.
struct HullBarPlacement
{
  /// The keyline's top-left.
  std::int32_t x = 0;
  std::int32_t y = 0;

  /// Hull left, 0 to 99: a bar is only ever drawn for something damaged.
  std::uint8_t percentRemaining = 0;
};

/// **HULL BARS IN THE WORLD, ON DAMAGED THINGS ONLY** (M3.3b, `Interface.md` section 6): one for every drawn record
/// under full hull, centered over its projected position and `HULL_BAR_LIFT_PIXELS` above its projected top. **A
/// fixed size at every depth**, so a healthy fleet draws nothing and a dying one is a row of red. Projected through
/// the camera and then the interface's own frame (ADR-016); anything off the frame or behind the camera draws none.
///
/// **Stations and modules draw one too**, which the handoff's "a damaged ship" does not name: the station is the thing
/// a siege shells, and a bar only on the ships would leave it silent.
[[nodiscard]] std::vector<HullBarPlacement> PlaceHullBars(std::span<const EntityRecord> _drawn, const CameraPose& _camera,
                                                          float _aspectRatio);

} // namespace Outpost
