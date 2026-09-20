#pragma once

#include "Camera.h"
#include "FrameInput.h"
#include "HeightView.h"
#include "PointerMode.h"

#include <cstdint>

namespace Outpost
{

/// Flies the camera from the frame's input (TechnicalDesign.md §6.5; SpeciesLook.md §7): the wheel
/// and PageUp/PageDown change its height in either pointer mode, the arrow keys and the screen's
/// edges move it along the ground in POINT mode, the mouse turns it in AIM mode, and the floor
/// keeps it out of the terrain.
///
/// THE TWO MODES SPLIT IT (Design/Interface.md §4's table, the owner's ruling 3 of 2026-09-19).
/// In aim the mouse is connected to the camera with no button held, which is Species's own
/// arrangement, and there is no pointer to put against a screen edge; in point there is a pointer,
/// it does not turn the camera, and the arrows and the edges are how the commander travels. The
/// wheel and the page keys are in both, because §4's table lists them without a mode.
///
/// NO LETTER KEYS. Design/Interface.md §7's hotkey table gives the letters to orders and two of the
/// camera's collided with it: S was Stop as well as backward, R was Attack-move as well as pitch
/// down. The owner ruled on 2026-09-19 that the orders win - §7 specifies the game's controls and
/// these were M0 placeholders that predate any order existing - so W, A, S, D, Q, E, R and F are
/// the commander's, and the camera keeps every other way it already had of doing the same things.
///
/// AIMING IS ON NO BUTTON AT ALL. M0 bound it to the right button because no orders existed yet,
/// and this task moved it to the middle one for a day; the owner's ruling of 2026-09-19 took it off
/// the buttons altogether, because Species connects the mouse to the camera for as long as a
/// location is being played and Escape is what releases it. The alternative Design/Interface.md
/// considered for M0's problem, a drag threshold on the right button, delays every order by however
/// long the threshold takes to fail.
class CameraController
{
public:
  void Advance(Neuron::Camera& _camera, const Neuron::FrameInput& _input, Neuron::PointerMode _mode, const Neuron::HeightView& _terrain,
               float _seconds, std::uint32_t _clientWidth, std::uint32_t _clientHeight) const;
};

} // namespace Outpost
