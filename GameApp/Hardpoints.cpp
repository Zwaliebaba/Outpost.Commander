#include "pch.h"
#include "Hardpoints.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr std::string_view GUN_TAG = "gun";
constexpr std::string_view EXHAUST_TAG = "exhaust";

// Placeholder looks for an exhaust, chosen to read from the RTS camera (design §11): presentation, not tuning, so they
// live here, as the combat effects' do. Sizes are in nozzle radii.
// The core sits a little out of the nozzle, so the hull does not hide half of it, and is as bright at rest as a ship
// that is waiting with its engines lit.
constexpr float CORE_OUT_RADII = 0.3f;
constexpr float CORE_RADII = 1.4f;
constexpr float CORE_RADII_AT_SPEED = 0.6f;
constexpr float CORE_BRIGHTNESS = 0.5f;
constexpr float CORE_BRIGHTNESS_AT_SPEED = 0.7f;
// The plume is a line of puffs streaming out of the nozzle: each further out, smaller and dimmer than the last. At rest
// it is a short stub, and at full speed it is several nozzles long.
constexpr int PLUME_PUFFS = 3;
constexpr float PLUME_FIRST_RADII = 1.2f;
constexpr float PLUME_SPACING_RADII = 1.3f;
constexpr float PLUME_LENGTH_AT_REST = 0.4f;
constexpr float PLUME_LENGTH_AT_SPEED = 1.6f;
constexpr float PUFF_RADII = 1.1f;
constexpr float PUFF_SHRINK_RADII = 0.25f;
constexpr float PUFF_BRIGHTNESS = 0.35f;
constexpr float PLUME_BRIGHTNESS_AT_REST = 0.3f;

DirectX::XMFLOAT4 Scaled(const DirectX::XMFLOAT4& _color, float _brightness) noexcept
{
  return {_color.x * _brightness, _color.y * _brightness, _color.z * _brightness, _color.w};
}
} // namespace

std::optional<Outpost::HardpointKind> Outpost::HardpointKindOf(std::string_view _tag) noexcept
{
  if (_tag == GUN_TAG)
    return HardpointKind::Gun;
  if (_tag == EXHAUST_TAG)
    return HardpointKind::Exhaust;
  return std::nullopt;
}

DirectX::XMFLOAT4X4 Outpost::PoseMatrix(const ModelPose& _pose) noexcept
{
  // XMMatrixRotationX lowers +z for a positive angle, which is the bank's sense. XMMatrixRotationY turns +x toward -z for
  // a positive angle, which is clockwise seen from above, so the heading goes in negated.
  const DirectX::XMMATRIX world = DirectX::XMMatrixScaling(_pose.scale, _pose.scale, _pose.scale) *
                                  DirectX::XMMatrixRotationX(_pose.bankRadians) * DirectX::XMMatrixRotationY(-_pose.headingRadians) *
                                  DirectX::XMMatrixTranslation(_pose.position.xMeters, _pose.liftMeters, _pose.position.zMeters);
  DirectX::XMFLOAT4X4 result;
  DirectX::XMStoreFloat4x4(&result, world);
  return result;
}

DirectX::XMFLOAT3 Outpost::PlaceDirection(const ModelPose& _pose, const DirectX::XMFLOAT3& _direction) noexcept
{
  // The bank rolls about the front, +x, lowering +z; then the heading turns +x toward +z, counterclockwise seen from
  // above. PoseMatrix does the same.
  const float bankCosine = std::cos(_pose.bankRadians);
  const float bankSine = std::sin(_pose.bankRadians);
  const float rolledY = (_direction.y * bankCosine) - (_direction.z * bankSine);
  const float rolledZ = (_direction.y * bankSine) + (_direction.z * bankCosine);
  const float cosine = std::cos(_pose.headingRadians);
  const float sine = std::sin(_pose.headingRadians);
  return {(_direction.x * cosine) - (rolledZ * sine), rolledY, (_direction.x * sine) + (rolledZ * cosine)};
}

DirectX::XMFLOAT3 Outpost::PlacePoint(const ModelPose& _pose, const DirectX::XMFLOAT3& _point) noexcept
{
  const DirectX::XMFLOAT3 turned = PlaceDirection(_pose, {_point.x * _pose.scale, _point.y * _pose.scale, _point.z * _pose.scale});
  return {turned.x + _pose.position.xMeters, turned.y + _pose.liftMeters, turned.z + _pose.position.zMeters};
}

std::optional<Outpost::PlanePosition> Outpost::NearestMuzzle(std::span<const Neuron::MeshHardpoint> _hardpoints, const ModelPose& _pose,
                                                             PlanePosition _target)
{
  std::optional<PlanePosition> nearest;
  float nearestMeters = std::numeric_limits<float>::max();
  for (const Neuron::MeshHardpoint& hardpoint : _hardpoints)
  {
    if (HardpointKindOf(hardpoint.tag) != HardpointKind::Gun)
      continue;
    const DirectX::XMFLOAT3 at = PlacePoint(_pose, hardpoint.position);
    const float meters = std::hypot(at.x - _target.xMeters, at.z - _target.zMeters);
    if (meters < nearestMeters)
    {
      nearestMeters = meters;
      nearest = PlanePosition{.xMeters = at.x, .zMeters = at.z};
    }
  }
  return nearest;
}

void Outpost::AddExhaustGlows(std::span<const Neuron::MeshHardpoint> _hardpoints, const ModelPose& _pose, const DirectX::XMFLOAT4& _color,
                              float _speedShare, std::vector<Neuron::GlowPipeline::Glow>& _glows)
{
  const float speed = std::clamp(_speedShare, 0.0f, 1.0f);
  for (const Neuron::MeshHardpoint& hardpoint : _hardpoints)
  {
    if (HardpointKindOf(hardpoint.tag) != HardpointKind::Exhaust)
      continue;
    const DirectX::XMFLOAT3 nozzle = PlacePoint(_pose, hardpoint.position);
    const DirectX::XMFLOAT3 stream = PlaceDirection(_pose, hardpoint.forward);
    const float radius = hardpoint.size * _pose.scale;
    const auto along = [&](float _radii) -> DirectX::XMFLOAT3
    { return {nozzle.x + (stream.x * radius * _radii), nozzle.y + (stream.y * radius * _radii), nozzle.z + (stream.z * radius * _radii)}; };

    _glows.push_back({.position = along(CORE_OUT_RADII),
                      .radiusMeters = radius * (CORE_RADII + (CORE_RADII_AT_SPEED * speed)),
                      .color = Scaled(_color, CORE_BRIGHTNESS + (CORE_BRIGHTNESS_AT_SPEED * speed))});
    const float length = PLUME_LENGTH_AT_REST + ((PLUME_LENGTH_AT_SPEED - PLUME_LENGTH_AT_REST) * speed);
    const float plumeBrightness = PLUME_BRIGHTNESS_AT_REST + ((1.0f - PLUME_BRIGHTNESS_AT_REST) * speed);
    for (int puff = 0; puff < PLUME_PUFFS; ++puff)
    {
      const auto step = static_cast<float>(puff);
      const float fade = 1.0f - (step / PLUME_PUFFS);
      _glows.push_back({.position = along((PLUME_FIRST_RADII + (PLUME_SPACING_RADII * step)) * length),
                        .radiusMeters = radius * (PUFF_RADII - (PUFF_SHRINK_RADII * step)),
                        .color = Scaled(_color, PUFF_BRIGHTNESS * fade * plumeBrightness)});
    }
  }
}
