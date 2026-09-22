#pragma once

#include "FitTransform.h"
#include "InputEvent.h"

#include <cstdint>

namespace Neuron
{

/// `Interface.md` section 1's gesture constants, which are stated there because a number nobody
/// wrote down is a number a suite cannot pin. All four are AUTHORED pixels or plain ratios, so none
/// of them moves when the display scale, the window size or the world's resolution does.

/// The travel that separates a tap from a pan -- 16 authored pixels, 3.05 mm. Above handheld tremor
/// and below a third of the 48-pixel touch floor.
///
/// IT IS PINNED HERE RATHER THAN INHERITED FROM THE RECOGNIZER (`Interface.md` section 3). The
/// asymmetry the design rests on -- an accidental pan is free, an accidental move order costs a
/// fleet -- only holds if this number is right, and `GestureRecognizer` has no property that sets
/// it. What that buys is a floor and not a ceiling: the recognizer still decides when it starts
/// reporting a manipulation at all, so the effective threshold is the larger of the two, and which
/// one binds on the target device is a thing to look at on the device rather than to assert here.
inline constexpr float TAP_SLOP_AUTHORED_PIXELS = 16.0f;

/// A contact wider or taller than this is a palm and never becomes an input record
/// (`Interface.md` section 2). 78 authored pixels is 14.9 mm; a fingertip is 8 to 12.
inline constexpr float CONTACT_REJECTION_AUTHORED_PIXELS = 78.0f;

/// Rotation is ignored until a manipulation's cumulative rotation exceeds this, because two fingers
/// dragging to pan are never exactly parallel and a camera that yaws whenever you pan is unusable
/// (`Interface.md` section 5).
inline constexpr float ROTATION_DEADZONE_DEGREES = 8.0f;

/// And zoom needs one for the mirror-image reason: fingers that rotate also change separation
/// slightly, and pitch is coupled to zoom, so a pure orbit would silently re-pitch the camera. Two
/// per cent, which is below that noise and above nothing a player intends.
inline constexpr float SCALE_DEADZONE_FRACTION = 0.02f;

/// `Windows::Devices::Input::PointerDeviceType::Touch`, as the integer the projection gives it, so
/// that this header names no Windows Runtime type and the suite that pins the rule below needs
/// none either. `GestureSeam.cpp` static_asserts that this number is still the projection's, at the
/// one site that performs the drop.
inline constexpr std::uint32_t POINTER_DEVICE_TYPE_TOUCH = 0;

/// R21: touch is the only input, and mouse and pen have no compatibility path. A key, a hover, a
/// second button and a wheel are four things a finger does not have.
[[nodiscard]] constexpr bool IsTouchPointer(std::uint32_t _pointerDeviceType) noexcept
{
  return _pointerDeviceType == POINTER_DEVICE_TYPE_TOUCH;
}

/// Palm rejection, in authored pixels. EXCEEDING the threshold in either dimension rejects, so a
/// contact exactly 78 across is still a finger.
///
/// A CONTACT THAT REPORTS NO SIZE IS A FINGER. Some digitizers give an empty `ContactRect`, and the
/// alternative -- treating "unknown" as a palm -- is a device on which the game cannot be played at
/// all, which is a worse failure than a camera that occasionally lurches.
[[nodiscard]] constexpr bool IsFingertipContact(float _widthAuthoredPixels, float _heightAuthoredPixels) noexcept
{
  return (_widthAuthoredPixels <= CONTACT_REJECTION_AUTHORED_PIXELS) && (_heightAuthoredPixels <= CONTACT_REJECTION_AUTHORED_PIXELS);
}

/// A point in the interface's authored coordinates. R8: a public aggregate.
struct AuthoredPoint
{
  float xPixels = 0.0f;
  float yPixels = 0.0f;

  [[nodiscard]] friend constexpr bool operator==(const AuthoredPoint&, const AuthoredPoint&) noexcept = default;
};

/// The two numbers that stand between a pointer and an authored coordinate, held together because
/// using one without the other is the defect (R18): a `CoreWindow` reports POSITIONS IN
/// DEVICE-INDEPENDENT PIXELS while the swap chain, the interface fit and every rectangle M0.17
/// places are in PHYSICAL ones. On the target device that factor is two, so an omitted conversion
/// puts every tap at half the distance from the top left corner and nothing fails.
///
/// R8: a public aggregate.
struct AuthoredSpace
{
  /// `ComputeInterfaceFit` over the BACK BUFFER's size (ADR-016), never `ComputeFit` over the scene
  /// target's. The world's fit is identity at the 1:1 default, which would put every tap in the top
  /// left quarter of the frame.
  FitTransform interfaceFit{};

  /// `DisplayInformation::RawPixelsPerViewPixel`, the same number `WindowMetrics` carries.
  float rawPixelsPerViewPixel = 1.0f;
};

/// Back-buffer pixels to authored ones: the inverse of M0.17's `MapAuthoredRect`, for a point. It
/// is the only way a tap's position becomes a number `Interface.md`'s constants can be compared
/// against, which is why hit testing stays authored while only the draw call sees physical pixels.
[[nodiscard]] AuthoredPoint AuthoredFromPhysical(const FitTransform& _interfaceFit, float _physicalXPixels,
                                                 float _physicalYPixels) noexcept;

/// The same for a length, which has no offset to subtract. The interface fit preserves the aspect
/// ratio, so one axis answers for both to within the integer rounding of the other -- a contact
/// size and a travel distance are both well inside that.
[[nodiscard]] float AuthoredLengthFromPhysical(const FitTransform& _interfaceFit, float _physicalLengthPixels) noexcept;

/// What the seam actually calls: device-independent pixels straight to authored ones.
[[nodiscard]] AuthoredPoint AuthoredFromDips(const AuthoredSpace& _space, float _xDips, float _yDips) noexcept;

[[nodiscard]] float AuthoredLengthFromDips(const AuthoredSpace& _space, float _lengthDips) noexcept;

/// The state one manipulation carries from `ManipulationStarted` to `ManipulationCompleted`, and
/// the reason the gate below is a function over a value rather than a pure function alone: a latch
/// is state, and it is state a test has to be able to hold and inspect.
///
/// R8: a public aggregate.
struct ManipulationGate
{
  /// False before a manipulation starts and after it completes. An update that arrives outside one
  /// is a dropped event rather than a manipulation beginning halfway through.
  bool active = false;

  /// LATCHED AT `ManipulationStarted` AND NEVER CHANGED (`Interface.md` section 2). A manipulation
  /// means two different things depending on how many fingers began it, and **a thumb landing
  /// mid-drag must not change what the drag is doing** -- the alternative is a camera that lurches
  /// whenever a hand rests on the glass.
  std::uint32_t contactCount = 0;

  /// `Interface.md` section 5's latch: once rotation engages it stays engaged for the rest of this
  /// manipulation. Without it the camera stutters every time the player crosses back under the
  /// threshold mid-gesture, which is worse than no deadzone at all.
  bool rotationEngaged = false;

  /// The cumulative rotation AT THE MOMENT THE DEADZONE WAS CROSSED, which the heading is measured
  /// from -- the same arrangement the pan has below, and settled as such rather than assumed
  /// (`OpenQuestions.md` Q38).
  float rotationOriginDegrees = 0.0f;

  /// Whether the travel has crossed the tap slop and become a pan. There is no way back: a pan that
  /// returned to within 16 pixels of its origin would otherwise become a tap again on release.
  bool panEngaged = false;

  /// The cumulative translation AT THE MOMENT THE SLOP WAS CROSSED, which is what the pan is
  /// measured from. `Interface.md` section 3: begin the pan at the point the threshold was crossed,
  /// so the first 16 pixels are not lost to a jump.
  float panOriginXAuthoredPixels = 0.0f;
  float panOriginYAuthoredPixels = 0.0f;
};

/// One manipulation update, after the deadzones, the latch and the slop. R8: a public aggregate.
struct GatedManipulation
{
  /// The latched count, not the live one.
  std::uint32_t contactCount = 0;

  /// False while the travel is still inside the tap slop, which is the window in which a release
  /// is still a tap.
  bool isPanning = false;

  /// Measured from the crossing point, so it is zero on the update that engages the pan.
  float panXAuthoredPixels = 0.0f;
  float panYAuthoredPixels = 0.0f;

  /// One inside the deadzone. **Above one the fingers moved apart**, which is the sign R21 names.
  float scale = 1.0f;

  /// Zero until the deadzone is crossed, and measured FROM THE CROSSING after it. **Positive is
  /// clockwise**, the recognizer's convention, kept.
  ///
  /// IT REBASES AT THE CROSSING EXACTLY AS THE PAN DOES (`Interface.md` section 5,
  /// `OpenQuestions.md` Q38), so engaging rotation moves nothing. The deadzone exists because a pan
  /// and a pinch rotate by ACCIDENT, so the crossing is usually reached unintentionally -- passing
  /// the whole cumulative through would snap the world eight degrees in the middle of a pan, which
  /// makes the accident it exists to absorb worse rather than better. What it costs is about eight
  /// degrees of every deliberate orbit.
  float rotationDegrees = 0.0f;
};

/// Feeds one input record through the gate, updating it.
///
/// A `Tapped` and a `Holding` carry no manipulation and come back zeroed; a tap's meaning is what
/// is under it (`Interface.md` section 4) and that is M0.21's, not this function's.
[[nodiscard]] GatedManipulation ApplyGate(ManipulationGate& _gate, const InputEvent& _event) noexcept;

} // namespace Neuron
