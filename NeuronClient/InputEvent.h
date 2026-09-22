#pragma once

#include "NeuronCore.h"

#include <cstdint>

namespace Neuron
{

/// R21's whole vocabulary, and there is no other. `Tapped`, `Holding` and a manipulation's three
/// moments are what `GestureRecognizer` emits; an interaction is named here or it does not exist.
///
/// A DOUBLE TAP IS NOT IN THIS LIST AND MUST NOT BE ADDED TO IT (ADR-017). It arrives as `Tapped`
/// carrying a count, because the first tap has already acted and the second only upgrades the
/// result -- which is the whole reason a double tap is affordable in a game where no tap may wait.
enum class InputEventKind : std::uint8_t
{
  Tapped,
  Holding,
  ManipulationStarted,
  ManipulationUpdated,
  ManipulationCompleted
};

/// One interaction, as plain values.
///
/// THIS TYPE IS WHY THE SEAM IS SPLIT IN TWO (`Plan/README.md` F6). Nothing in this tree can
/// construct a `CoreWindow`, and both client suites are desktop test DLLs, so the arithmetic under
/// a gesture can have the suite R21 requires only if it never sees a `PointerPoint`. The half that
/// touches the Windows Runtime turns each event into one of these; the half that does arithmetic
/// takes only these.
///
/// EVERY LENGTH HERE IS AUTHORED (`Interface.md` section 1), never a device-independent pixel and
/// never a physical one. The seam converts once, through the interface fit, so that every constant
/// the arithmetic compares against is the constant `Interface.md` states -- 16 for the tap slop, 78
/// for a palm, 24 for the pick radius. A record in DIPs would make all three depend on the display
/// scale, which is exactly the defect R18 exists to catch one layer down.
///
/// THE VALUES ARE RAW. The palm and the non-touch pointer are gone already -- those are dropped at
/// the seam and never become a record at all -- but the tap slop, the rotation deadzone, its latch
/// and the scale deadzone are `GestureArithmetic`'s and have not been applied here.
///
/// R8: a public aggregate, so plain fields and brace initialization.
struct InputEvent
{
  InputEventKind kind = InputEventKind::Tapped;

  /// How many contacts were down when this arrived. `GestureRecognizer` does not report it, so the
  /// seam counts (`Interface.md` section 2); the LATCH that makes a manipulation keep its meaning
  /// is `GestureArithmetic`'s, because that is the half a test can reach.
  std::uint32_t contactCount = 0;

  /// `TappedEventArgs::TapCount` under `GestureSettings::DoubleTap` (ADR-017). One or two, and zero
  /// on anything that is not a tap.
  std::uint32_t tapCount = 0;

  /// Where it happened, in authored pixels.
  float xAuthoredPixels = 0.0f;
  float yAuthoredPixels = 0.0f;

  /// The manipulation's CUMULATIVE translation since it began, in authored pixels. Cumulative
  /// rather than per-update because every number below it is cumulative and because ADR-018's
  /// camera is absolute positioning rather than a rate.
  float translationXAuthoredPixels = 0.0f;
  float translationYAuthoredPixels = 0.0f;

  /// Cumulative scale, as the multiplier the recognizer reports. One is no zoom; **above one the
  /// fingers moved APART.** R21 names this as a thing a package can hide and a test cannot.
  float scale = 1.0f;

  /// Cumulative rotation in degrees, **positive CLOCKWISE**, which is the recognizer's convention
  /// and is kept rather than converted. The other half of the sign R21 names.
  float rotationDegrees = 0.0f;
};

} // namespace Neuron
