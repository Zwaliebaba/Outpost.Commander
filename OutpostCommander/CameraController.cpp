#include "pch.h"

#include "CameraController.h"

#include <cstdint>

namespace Outpost
{

namespace
{

// The Species free camera's rates (SpeciesLook.md §7): 250 units a second, four times that with
// the speed-up key.
constexpr float MOVE_RATE = 250.0f;
constexpr float FAST_MULTIPLIER = 4.0f;
constexpr float WHEEL_HEIGHT_PER_DETENT = 50.0f;
constexpr std::uint32_t EDGE_PIXELS = 8;

// Virtual-key codes. NO LETTERS: Design/Interface.md §7's hotkey table gives the letters to orders,
// and two of the camera's collided with it - S was Stop as well as backward, R was Attack-move as
// well as pitch down. The owner ruled on 2026-09-19 that the orders win, because §7 is the document
// that specifies the game's controls and the camera's letters were M0 placeholders from
// SpeciesLook.md §7 that predate any order existing. Nothing is lost but a second way to do the
// same thing: the camera still has the arrow keys, the screen's edges, the wheel, PageUp and
// PageDown, and - in aim mode - the mouse itself, which covers turning it by key.
constexpr std::uint8_t KEY_SHIFT = 0x10;
constexpr std::uint8_t KEY_PAGE_UP = 0x21;
constexpr std::uint8_t KEY_PAGE_DOWN = 0x22;
constexpr std::uint8_t KEY_LEFT = 0x25;
constexpr std::uint8_t KEY_UP = 0x26;
constexpr std::uint8_t KEY_RIGHT = 0x27;
constexpr std::uint8_t KEY_DOWN = 0x28;

[[nodiscard]] float Axis(const Neuron::FrameInput& _input, std::uint8_t _positive, std::uint8_t _negative) noexcept
{
  return (_input.keysHeld[_positive] ? 1.0f : 0.0f) - (_input.keysHeld[_negative] ? 1.0f : 0.0f);
}

} // namespace

void CameraController::Advance(Neuron::Camera& _camera, const Neuron::FrameInput& _input, Neuron::PointerMode _mode,
                               const Neuron::HeightView& _terrain, float _seconds, std::uint32_t _clientWidth,
                               std::uint32_t _clientHeight) const
{
  // THE ARROWS AND THE EDGES ARE POINT MODE'S (§4's table). An aimed pointer is hidden and warped
  // nowhere, so "the mouse is at the edge of the screen" is not a thing that can be true of it -
  // reading the edges in aim mode would scroll the map from wherever the pointer happened to be
  // left when the mode changed, which is a camera that drifts on its own.
  const bool pointing = _mode == Neuron::PointerMode::Point;
  float forward = pointing ? Axis(_input, KEY_UP, KEY_DOWN) : 0.0f;
  float right = pointing ? Axis(_input, KEY_RIGHT, KEY_LEFT) : 0.0f;
  // The screen's edges, when the mouse is inside the client area at all.
  if (pointing && _clientWidth > 0 && _clientHeight > 0 && _input.mouseX >= 0 && _input.mouseY >= 0 &&
      static_cast<std::uint32_t>(_input.mouseX) < _clientWidth && static_cast<std::uint32_t>(_input.mouseY) < _clientHeight)
  {
    if (static_cast<std::uint32_t>(_input.mouseX) < EDGE_PIXELS)
    {
      right -= 1.0f;
    }
    if (static_cast<std::uint32_t>(_input.mouseX) >= _clientWidth - EDGE_PIXELS)
    {
      right += 1.0f;
    }
    if (static_cast<std::uint32_t>(_input.mouseY) < EDGE_PIXELS)
    {
      forward += 1.0f;
    }
    if (static_cast<std::uint32_t>(_input.mouseY) >= _clientHeight - EDGE_PIXELS)
    {
      forward -= 1.0f;
    }
  }
  const float rate = MOVE_RATE * (_input.keysHeld[KEY_SHIFT] ? FAST_MULTIPLIER : 1.0f) * _seconds;
  const float up = (Axis(_input, KEY_PAGE_UP, KEY_PAGE_DOWN) * rate) + static_cast<float>(_input.wheelDetents) * WHEEL_HEIGHT_PER_DETENT;
  _camera.Move(forward * rate, right * rate, up);

  // AIM MODE TURNS THE CAMERA WITH NO BUTTON HELD (§4). The signs are NeuronClient/PointerMode.h's, and
  // they are there rather than here because they are exactly the thing that ships inverted: both
  // compile either way and neither is noticed until somebody plays it.
  Neuron::Aim aim{_camera.Yaw(), _camera.Pitch()};
  if (!pointing)
  {
    const Neuron::MouseTravel travel = Neuron::TravelOf(_input);
    aim = Neuron::AimedBy(aim, travel.x, travel.y);
  }
  _camera.SetOrientation(aim.yawRadians, aim.pitchRadians);
  _camera.ClampHeight(_terrain);
}

} // namespace Outpost
