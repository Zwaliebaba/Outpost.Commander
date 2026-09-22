#pragma once

#include "GraphicsDevice.h"
#include "SceneTarget.h"

#include <cstdint>
#include <memory>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason `GraphicsDevice`'s binding is.
struct WorldPassBinding;

/// M0.21b's world pass: one shape per entity, into the SCENE TARGET, before the present blit.
///
/// **THIS IS THE PASS THE MILESTONE WAS MISSING** (`Plan/README.md` F10). M0.15 created the scene
/// target and blitted it to the back buffer; M0.17 drew a rectangle into the back buffer *after*
/// that blit. Nothing ever drew into the target itself, so the world was a clear color and M0.23
/// had no drawn position to time.
///
/// **WHERE IT SITS IS ADR-011's ORDERING AND IT IS NOT NEGOTIABLE.** The scene target is bound and
/// cleared, this draws into it at the world's resolution, the present step fits it into the back
/// buffer, and only then does the interface pass draw at physical resolution. Recording this after
/// the present step would put the world on top of the interface at the wrong scale.
///
/// **IT KNOWS NOTHING ABOUT AN ENTITY** (R9). A transform and a color arrive as root constants;
/// what a `Scout` is, which color a team has, and where a replica was interpolated to all live in
/// `GameClient`. This class could draw anything that fits in a matrix.
///
/// AT M0 THE SHAPE IS GENERATED FROM `SV_VertexID` and there is no vertex buffer, no index buffer
/// and no descriptor heap — the same arrangement `InterfacePass` has and for the same reason: a
/// step that exists to prove a transform should not also be proving a buffer upload. **M1.9
/// replaces the generated arrow with the CMO meshes**, and that is when the vertex buffer,
/// instancing and the light rig arrive.
class WorldPass
{
public:
  /// A row-major 4x4 for the vertex stage and a color for the pixel stage: twenty root constants,
  /// well inside the sixty-four a root signature may hold without a constant buffer.
  static constexpr std::uint32_t TRANSFORM_CONSTANT_COUNT = 16;
  static constexpr std::uint32_t COLOR_CONSTANT_COUNT = 4;

  /// The arrow is a strip of five: nose, port quarter, tail notch, starboard quarter, nose again.
  static constexpr std::uint32_t ARROW_VERTEX_COUNT = 5;

  WorldPass() noexcept;
  ~WorldPass() noexcept;

  WorldPass(const WorldPass&) = delete;
  WorldPass& operator=(const WorldPass&) = delete;
  WorldPass(WorldPass&&) = delete;
  WorldPass& operator=(WorldPass&&) = delete;

  /// The root signature and the pipeline state, from ADR-012's checked-in DXIL.
  ///
  /// **The target's formats and sample count are read off `SceneTarget` rather than restated.** A
  /// pipeline state has to agree with what it renders into, and a pass that states the format
  /// itself is the second place it can drift — which would surface as a device-removal on a machine
  /// where the debug layer is off.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;

  /// The HRESULT of the last call that failed, or zero.
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// Records one shape into WHATEVER RENDER TARGET IS ALREADY BOUND, which `SceneTarget::RecordClear`
  /// has just bound along with the depth buffer. Same arrangement as `PresentStep` and
  /// `InterfacePass`: the caller owns the ordering, because the ordering is the design's.
  ///
  /// _worldViewProjection is row-major and already multiplied out — world times view times
  /// projection. `GameClient/Camera.h` produces it and the shader is declared `row_major` to match,
  /// so nothing transposes anything at runtime.
  ///
  /// **It sets the viewport to the scene target's full extent**, because the present step and the
  /// interface pass both leave it somewhere else and a frame draws them in that order.
  [[nodiscard]] bool Record(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, const float (&_worldViewProjection)[16],
                            float _red, float _green, float _blue, float _alpha) noexcept;

private:
  std::shared_ptr<WorldPassBinding> m_binding;
};

} // namespace Neuron
