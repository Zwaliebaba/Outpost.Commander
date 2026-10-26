#pragma once

namespace Outpost
{
// What the game makes of a mesh's hardpoints (ADR-018). The engine carries each hardpoint's tag; what a tag means is
// decided here, and a model with a tag the game does not know fails to load.
enum class HardpointKind : std::uint8_t
{
  // Where a shot leaves from. Weapons are turrets (Game §combat.turrets), so only its position is used.
  Gun,
  // Where an engine's exhaust leaves, pointing the way the exhaust streams, its size the nozzle's radius (ADR-019).
  Exhaust,
  // A light that blinks in its side's line color, as on a mast or an antenna, its size the lamp's radius (ADR-018).
  Beacon
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

// How a model on legs stands on uneven ground, such as a Mining Rig on its rock (owner, 2026-10-03, ADR-044): along its
// own axes and in meters, tilted about pivotMeters, the middle of its feet, its +z side lowered by bankRadians as a bank
// lowers it and then its front raised by pitchRadians, and lifted by liftMeters, before its pose turns and places it.
struct Stance
{
  DirectX::XMFLOAT3 pivotMeters{};
  float bankRadians = 0.0f;
  float pitchRadians = 0.0f;
  float liftMeters = 0.0f;
};

// The furthest a stance tilts a model either way, about 29 degrees.
inline constexpr float MAX_STANCE_TILT_RADIANS = 0.5f;

// The stance that stands a model on its feet. _feetMeters are its feet, in meters from its origin along its own axes, and
// _groundAt gives the ground's height under a point given the same way, or nothing where there is no ground. The model
// tilts to the plane that fits the ground under its feet best, fitted again as the tilt moves them, and then lifts until
// no foot is under the ground: at least one stands on it, and a model on three legs stands on all three. Nothing when no
// foot is over the ground.
[[nodiscard]] std::optional<Stance> StandOnFeet(std::span<const DirectX::XMFLOAT3> _feetMeters,
                                                const std::function<std::optional<float>(float, float)>& _groundAt);

// Where _stance tilts and lifts a point _meters from a model's origin, along the model's own axes.
[[nodiscard]] DirectX::XMFLOAT3 StandPoint(const Stance& _stance, const DirectX::XMFLOAT3& _meters) noexcept;

// The world matrix that draws a fitted mesh where _pose and _stance put it: scaled, stood, then turned, moved and raised by
// the pose, whose bank it does not use. It puts a point where StandPoint and then PlacePoint, at a scale of 1, put it.
[[nodiscard]] DirectX::XMFLOAT4X4 StanceMatrix(const ModelPose& _pose, const Stance& _stance) noexcept;

// Where a shot at _target leaves the model from: its gun nearest the target, on the ground. Nothing for a model with no
// gun. It is presentation only: the server measures range from the ship's center (ADR-014 decision 7). A shot of a further
// gun, such as a Command Station's second Defence gun (ADR-064 decision 11), leaves from the next nearest, _gun places on, and
// wraps round when the model has fewer guns than that.
[[nodiscard]] std::optional<PlanePosition> NearestMuzzle(std::span<const Neuron::MeshHardpoint> _hardpoints, const ModelPose& _pose,
                                                         PlanePosition _target, std::uint8_t _gun = 0);

// A beacon's blink (ADR-018 decision 9): lit for BEACON_ON_SECONDS of every BEACON_PERIOD_SECONDS, from a phase that its
// entity's identifier sets, so that an entity always blinks the same way and two whose identifiers are near do not blink
// together.
inline constexpr double BEACON_PERIOD_SECONDS = 1.6;
inline constexpr double BEACON_ON_SECONDS = 0.2;

// Whether _entity's beacons are lit _seconds into the view's clock.
[[nodiscard]] bool IsBeaconLit(EntityId _entity, double _seconds) noexcept;

// The glows of _entity's beacons, a model drawn by _world at _scale, _seconds into the view's clock: a glow in _color at
// each while they are lit, and none while the model is being built, _builtPermille short of PERMILLE.
void AddBeaconGlows(std::span<const Neuron::MeshHardpoint> _hardpoints, const DirectX::XMFLOAT4X4& _world, float _scale,
                    const DirectX::XMFLOAT4& _color, EntityId _entity, double _seconds, std::int32_t _builtPermille,
                    std::vector<Neuron::GlowPipeline::Glow>& _glows);

// The glows of a ship's exhausts (ADR-019): at each exhaust a bright core, and a plume streaming the way the exhaust
// points, which grows longer and brighter as the ship goes faster. _speedShare runs from 0 at rest to 1 at full speed.
void AddExhaustGlows(std::span<const Neuron::MeshHardpoint> _hardpoints, const ModelPose& _pose, const DirectX::XMFLOAT4& _color,
                     float _speedShare, std::vector<Neuron::GlowPipeline::Glow>& _glows);
} // namespace Outpost
