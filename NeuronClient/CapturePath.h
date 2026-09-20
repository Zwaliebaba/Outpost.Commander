#pragma once

#include "Lighting.h"
#include "RenderView.h"

#include <cstdint>
#include <span>

// Where the capture's camera looks, tick by tick (TechnicalDesign.md §6.1; m1-vertical-slice/G2).
// The capture writes a frame every hundred ticks of a scripted match, and those frames are the only
// look anybody gets at this game, so where the camera is pointed is not a detail of the run - it is
// the whole of what the artefact says.
//
// NO DIRECT3D AND NO DirectXMath HERE, for the reason NeuronClient/GroundRay.h gives: everything below is
// arithmetic over a script and a list of instances, so a camera pointed at the wrong valley is a
// failing assertion on any machine rather than ninety BMPs nobody can rerun. What the capture keeps
// is the device, the passes and the ground height under the aim point.
//
// A SCRIPT AND NOT A FORMULA. M0's capture computed its pose from the frame number, which was right
// for an empty island and cannot say "and here is where the two commanders meet". A script is a
// handful of vantages by tick, each a statement of intent that reads as one, interpolated between;
// changing where the capture looks is changing a table rather than reading an expression backwards.
//
// AND IT FOLLOWS THE FIGHTING, because the script cannot know where that is. Measured on the slice
// landscape (m1-vertical-slice/S14, seed 1): the two commanders start at cells (36, 92) and
// (108, 20) and their first fight is around cell (58, 80) - which is neither start, nor the middle,
// nor any fraction of the line between them, because an army walks around terrain rather than
// through it. So a vantage may say that it FOLLOWS, and then its aim is wherever the shots this
// commander can see are, at the distance and the elevation the script asked for.

namespace Neuron
{

/// Where the camera is and what it looks at, in world units.
struct CapturePose
{
  float eyeX = 0.0f;
  float eyeY = 0.0f;
  float eyeZ = 0.0f;
  float atX = 0.0f;
  float atY = 0.0f;
  float atZ = 0.0f;
};

/// One vantage of the script, in force from its tick until the next vantage's, and interpolated
/// into it so that the camera travels rather than cuts.
///
/// THE AIM IS A POINT ON THE GROUND AND THE EYE IS RELATIVE TO IT. A vantage that named the eye in
/// world units would have to know how high the ground is under it, which is the landscape's answer
/// and not a script's; naming a bearing, a setback and an elevation makes one vantage read the same
/// on a beach and on a ridge, and makes "over the base, from the south, three hundred units up" a
/// sentence rather than three coordinates.
struct CaptureVantage
{
  /// The first tick this vantage is in force. The script is ascending in this field.
  std::uint32_t tick = 0;
  float aimX = 0.0f; ///< World units. The point on the ground the camera looks at.
  float aimZ = 0.0f;
  /// Where the eye sits round the aim point, from due -Z toward +X, in radians. INTERPOLATED AS A
  /// NUMBER AND NOT AS AN ANGLE: it does not wrap, so a sweep of more than half a turn is written
  /// as two vantages rather than left to guess which way round the camera goes.
  float bearingRadians = 0.0f;
  float setbackWorldUnits = 0.0f;   ///< How far back along the bearing the eye sits.
  float elevationWorldUnits = 0.0f; ///< How high above the ground at the aim point the eye sits.
  /// Whether this vantage looks at the fighting when there is any this commander can see. The
  /// opening sweep of the landscape does not; the vantages that exist to show combat do.
  bool follow = false;
  /// Not interpolated - a frame is fogged one way or the other. ADR-005 rests on a pair of frames
  /// drawn from one vantage under the two modes, and the capture writes that pair on every run.
  FogMode fog = FogMode::Desaturation;
};

/// The vantage in force at a tick, interpolated into the next. An empty script gives a default
/// vantage; a tick before the first gives the first, and after the last gives the last.
[[nodiscard]] CaptureVantage VantageAt(std::span<const CaptureVantage> _script, std::uint32_t _tick) noexcept;

/// The pose a vantage gives, once the caller has read the height of the ground under its aim point.
[[nodiscard]] CapturePose PoseOf(const CaptureVantage& _vantage, float _groundHeightWorldUnits) noexcept;

/// The middle of what is being shot at: the mean of the projectiles in a view, which is where the
/// fighting this commander can SEE is - a shot fired outside his fog is not in his view at all.
/// False, and the outputs untouched, when nothing is in the air.
[[nodiscard]] bool ActionCenter(std::span<const RenderInstance> _instances, float& _outX, float& _outZ) noexcept;

} // namespace Neuron
