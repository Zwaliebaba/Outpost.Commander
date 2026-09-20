#pragma once

#include <cstdint>

// The pointer's two modes and the two pieces of arithmetic around them that are easy to get
// backwards (Design/Interface.md §4 and §12 ruling 3, rewritten by the owner on 2026-09-19;
// m1-vertical-slice/G1b).
//
// WHY THIS IS A LIBRARY AND NOT A FEW LINES IN THE EXECUTABLE. OutpostCommander is an Application,
// so nothing in it can be linked into a test DLL - which is why m1-vertical-slice/G1b's selection
// and order input live in Replica. The same argument reaches these two: the SIGN of an aim delta
// and the ORDER Escape peels in are exactly the things that ship inverted and are never noticed by
// a compiler. Everything else the pointer's mode decides - which cursor is drawn, whether the drag
// rectangle is live - is a branch in the drawing code with nothing to get wrong.
//
// IT IS IN Neuron AND NOT IN Outpost, because nothing here knows a game concept: a mode, a mouse
// count and a camera angle (AGENTS.md R9). What an armed order IS remains the game's; this is only
// told whether there is one.

namespace Neuron
{

/// How far one raw mouse count turns the camera in aim mode, in radians (Design/Interface.md §4's
/// AIM_RADIANS_PER_COUNT).
///
/// SPECIES'S OWN EDITOR RATE, and not the delta camera M0 shipped at 0.003. SpeciesLook.md §7 puts
/// its editor camera at 0.005 radians a pixel, and §12 ruling 3 takes that camera rather than
/// Species's playing one - which has no yaw and no pitch at all, and turns by ray-casting a virtual
/// cursor onto the terrain and warping the operating system's pointer back to it every frame.
inline constexpr float AIM_RADIANS_PER_COUNT = 0.005f;

/// Which of the two the pointer is in. AIM is the default, which is Species's arrangement: the
/// mouse is connected to the camera for as long as a location is being played.
enum class PointerMode : std::uint8_t
{
  Aim,  ///< The mouse turns the camera, Windows' pointer is hidden, and the ray goes through the screen's CENTRE
  Point ///< An ordinary pointer, the camera does not turn, and the ray goes through the pointer
};

[[nodiscard]] constexpr PointerMode Swapped(PointerMode _mode) noexcept
{
  return _mode == PointerMode::Aim ? PointerMode::Point : PointerMode::Aim;
}

/// What one press of Escape does. "Escape is peeled, not swallowed" (Design/Interface.md §7): it
/// cancels the innermost thing that is open, ONE PRESS AT A TIME, and only when there is nothing
/// left to cancel does it swap the pointer's mode.
///
/// THE ORDER IS THE WHOLE OF IT. A commander who has armed a Build and opened a panel expects the
/// first Escape to disarm, not to release his mouse and leave the order armed under a pointer that
/// has stopped aiming. Swapping the mode is the LAST resort and never happens on the same press as
/// a cancel.
enum class EscapePeel : std::uint8_t
{
  ArmedOrder, ///< Something is armed: disarm it, and nothing else
  ModalPanel, ///< Nothing armed but a modal panel is open: close it
  SwapPointer ///< Nothing to cancel: aim becomes point, point becomes aim
};

[[nodiscard]] constexpr EscapePeel PeelOf(bool _orderArmed, bool _modalPanelOpen) noexcept
{
  if (_orderArmed)
  {
    return EscapePeel::ArmedOrder;
  }
  return _modalPanelOpen ? EscapePeel::ModalPanel : EscapePeel::SwapPointer;
}

/// A camera orientation, for the aim below.
struct Aim
{
  float yawRadians = 0.0f;
  float pitchRadians = 0.0f;

  [[nodiscard]] constexpr bool operator==(const Aim&) const noexcept = default;
};

/// The orientation after a frame's worth of raw mouse travel in aim mode.
///
/// THE SIGNS ARE THE POINT OF THIS FUNCTION. Moving the mouse RIGHT turns the camera right, so yaw
/// gains the horizontal count; moving it FORWARD (a negative y count, because the screen's y grows
/// downward) raises the view, so pitch LOSES the vertical count. Both are one character away from
/// an inverted camera, both compile either way, and neither is noticed until somebody plays it.
/// NeuronClient/Camera.h clamps the pitch, so nothing here does.
[[nodiscard]] Aim AimedBy(const Aim& _aim, std::int32_t _countsX, std::int32_t _countsY) noexcept;

} // namespace Neuron
