#pragma once

#include "NeuronCore.h"

#include <cstdint>

namespace Neuron
{

/// R13's three cases, and which one applies is decided in integers rather than by comparing
/// floats. **R13's whole arrangement is worth nothing if a conversion error lands the scale at
/// 1.99 rather than 2, or at 0.999 rather than 1** -- so nothing here asks whether a float is
/// close to a whole number.
enum class FitFilter : std::uint8_t
{
  /// The destination already matches the source: no resample at all.
  None,
  /// An exact integer multiple. Point sampling doubles pixels, which is sharp rather than crisp
  /// (ADR-016 corrects ADR-007 on exactly this) -- nothing finer than the multiple can exist.
  Point,
  /// Anything else, aspect preserved and centered.
  Bilinear
};

/// Where a source rectangle lands inside a destination one, and how it is sampled getting there.
///
/// R8: a public aggregate.
struct FitTransform
{
  /// Destination pixels per source pixel. Exact at the two settled points -- 1 and 2 -- because
  /// those are the cases the integer path produces rather than the float one.
  float scale = 1.0f;

  /// The fitted rectangle, in destination pixels.
  std::int32_t offsetX = 0;
  std::int32_t offsetY = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;

  FitFilter filter = FitFilter::None;

  /// Bars down the sides. A 3:2 source in a 16:9 window.
  [[nodiscard]] constexpr bool IsPillarboxed() const noexcept
  {
    return offsetX > 0;
  }

  /// Bars top and bottom. A 16:9 source in a 3:2 window.
  [[nodiscard]] constexpr bool IsLetterboxed() const noexcept
  {
    return offsetY > 0;
  }

  [[nodiscard]] friend constexpr bool operator==(const FitTransform&, const FitTransform&) noexcept = default;
};

/// Aspect preserved, centered, and the filter chosen by R13's three cases.
///
/// IT RETURNS A VALUE RATHER THAN APPLYING ONE, AND IT IS CALLED TWICE (ADR-016). The **world
/// fit** takes the scene target's size; the **interface fit** takes 1440 x 960, the authored
/// layout space. Those are two different numbers doing two different jobs and they coincide only
/// at a 0.5 world scale.
///
/// An earlier version of this step said "two consumers, one computation" and meant one value.
/// That is the defect ADR-016 corrects: at the 1:1 world default the shared transform is identity,
/// which renders every panel, glyph and touch target at half size in one corner. One place asks
/// the window anything; **two values come out of it**.
[[nodiscard]] FitTransform ComputeFit(std::int32_t _sourceWidth, std::int32_t _sourceHeight, std::int32_t _destinationWidth,
                                      std::int32_t _destinationHeight) noexcept;

/// ADR-016 section 2: the interface's authored layout space, and NOT a rendering decision. It is a
/// statement about fingertips -- 48 authored pixels is 96 physical pixels and 9.15 mm against the
/// PANEL (`Interface.md` section 1) -- so no layout number moves when the world's resolution does.
inline constexpr std::int32_t INTERFACE_AUTHORED_WIDTH = 1440;
inline constexpr std::int32_t INTERFACE_AUTHORED_HEIGHT = 960;

/// The interface fit, which is the whole reason this file returns two values instead of one.
[[nodiscard]] inline FitTransform ComputeInterfaceFit(std::int32_t _destinationWidth, std::int32_t _destinationHeight) noexcept
{
  return ComputeFit(INTERFACE_AUTHORED_WIDTH, INTERFACE_AUTHORED_HEIGHT, _destinationWidth, _destinationHeight);
}

} // namespace Neuron
