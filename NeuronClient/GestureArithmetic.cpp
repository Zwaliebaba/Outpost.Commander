#include "pch.h"

#include "GestureArithmetic.h"

#include <cmath>

namespace Neuron
{

namespace
{
/// Whether a cumulative value has left a symmetric deadzone around its rest value. Both deadzones
/// in `Interface.md` section 5 are this shape and the only difference is what rest means -- zero
/// degrees for rotation, a scale of one for zoom.
[[nodiscard]] bool OutsideDeadzone(float _value, float _rest, float _deadzone) noexcept
{
  return std::fabs(_value - _rest) > _deadzone;
}
} // namespace

AuthoredPoint AuthoredFromPhysical(const FitTransform& _interfaceFit, float _physicalXPixels, float _physicalYPixels) noexcept
{
  if ((_interfaceFit.width <= 0) || (_interfaceFit.height <= 0))
  {
    return AuthoredPoint{};
  }

  const float offsetX = static_cast<float>(_interfaceFit.offsetX);
  const float offsetY = static_cast<float>(_interfaceFit.offsetY);
  return AuthoredPoint{
    .xPixels = ((_physicalXPixels - offsetX) * static_cast<float>(INTERFACE_AUTHORED_WIDTH)) / static_cast<float>(_interfaceFit.width),
    .yPixels = ((_physicalYPixels - offsetY) * static_cast<float>(INTERFACE_AUTHORED_HEIGHT)) / static_cast<float>(_interfaceFit.height)};
}

float AuthoredLengthFromPhysical(const FitTransform& _interfaceFit, float _physicalLengthPixels) noexcept
{
  if (_interfaceFit.width <= 0)
  {
    return 0.0f;
  }

  return (_physicalLengthPixels * static_cast<float>(INTERFACE_AUTHORED_WIDTH)) / static_cast<float>(_interfaceFit.width);
}

AuthoredPoint AuthoredFromDips(const AuthoredSpace& _space, float _xDips, float _yDips) noexcept
{
  // THE CONVERSION R18 NAMES, and the whole reason these two numbers travel together: a CoreWindow
  // reports a position in device-independent pixels and everything downstream of the swap chain is
  // in physical ones. On the target device the factor is two.
  return AuthoredFromPhysical(_space.interfaceFit, _xDips * _space.rawPixelsPerViewPixel, _yDips * _space.rawPixelsPerViewPixel);
}

float AuthoredLengthFromDips(const AuthoredSpace& _space, float _lengthDips) noexcept
{
  return AuthoredLengthFromPhysical(_space.interfaceFit, _lengthDips * _space.rawPixelsPerViewPixel);
}

GatedManipulation ApplyGate(ManipulationGate& _gate, const InputEvent& _event) noexcept
{
  if (_event.kind == InputEventKind::ManipulationStarted)
  {
    // THE CONTACT COUNT IS LATCHED HERE AND NOWHERE ELSE. Everything after this reads the latched
    // value, so a third finger landing mid-drag reports two and the drag keeps doing what it was
    // doing (`Interface.md` section 2).
    _gate = ManipulationGate{.active = true, .contactCount = _event.contactCount};
    return GatedManipulation{.contactCount = _gate.contactCount};
  }

  const bool isManipulation =
    (_event.kind == InputEventKind::ManipulationUpdated) || (_event.kind == InputEventKind::ManipulationCompleted);
  if (!_gate.active || !isManipulation)
  {
    return GatedManipulation{};
  }

  GatedManipulation gated{.contactCount = _gate.contactCount};

  // THE TAP SLOP, measured on the cumulative translation and compared SQUARED, so there is no
  // square root in the path a finger drives every frame and no chance of one rounding a boundary
  // case the wrong way. `Interface.md` section 3 says exceed, so a travel of exactly 16 is a tap.
  const float travelX = _event.translationXAuthoredPixels;
  const float travelY = _event.translationYAuthoredPixels;
  if (!_gate.panEngaged && (((travelX * travelX) + (travelY * travelY)) > (TAP_SLOP_AUTHORED_PIXELS * TAP_SLOP_AUTHORED_PIXELS)))
  {
    // AND THE PAN BEGINS HERE RATHER THAN AT THE PRESS, which is the half of this rule that is
    // easy to leave out: without it the camera jumps by the whole slop the instant it engages, and
    // the 16 pixels the player already traveled are spent twice.
    _gate.panEngaged = true;
    _gate.panOriginXAuthoredPixels = travelX;
    _gate.panOriginYAuthoredPixels = travelY;
  }

  gated.isPanning = _gate.panEngaged;
  gated.panXAuthoredPixels = _gate.panEngaged ? (travelX - _gate.panOriginXAuthoredPixels) : 0.0f;
  gated.panYAuthoredPixels = _gate.panEngaged ? (travelY - _gate.panOriginYAuthoredPixels) : 0.0f;

  // THE ROTATION DEADZONE, AND ITS LATCH. Crossing back under eight degrees does not disengage it:
  // that is what stops the camera stuttering every time a two-finger pan wanders across the
  // threshold, and it is the difference between this and a plain comparison.
  _gate.rotationEngaged = _gate.rotationEngaged || OutsideDeadzone(_event.rotationDegrees, 0.0f, ROTATION_DEADZONE_DEGREES);
  gated.rotationDegrees = _gate.rotationEngaged ? _event.rotationDegrees : 0.0f;

  // THE SCALE DEADZONE, WHICH DOES NOT LATCH, and the asymmetry is deliberate rather than an
  // oversight: ADR-018 counts three constants here -- the eight degrees, its latch, and this two
  // per cent -- and gives zoom no fourth. Two per cent of scale is small enough that crossing back
  // and forth over it is not something a hand can feel, where eight degrees of heading is.
  gated.scale = OutsideDeadzone(_event.scale, 1.0f, SCALE_DEADZONE_FRACTION) ? _event.scale : 1.0f;

  if (_event.kind == InputEventKind::ManipulationCompleted)
  {
    _gate.active = false;
  }

  return gated;
}

} // namespace Neuron
