#pragma once

#include "GraphicsDevice.h"
#include "MeshBuffer.h"
#include "SceneTarget.h"

#include <cstdint>
#include <memory>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason `GraphicsDevice`'s binding is.
struct MeshPassBinding;

/// M1.9's ship pass: **one instanced draw per mesh**, into the scene target, before the present blit.
///
/// **IT KNOWS NOTHING ABOUT AN ENTITY** (R9). A mesh, a list of instances, a transform and a light
/// rig arrive; what a `Scout` is, which colour a team has and where a replica was interpolated to
/// all live in `GameClient`. This class could draw anything with that vertex layout.
///
/// **IT DOES NOT REPLACE `WorldPass`.** That one draws M0.21b's generated arrow from `SV_VertexID`,
/// which is still what a design with no authored mesh gets -- the `Cruiser` today, and anything M4
/// adds before its geometry does.
///
/// **TEAM COLOUR IS PER INSTANCE AND NOT PER DRAW**, which is what lets one call cover every ship of
/// a shape regardless of owner. A mesh that wanted a second material for its team would cost a draw
/// call per owner, and that is a trade to argue rather than take.
class MeshPass
{
public:
  /// A row-major 4x4 for the vertex stage: sixteen root constants.
  static constexpr std::uint32_t TRANSFORM_CONSTANT_COUNT = 16;

  /// Six `float4` for the pixel stage: the three hull stops, the key and fill lights, the ambient.
  /// Twenty-four constants, so the two together are forty of the sixty-four a root signature holds
  /// without a constant buffer.
  static constexpr std::uint32_t LOOK_CONSTANT_COUNT = 24;

  /// What the pixel stage is told about the look. **The layout IS the `cbuffer`'s** -- six `float4`
  /// in order -- and the two must not disagree, which is why this is a struct rather than a span.
  ///
  /// R8: a public aggregate.
  struct Look
  {
    float hullDeep[4]{};
    float hullBase[4]{};
    float hullEdge[4]{};

    /// xyz toward the light, w the intensity.
    float keyLight[4]{};
    float fillLight[4]{};

    /// xyz the colour, w the intensity. Multiplied by albedo, never added flat.
    float ambient[4]{};
  };

  MeshPass() noexcept;
  ~MeshPass() noexcept;

  MeshPass(const MeshPass&) = delete;
  MeshPass& operator=(const MeshPass&) = delete;
  MeshPass(MeshPass&&) = delete;
  MeshPass& operator=(MeshPass&&) = delete;

  /// The root signature, the input layout and the pipeline state, from ADR-012's checked-in DXIL.
  ///
  /// The target's formats and sample count are read off `SceneTarget` rather than restated, for the
  /// reason `WorldPass::Create` gives.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// Binds the pass and its constants. Called once a frame, before the per-mesh draws.
  ///
  /// _viewProjection is row-major and already multiplied out. **It is the VIEW-PROJECTION and not a
  /// world transform**: the per-instance position and heading are what place each ship, so the
  /// matrix is the same for every one of them and is sent once.
  [[nodiscard]] bool Begin(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, const float (&_viewProjection)[16],
                           const Look& _look) noexcept;

  /// One mesh, _instanceCount times, from the instance buffer at _instanceAddress.
  ///
  /// **THE INSTANCE BUFFER IS THE CALLER'S RING**, indexed by the frame in flight: a single buffer
  /// would be written while the graphics processor was still reading the last frame's copy, which
  /// draws one frame of ships at another frame's positions, intermittently.
  [[nodiscard]] bool Draw(const GraphicsDevice& _device, const MeshBuffer& _mesh, std::uint64_t _instanceAddress,
                          std::uint32_t _instanceBytes, std::uint32_t _instanceCount) noexcept;

private:
  std::shared_ptr<MeshPassBinding> m_binding;
};

} // namespace Neuron
