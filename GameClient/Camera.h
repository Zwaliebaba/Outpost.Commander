#pragma once

#include "GameCore.h"

#include "NeuronCore.h"

namespace Outpost
{

/// ADR-001's four degrees of freedom and ADR-018's one solve, and nothing else.
///
/// FLOATS ARE CORRECT HERE. R16 governs the simulation and this is the renderer: two clients whose
/// cameras disagree by a thousandth of a degree disagree about nothing the host will ever ask them,
/// and `Scripts/CheckDeterminism.py` does not sweep `GameClient` for exactly that reason. What the
/// plane's geometry must be is *exact enough to invert*, which is a tolerance rather than a
/// bit-for-bit property, and the round trip is what the suite pins.
///
/// THE BUDGET IS SMALLER THAN "A 3D CAMERA" AND THAT IS A DECISION, NOT AN OMISSION. A focus point
/// on the plane, a heading, and a distance -- with pitch DERIVED from the distance (ADR-001). A
/// fifth degree of freedom is not a feature to add later; it is ADR-001 being reversed, and the
/// gesture budget R21 leaves has nothing to spend on it.
///
/// THE MATRIX ARRIVED WITH THE RENDERER THAT NEEDED ONE (M0.21b). M0.20 shipped none and said why:
/// nothing drew the world, so a projection here would have been an API with no consumer, written
/// against a pipeline nobody had chosen. That is no longer true, and `ViewProjection` below is it.
/// What M0.20 built stands unchanged underneath it -- a ray onto the plane, a point back onto the
/// screen, and the solve between them -- and the suite asserts the two agree to within a pixel,
/// which is what stops the renderer and the tap disagreeing about where a ship is.

/// `Interface.md` section 5, pinned there rather than derived: 40 degrees vertical.
inline constexpr float VERTICAL_FIELD_OF_VIEW_DEGREES = 40.0f;

/// The floor, and it is protecting three things at once (`Interface.md` section 5, ADR-018): tap
/// precision, ADR-010's selection wedge, and the anchor solve itself -- an anchor near the horizon
/// needs an unbounded focus movement to stay under the finger, and this is what stops that.
inline constexpr float MINIMUM_PITCH_DEGREES = 30.0f;

/// "Near top-down" (`Interface.md` section 5), and the five degrees short of vertical are load
/// bearing rather than taste: at exactly 90 the forward vector and the world's up are parallel and
/// the camera basis has no defined roll. The design's 22,500-unit far distance is the arithmetic
/// for a VERTICAL camera -- 16,384 over twice the tangent of 20 degrees is 22,507 -- so at 85 the
/// frame covers a little more than the square on its height, which the margin below absorbs.
///
/// **NOT A FIGURE THE DESIGN STATES.** `Interface.md` section 5 says "near top-down" and pins the
/// far distance; this is the plan reading a number out of that sentence, and M1.8 is where both
/// ends of the range are pinned for real.
inline constexpr float MAXIMUM_PITCH_DEGREES = 85.0f;

/// The far end, from `Interface.md` section 5 and ADR-018's third owed measurement: showing the
/// whole 16,384-unit square at a 40-degree vertical field of view.
inline constexpr float MAXIMUM_CAMERA_DISTANCE = 22500.0f;

/// The near end, which **ADR-018 explicitly does not pin** -- it says "at a close view of roughly
/// 1,500 world units it would be about 1,400", and leaves M1.8 to settle it. 1,400 is that
/// sentence taken at its word, and it makes the range 16x, which is the figure both documents
/// quote for "about two pinch gestures".
inline constexpr float MINIMUM_CAMERA_DISTANCE = 1400.0f;

/// The play area's half extent in the renderer's units, **derived from `GameCore`'s figure rather
/// than restated**. `GameCore/Command.h` already owns it as a `Neuron::Fixed` -- it is what the
/// host clamps a command target against -- and a second literal here would be a second place to
/// update the day the map changes size, which is the defect this tree keeps finding in its own
/// documents. 2,097,152 over 256 is 8,192, half of `GameDesign.md`'s 16,384-unit square.
inline constexpr float PLAY_AREA_HALF_EXTENT_UNITS = static_cast<float>(PLAY_AREA_HALF_EXTENT) / static_cast<float>(Neuron::FIXED_ONE);

/// "The play area plus a margin" (`Interface.md` section 5, ADR-018 decision 6) -- and the margin
/// is a number **neither document states**, so here is the one this file takes and why.
///
/// 1,024 units is a sixteenth of the square and about two thirds of the roughly 1,500-unit close
/// view. What that buys is that at the closest zoom a ship in the very corner can sit off-center
/// instead of pinned against the frame's edge, which is the situation the margin exists for. Small
/// enough that the empty band around the map never becomes somewhere to fly to.
inline constexpr float FOCUS_CLAMP_MARGIN = 1024.0f;

/// On release the heading snaps to the nearest cardinal within this (ADR-018 decision 5,
/// `Interface.md` section 5, both of which say "a stated threshold" and state no number).
///
/// Ten degrees, and the argument is the eight-degree rotation deadzone next door: the snap has to
/// be wider than the deadzone or a rotation that barely engaged would not be tidied up by it, and
/// narrow enough that a deliberate diagonal -- a player holding the map at 45 degrees to read a
/// formation -- is left alone. Between 8 and 45, nearer the bottom.
inline constexpr float CARDINAL_SNAP_DEGREES = 10.0f;

/// A point or a direction in the world. Three floats and no operations of its own: what this file
/// does with them is specific enough that a general vector type would be a larger surface than the
/// half dozen expressions below.
///
/// **Z IS UP AND THE PLANE IS Z = 0.** The simulation's two coordinates are x and y (ADR-001,
/// ADR-022), so a wire position drops in unchanged and the third coordinate belongs to the camera
/// and to whatever the renderer later draws above and below the plane.
///
/// R8: a public aggregate.
struct Vector3
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

/// ADR-001's four degrees of freedom, and pitch is not among them because it is derived.
///
/// R8: a public aggregate. Nothing here maintains an invariant -- the clamp is a function you call
/// rather than a rule the type enforces, because ADR-018 decision 6 wants the clamp applied *after
/// the solve* at one site, not silently on every assignment.
struct CameraPose
{
  /// On the plane. The height is not a field: the focus is on the plane by definition (ADR-001).
  float focusX = 0.0f;
  float focusY = 0.0f;

  /// Which way the camera faces across the plane, in radians. Zero looks along +x.
  float headingRadians = 0.0f;

  /// Eye to focus. Pitch follows from this and nothing else.
  float distance = MAXIMUM_CAMERA_DISTANCE;
};

/// The camera's basis in the world: where the eye is and the three axes it sees along.
///
/// It depends on heading, distance and the focus -- but the three AXES depend only on heading and
/// pitch, which is the fact ADR-018's closed-form solve is built on.
struct CameraBasis
{
  Vector3 eye;
  /// Unit, from the eye toward the focus.
  Vector3 forward;
  /// Unit, to the camera's right. Never rolls: it stays parallel to the plane (ADR-001).
  Vector3 right;
  /// Unit, completing the basis.
  Vector3 up;
};

/// Pitch from distance, in radians (`Interface.md` section 5, ADR-001).
///
/// **IT SATURATES AND DOES NOT TERMINATE.** The output is clamped at the floor; the distance is
/// not. Read the coupling the other way -- a floor on pitch ending the zoom range -- and the
/// camera quietly loses its close view, which is the opposite of what the floor is for. So a
/// distance below the near end is a legal distance that simply stops lowering the pitch.
[[nodiscard]] float PitchForDistance(float _distance) noexcept;

[[nodiscard]] CameraBasis BuildBasis(const CameraPose& _pose) noexcept;

/// Where a point on the screen meets the plane -- ADR-001 in code, and the same cast M0.21's tap
/// uses at a different moment.
///
/// _screenX and _screenY are normalized: -1 to +1, x right and **y up**, which is the projection's
/// own convention rather than a pixel row's. `AuthoredToNormalized` converts.
///
/// FALSE WHEN THE RAY DOES NOT MEET THE PLANE IN FRONT OF THE CAMERA, which is the horizon and
/// above it. The pitch floor is what keeps that off screen (`Interface.md` section 5) -- so a false
/// here means the floor has been bypassed, and the caller wants to know rather than to receive a
/// point from behind the eye.
[[nodiscard]] bool ScreenToPlane(const CameraPose& _pose, float _aspectRatio, float _screenX, float _screenY, float& _outWorldX,
                                 float& _outWorldY) noexcept;

/// The inverse, for a point on the plane. False when the point is behind the camera.
[[nodiscard]] bool PlaneToScreen(const CameraPose& _pose, float _aspectRatio, float _worldX, float _worldY, float& _outScreenX,
                                 float& _outScreenY) noexcept;

/// ADR-018's solve, which is the whole camera: the focus that puts _anchor under the screen point
/// _screenX, _screenY **at the pose given**.
///
/// THE POSE PASSED IN IS THE NEW ONE -- scale has already become a distance and therefore a pitch,
/// rotation has already become a heading. That ordering is decision 3 and it is what removes the
/// feedback loop between zoom and pitch: the anchor comes from the starting pose, the solve runs at
/// the new one, and there is no iteration anywhere.
///
/// **DO NOT ALSO APPLY THE RECOGNIZER'S TRANSLATION.** It is already in this solve. Applying both
/// double-counts it, and a camera that appears to accelerate under the finger is that defect and
/// almost always that one (ADR-018 decision 2).
///
/// _pose's focus is ignored -- it is the output. False when the screen point does not meet the
/// plane, exactly as ScreenToPlane.
[[nodiscard]] bool SolveFocusForAnchor(const CameraPose& _pose, float _aspectRatio, float _anchorWorldX, float _anchorWorldY,
                                       float _screenX, float _screenY, float& _outFocusX, float& _outFocusY) noexcept;

/// The play area plus its margin, applied **after** the solve (ADR-018 decision 6).
///
/// At the edge the ground stops and the finger slides over it, because the anchor is no longer
/// where the solve asked for. That reads as a defect the first time it is seen and it is the
/// correct behavior for a strategy map -- the alternative is a camera that fights the hand.
void ClampFocus(CameraPose& _pose) noexcept;

/// Distance clamped to the range, for a caller applying a pinch. The pitch floor does NOT bound
/// this -- see PitchForDistance.
[[nodiscard]] float ClampDistance(float _distance) noexcept;

/// The heading a release settles on: the nearest cardinal when one is within CARDINAL_SNAP_DEGREES,
/// and the heading untouched otherwise (ADR-018 decision 5).
[[nodiscard]] float SnapHeadingToCardinal(float _headingRadians) noexcept;

/// An authored-pixel point -- `Interface.md` section 1's 1440 x 960 frame, which is what the
/// gesture seam reports in -- as the normalized pair the functions above take. The y axis flips:
/// authored pixels count down from the top and the projection counts up.
void AuthoredToNormalized(float _authoredX, float _authoredY, float _authoredWidth, float _authoredHeight, float& _outScreenX,
                          float& _outScreenY) noexcept;

/// A 4x4 transform, row-major, in the order a constant buffer wants it.
///
/// **M0.20 SHIPPED NO MATRIX AND SAID WHY**: a projection with no consumer is written against a pipeline
/// nobody has chosen. M0.21b chooses one, so here it is, and the header above is amended rather than
/// left claiming there is none.
///
/// R8: a public aggregate.
struct Matrix4
{
  /// Row-major: `m[row * 4 + column]`. HLSL defaults to column-major packing, so the shader that
  /// consumes this is declared `row_major` -- one word there against a transpose on every frame here.
  float m[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
};

/// View times projection, for the pose given.
///
/// **LEFT-HANDED, WITH DEPTH IN [0, 1]**, which is Direct3D's convention and the handoff's: +Z forward,
/// Y up, and the near plane at zero rather than at minus one. Getting this wrong produces a picture that
/// looks plausible and is mirrored, which is the failure `CameraTests` pins directly rather than through a
/// round trip -- a round trip inverts twice and agrees with itself.
///
/// The clip planes are Q39's: near 50, far 50,000.
[[nodiscard]] Matrix4 ViewProjection(const CameraPose& _pose, float _aspectRatio) noexcept;

/// Where one entity goes: its position on the plane and its heading, as a world transform.
///
/// The heading rotates about Y, which is the axis out of the plane (`Vector3`'s note). A wire heading
/// widens through `DequantizeWireHeading` before it gets here.
[[nodiscard]] Matrix4 EntityTransform(float _worldX, float _worldY, Neuron::Angle _heading) noexcept;

/// Q39's clip planes, from `Interface.md` section 5. The near plane is what sets depth precision and it
/// is far closer than the camera ever gets to a hull; the far plane has room for the whole play area from
/// the far end of the zoom.
inline constexpr float NEAR_CLIP_PLANE = 50.0f;
inline constexpr float FAR_CLIP_PLANE = 50000.0f;

/// How far the plane stretches at the top edge of the frame against its center, in camera heights.
///
/// THIS IS THE NUMBER THE PITCH FLOOR EXISTS TO BOUND. At the floor it is 5.67 against 1.73 --
/// a 3.3x stretch top to middle -- and it grows without bound as the top edge approaches the
/// horizontal, taking tap precision and ADR-010's wedge with it (`Interface.md` section 5). A test
/// asserts the pair, so that lowering the floor cannot be a quiet change.
[[nodiscard]] float GroundDistanceAtScreenTop(float _pitchRadians) noexcept;
[[nodiscard]] float GroundDistanceAtScreenCenter(float _pitchRadians) noexcept;

} // namespace Outpost
