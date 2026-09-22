#pragma once

#include "NeuronCore.h"

#include <array>
#include <cstddef>

namespace Neuron
{

/// Surface temperature to colour. [`ADR-019`](../Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md):
/// **stellar colour IS surface temperature** -- hot stars blue-white at 10,000 K and up, the Sun yellow
/// at 5,800 K, cool dwarfs orange-red near 3,000 K.
///
/// **IT IS THE ENGINE'S BECAUSE A PLANCKIAN LOCUS KNOWS NOTHING ABOUT A GAME** (R9). What temperatures
/// *this* sky draws from, and how far they are desaturated, is `GameClient/SkyLook.h`.
///
/// **COMPUTED FROM THE PHYSICS, NOT RECALLED FROM A TABLE OF RGB TRIPLES.** Planck's law gives the
/// spectral radiance at a temperature; the CIE 1931 observer integrates that to XYZ; a matrix takes XYZ
/// to linear sRGB. The alternative -- typing in eight remembered colours -- is a table nobody can check
/// and that drifts the day somebody adds a ninth stop by eye.
///
/// **THE OBSERVER IS AN ANALYTIC FIT AND THAT IS STATED RATHER THAN HIDDEN.** The CIE colour matching
/// functions are tabulated data; carrying that table would be several hundred numbers for a backdrop.
/// What is used instead is the multi-lobe Gaussian fit of **Wyman, Sloan and Shirley (2013)**, which is
/// a published closed form accurate to within a percent or so of the tabulated curves across the visible
/// band. For a star field desaturated to a fifth, that is far below what anybody could see.
///
/// **INTEGRATED AT A FIXED STEP, SO THE ANSWER IS THE SAME EVERYWHERE.** Five nanometres from 380 to
/// 780, which is 81 samples; R16 does not reach the renderer, but a table a test pins exactly still has
/// to be the same table on two machines.

/// The visible band this integrates over, and the step it takes.
inline constexpr float SPECTRUM_START_NANOMETRES = 380.0f;
inline constexpr float SPECTRUM_END_NANOMETRES = 780.0f;
inline constexpr float SPECTRUM_STEP_NANOMETRES = 5.0f;

/// Eight stops, which is what ADR-019 asked for, spanning the range a star field actually contains.
inline constexpr std::size_t BLACKBODY_STOP_COUNT = 8;

/// A colour, linear, with the brightest channel normalized to one.
///
/// **NORMALIZED RATHER THAN ABSOLUTE**, because a blackbody's total radiance goes as the fourth power of
/// its temperature and nothing here wants a star that is ten thousand times brighter than another. How
/// bright a star is drawn is the magnitude tier's business (ADR-019), and it is deliberately separate
/// from what colour it is.
///
/// R8: a public aggregate.
struct Chromaticity
{
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;

  [[nodiscard]] friend constexpr bool operator==(const Chromaticity&, const Chromaticity&) noexcept = default;
};

/// The temperatures the stops sit at, in kelvin. **2,000 to 20,000** covers the cool dwarfs and the
/// hot blue-white stars ADR-019 names, with the Sun's 5,800 comfortably inside.
inline constexpr std::array<float, BLACKBODY_STOP_COUNT> BLACKBODY_STOPS{2000.0f, 3000.0f, 4000.0f,  5000.0f,
                                                                         6500.0f, 8000.0f, 12000.0f, 20000.0f};

/// The colour of a blackbody at _kelvin, computed. Clamped to the ends of the range above rather than
/// extrapolated -- a negative temperature has no colour and a million kelvin is not in this sky.
[[nodiscard]] Chromaticity BlackbodyColor(float _kelvin) noexcept;

/// The eight stops, computed once. **This is the table ADR-019 asked to be pinned**, and it is a
/// function rather than a literal so that the pinning is of the physics and not of somebody's typing.
[[nodiscard]] const std::array<Chromaticity, BLACKBODY_STOP_COUNT>& BlackbodyTable() noexcept;

/// The same colour with the saturation pulled toward white.
///
/// **REAL STARS READ VERY NEARLY WHITE** (ADR-019), and oversaturated red and blue confetti is the
/// single most common way a procedural star field announces itself as fake. _saturation of one is the
/// chromaticity untouched and zero is white; ADR-019 asks for **about 0.2**.
///
/// The grey it pulls toward is the colour's own luminance under Rec. 709, so a desaturated hot star
/// stays as bright as it was rather than getting darker as it gets whiter.
[[nodiscard]] Chromaticity Desaturate(const Chromaticity& _color, float _saturation) noexcept;

} // namespace Neuron
