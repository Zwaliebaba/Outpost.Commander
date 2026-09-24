#pragma once

#include "GraphicsDevice.h"
#include "SceneTarget.h"

#include <cstdint>
#include <memory>
#include <span>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason `GraphicsDevice`'s binding is.
struct BeamPassBinding;

/// One beam as the graphics processor wants it. **The layout IS the vertex shader's input**, as
/// `StarInstance`'s is: two world points, a width in world units, and an already-lit color.
///
/// R8: a public aggregate.
struct BeamInstance
{
  float from[3]{0.0f, 0.0f, 0.0f};
  float widthUnits = 1.0f;
  float to[3]{0.0f, 0.0f, 0.0f};
  float padding = 0.0f;

  /// Already scaled by the beam's brightness: the pass blends additively.
  float color[3]{1.0f, 1.0f, 1.0f};
  float padding2 = 0.0f;
};

static_assert(sizeof(BeamInstance) == 48, "The beam instance layout is the vertex shader's input and is padded to 48 bytes");

/// **AN INSTANCED BEAM DRAW, AND IT KNOWS NOTHING ABOUT A WEAPON OR A MINER** (R9). A list of point pairs arrives
/// and is drawn as soft-edged quads lying on the plane, into the scene target, blended additively and tested
/// against the hulls' depth without writing it -- so a hull in front hides a beam and two beams that cross add.
/// What the beams are, and what color they take, is `GameClient/Beams.h`'s.
///
/// **A RING OF UPLOAD BUFFERS, ONE A FRAME IN FLIGHT**, because beams change every frame: a single buffer would be
/// rewritten while the graphics processor was still reading the last frame's copy, which is `MeshPass`'s reason
/// for its `InstanceRing` too.
class BeamPass
{
public:
  /// A row-major 4x4: the view-projection.
  static constexpr std::uint32_t CONSTANT_COUNT = 16;

  /// Four corners as a triangle strip.
  static constexpr std::uint32_t VERTICES_PER_BEAM = 4;

  /// The swap chain's frames, as `InstanceRing` counts them.
  static constexpr std::uint32_t FRAME_COUNT = 3;

  BeamPass() noexcept;
  ~BeamPass() noexcept;

  BeamPass(const BeamPass&) = delete;
  BeamPass& operator=(const BeamPass&) = delete;
  BeamPass(BeamPass&&) = delete;
  BeamPass& operator=(BeamPass&&) = delete;

  /// The root signature, the pipeline state from ADR-012's checked-in DXIL, and the ring at _capacity beams a
  /// frame.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, std::uint32_t _capacity) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// Writes this frame's beams into the ring's slot for _frameIndex and draws them in one call. Beams past the
  /// capacity are dropped rather than written past the end. False when there was nothing to draw or no device.
  [[nodiscard]] bool Draw(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, std::uint32_t _frameIndex,
                          const float (&_viewProjection)[16], std::span<const BeamInstance> _beams) noexcept;

private:
  std::shared_ptr<BeamPassBinding> m_binding;
};

} // namespace Neuron
