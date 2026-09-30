#include "pch.h"
#include "Camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
using Neuron::JsonBound;
using Neuron::JsonObjectReader;

// Looking along +z, the screen's right is +x and its top is +z, as a map is read.
constexpr float INITIAL_YAW_RADIANS = std::numbers::pi_v<float> / 2.0f;
// The near plane follows the distance, so that depth precision does too; the far plane reaches past the map's far
// corner from any focus inside the limit.
constexpr float NEAR_PLANE_SHARE_OF_DISTANCE = 0.01f;
constexpr float MINIMUM_NEAR_PLANE_METERS = 1.0f;
constexpr float FAR_PLANE_BEYOND_FOCUS_METERS = 4000.0f;

constexpr std::uint8_t KEY_W = 'W';
constexpr std::uint8_t KEY_A = 'A';
constexpr std::uint8_t KEY_S = 'S';
constexpr std::uint8_t KEY_D = 'D';
constexpr std::uint8_t KEY_Q = 'Q';
constexpr std::uint8_t KEY_E = 'E';

float Radians(float _degrees) noexcept
{
  return _degrees * std::numbers::pi_v<float> / 180.0f;
}

float ReadPositive(JsonObjectReader& _reader, std::string_view _name)
{
  return static_cast<float>(_reader.Number(_name, JsonBound::Positive));
}

// An angle above 0 and at most _maximum degrees.
float ReadAngle(JsonObjectReader& _reader, std::string_view _name, double _maximum)
{
  const double degrees = _reader.Number(_name, JsonBound::Positive);
  if (degrees > _maximum)
    Neuron::JsonFail(_reader.PathOf(_name), std::format("must be at most {} degrees, found {}", _maximum, degrees));
  return static_cast<float>(degrees);
}

// -1, 0 or 1 from a pair of keys.
float Axis(const Neuron::InputState& _input, std::uint8_t _positive, std::uint8_t _negative) noexcept
{
  return (_input.IsDown(_positive) ? 1.0f : 0.0f) - (_input.IsDown(_negative) ? 1.0f : 0.0f);
}
} // namespace

Outpost::CameraSettings Outpost::LoadCameraSettings(std::string_view _json)
{
  const Neuron::JsonValue document = Neuron::ParseJson(_json);
  JsonObjectReader reader(document, "");
  CameraSettings settings;
  settings.defaultViewWidthMeters = ReadPositive(reader, "defaultViewWidthMeters");
  settings.minimumViewWidthMeters = ReadPositive(reader, "minimumViewWidthMeters");
  settings.maximumViewWidthMeters = ReadPositive(reader, "maximumViewWidthMeters");
  settings.pitchAtMinimumDegrees = ReadAngle(reader, "pitchAtMinimumDegrees", 90.0);
  settings.pitchAtMaximumDegrees = ReadAngle(reader, "pitchAtMaximumDegrees", 90.0);
  settings.verticalFieldOfViewDegrees = ReadAngle(reader, "verticalFieldOfViewDegrees", 120.0);
  settings.focusLimitMeters = ReadPositive(reader, "focusLimitMeters");
  settings.panViewWidthsPerSecond = ReadPositive(reader, "panViewWidthsPerSecond");
  settings.edgeScrollMarginPixels = reader.Integer("edgeScrollMarginPixels", 0);
  settings.zoomFactorPerNotch = ReadPositive(reader, "zoomFactorPerNotch");
  settings.rotateDegreesPerSecond = ReadPositive(reader, "rotateDegreesPerSecond");
  reader.Finish();

  if (settings.minimumViewWidthMeters > settings.maximumViewWidthMeters)
    Neuron::JsonFail("minimumViewWidthMeters", "is wider than maximumViewWidthMeters");
  if (settings.defaultViewWidthMeters < settings.minimumViewWidthMeters ||
      settings.defaultViewWidthMeters > settings.maximumViewWidthMeters)
    Neuron::JsonFail("defaultViewWidthMeters", "is outside the minimum and maximum view widths");
  if (settings.zoomFactorPerNotch <= 1.0f)
    Neuron::JsonFail("zoomFactorPerNotch", "must be more than 1, or the wheel would not zoom");
  return settings;
}

Outpost::Camera::Camera(const CameraSettings& _settings) noexcept
  : m_settings(_settings),
    m_yawRadians(INITIAL_YAW_RADIANS),
    m_viewWidthMeters(_settings.defaultViewWidthMeters)
{
}

void Outpost::Camera::Update(const Neuron::InputState& _input, float _elapsedSeconds, std::uint32_t _viewportWidthPixels,
                             std::uint32_t _viewportHeightPixels) noexcept
{
  if (!_input.active || _viewportWidthPixels == 0 || _viewportHeightPixels == 0)
  {
    m_dragCursorPixels.reset();
    return;
  }

  // Middle-drag holds the ground under the cursor: the view moves the other way from the mouse.
  if (_input.IsDown(VK_MBUTTON))
  {
    if (m_dragCursorPixels)
    {
      const float metersPerPixel = m_viewWidthMeters / static_cast<float>(_viewportWidthPixels);
      const auto dxPixels = static_cast<float>(_input.cursorXPixels - m_dragCursorPixels->first);
      const auto dyPixels = static_cast<float>(_input.cursorYPixels - m_dragCursorPixels->second);
      Pan(-dxPixels * metersPerPixel, dyPixels * metersPerPixel);
    }
    m_dragCursorPixels = {_input.cursorXPixels, _input.cursorYPixels};
  }
  else
  {
    m_dragCursorPixels.reset();
  }

  float right = Axis(_input, KEY_D, KEY_A);
  float forward = Axis(_input, KEY_W, KEY_S);
  // Edge scroll needs the screen's edge to be the window's, which is so only while the cursor is held (ADR-012).
  if (_input.cursorClipped)
  {
    const std::int32_t margin = m_settings.edgeScrollMarginPixels;
    const auto width = static_cast<std::int32_t>(_viewportWidthPixels);
    const auto height = static_cast<std::int32_t>(_viewportHeightPixels);
    if (_input.cursorXPixels <= margin)
      right -= 1.0f;
    else if (_input.cursorXPixels >= width - 1 - margin)
      right += 1.0f;
    if (_input.cursorYPixels <= margin)
      forward += 1.0f;
    else if (_input.cursorYPixels >= height - 1 - margin)
      forward -= 1.0f;
  }
  const float panMeters = m_viewWidthMeters * m_settings.panViewWidthsPerSecond * _elapsedSeconds;
  Pan(std::clamp(right, -1.0f, 1.0f) * panMeters, std::clamp(forward, -1.0f, 1.0f) * panMeters);

  Rotate(Axis(_input, KEY_Q, KEY_E) * Radians(m_settings.rotateDegreesPerSecond) * _elapsedSeconds);
  Zoom(_input.wheelNotches);
}

void Outpost::Camera::Pan(float _rightMeters, float _forwardMeters) noexcept
{
  const DirectX::XMFLOAT2 right = GroundRight();
  const DirectX::XMFLOAT2 forward = GroundForward();
  const float limit = m_settings.focusLimitMeters;
  m_focusXMeters = std::clamp(m_focusXMeters + (right.x * _rightMeters) + (forward.x * _forwardMeters), -limit, limit);
  m_focusZMeters = std::clamp(m_focusZMeters + (right.y * _rightMeters) + (forward.y * _forwardMeters), -limit, limit);
}

void Outpost::Camera::Zoom(float _notches) noexcept
{
  m_viewWidthMeters = std::clamp(m_viewWidthMeters / std::pow(m_settings.zoomFactorPerNotch, _notches), m_settings.minimumViewWidthMeters,
                                 m_settings.maximumViewWidthMeters);
}

void Outpost::Camera::Rotate(float _radians) noexcept
{
  m_yawRadians = std::remainder(m_yawRadians + _radians, 2.0f * std::numbers::pi_v<float>);
}

float Outpost::Camera::PitchRadians() const noexcept
{
  const float range = m_settings.maximumViewWidthMeters - m_settings.minimumViewWidthMeters;
  const float along = range > 0.0f ? (m_viewWidthMeters - m_settings.minimumViewWidthMeters) / range : 0.0f;
  return Radians(std::lerp(m_settings.pitchAtMinimumDegrees, m_settings.pitchAtMaximumDegrees, along));
}

DirectX::XMFLOAT3 Outpost::Camera::EyePosition(float _aspectRatio) const noexcept
{
  const float distance = DistanceToFocus(_aspectRatio);
  const float pitch = PitchRadians();
  const DirectX::XMFLOAT2 forward = GroundForward();
  const float across = distance * std::cos(pitch);
  return {m_focusXMeters - (forward.x * across), distance * std::sin(pitch), m_focusZMeters - (forward.y * across)};
}

DirectX::XMFLOAT4X4 Outpost::Camera::ViewProjection(float _aspectRatio) const noexcept
{
  const float distance = DistanceToFocus(_aspectRatio);
  const DirectX::XMFLOAT3 eye = EyePosition(_aspectRatio);
  const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f),
                                                           DirectX::XMVectorSet(m_focusXMeters, 0.0f, m_focusZMeters, 1.0f),
                                                           DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
  const float nearMeters = std::max(MINIMUM_NEAR_PLANE_METERS, distance * NEAR_PLANE_SHARE_OF_DISTANCE);
  const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(Radians(m_settings.verticalFieldOfViewDegrees), _aspectRatio,
                                                                         nearMeters, distance + FAR_PLANE_BEYOND_FOCUS_METERS);
  DirectX::XMFLOAT4X4 result;
  DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixMultiply(view, projection));
  return result;
}

std::optional<DirectX::XMFLOAT2> Outpost::Camera::GroundPoint(float _screenX, float _screenY, float _aspectRatio) const noexcept
{
  const DirectX::XMFLOAT4X4 viewProjection = ViewProjection(_aspectRatio);
  const DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&viewProjection));
  DirectX::XMFLOAT3 nearPoint;
  DirectX::XMFLOAT3 farPoint;
  DirectX::XMStoreFloat3(&nearPoint, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(_screenX, _screenY, 0.0f, 1.0f), inverse));
  DirectX::XMStoreFloat3(&farPoint, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(_screenX, _screenY, 1.0f, 1.0f), inverse));

  const float dy = farPoint.y - nearPoint.y;
  if (dy >= 0.0f || nearPoint.y <= 0.0f)
    return std::nullopt;
  const float along = -nearPoint.y / dy;
  return DirectX::XMFLOAT2{nearPoint.x + ((farPoint.x - nearPoint.x) * along), nearPoint.z + ((farPoint.z - nearPoint.z) * along)};
}

DirectX::XMFLOAT2 Outpost::Camera::GroundForward() const noexcept
{
  return {std::cos(m_yawRadians), std::sin(m_yawRadians)};
}

DirectX::XMFLOAT2 Outpost::Camera::GroundRight() const noexcept
{
  // A quarter turn clockwise from forward, seen from above.
  return {std::sin(m_yawRadians), -std::cos(m_yawRadians)};
}

float Outpost::Camera::DistanceToFocus(float _aspectRatio) const noexcept
{
  // The ground across the middle of the screen is as far from the eye as the focus is, so the view width fixes the
  // distance through the horizontal field of view.
  const float halfVertical = Radians(m_settings.verticalFieldOfViewDegrees) / 2.0f;
  const float tanHalfHorizontal = std::tan(halfVertical) * _aspectRatio;
  return m_viewWidthMeters / (2.0f * tanHalfHorizontal);
}
