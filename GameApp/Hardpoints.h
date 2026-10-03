#pragma once

namespace Outpost
{
// What the game makes of a mesh's hardpoints (ADR-018). The engine carries each hardpoint's tag; what a tag means is
// decided here, and a model with a tag the game does not know fails to load.
enum class HardpointKind : std::uint8_t
{
  // Where a shot leaves from. Weapons are turrets (design §7), so only its position is used.
  Gun,
  // Where an engine's exhaust leaves, pointing the way the exhaust streams, its size the nozzle's radius (ADR-019).
  Exhaust
};

// The kind a tag names, or nothing for a tag the game does not know.
[[nodiscard]] std::optional<HardpointKind> HardpointKindOf(std::string_view _tag) noexcept;

// Where a model stands: a point on the ground, raised by liftMeters, rolled about its front by bankRadians, turned to a
// heading counterclockwise from +x seen from above, and scaled uniformly from the size its mesh was fitted to. A
// positive bank lowers the model's left side, its +z, which is into a counterclockwise turn (ADR-029). GameClient draws
// the model by PoseMatrix, which places it as PlacePoint does.
struct ModelPose
{
  PlanePosition position;
  float liftMeters = 0.0f;
  float headingRadians = 0.0f;
  float bankRadians = 0.0f;
  float scale = 1.0f;
};

// The world matrix that draws a fitted mesh where _pose puts it.
[[nodiscard]] DirectX::XMFLOAT4X4 PoseMatrix(const ModelPose& _pose) noexcept;

// A point of the fitted mesh, where the pose puts it in the world.
[[nodiscard]] DirectX::XMFLOAT3 PlacePoint(const ModelPose& _pose, const DirectX::XMFLOAT3& _point) noexcept;
// A direction of the fitted mesh, where the pose rolls and turns it.
[[nodiscard]] DirectX::XMFLOAT3 PlaceDirection(const ModelPose& _pose, const DirectX::XMFLOAT3& _direction) noexcept;

// Where a shot at _target leaves the model from: its gun nearest the target, on the ground. Nothing for a model with no
// gun. It is presentation only: the server measures range from the ship's center (design §7).
[[nodiscard]] std::optional<PlanePosition> NearestMuzzle(std::span<const Neuron::MeshHardpoint> _hardpoints, const ModelPose& _pose,
                                                         PlanePosition _target);

// The glows of a ship's exhausts (ADR-019): at each exhaust a bright core, and a plume streaming the way the exhaust
// points, which grows longer and brighter as the ship goes faster. _speedShare runs from 0 at rest to 1 at full speed.
void AddExhaustGlows(std::span<const Neuron::MeshHardpoint> _hardpoints, const ModelPose& _pose, const DirectX::XMFLOAT4& _color,
                     float _speedShare, std::vector<Neuron::GlowPipeline::Glow>& _glows);
} // namespace Outpost
