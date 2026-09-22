#pragma once

#include "GraphicsDevice.h"
#include "SceneTarget.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason `GraphicsDevice`'s binding is.
struct PointSpritesBinding;

/// One star as the graphics processor wants it. **The layout IS the vertex shader's input** and the
/// two must not disagree, which is why this is a struct with a fixed size rather than a span somebody
/// fills.
///
/// Thirty-two bytes, which is a round number by construction: the padding is there to make it one
/// rather than because anything needs the field. Three thousand stars is 94 KiB, uploaded once.
///
/// R8: a public aggregate.
struct StarInstance
{
  /// World space, on the unit sphere.
  float direction[3]{0.0f, 0.0f, 1.0f};

  /// In scene-target pixels, so the world resolution reaches it (ADR-016).
  float sizePixels = 1.5f;

  /// Already desaturated and already scaled by its tier's value, so the pixel stage multiplies by a
  /// falloff and nothing else.
  float colour[3]{1.0f, 1.0f, 1.0f};

  float padding = 0.0f;
};

static_assert(sizeof(StarInstance) == 32, "The star instance layout is the vertex shader's input and is padded to 32 bytes");

/// **AN INSTANCED SPRITE DRAW, AND IT KNOWS NOTHING ABOUT A SKY** (R9). A list of directions, sizes and
/// colours arrives and is drawn as camera-facing quads at infinity; that these are stars, how many
/// there are and what colours they took all live in `GameClient/SkyLook.h` and `Neuron::StarField`.
///
/// **THERE IS NO RING HERE, AND THAT IS A CONSEQUENCE RATHER THAN AN OVERSIGHT.** `MeshPass` writes its
/// instances through `InstanceRing` because the fleet moves every frame and a single buffer would be
/// rewritten while the graphics processor was still reading the last frame's copy. ADR-019's sky is
/// generated once and never updated -- it takes no time input at all -- so the buffer is written once
/// at match start and read unchanged forever after. If anything ever animates the sky, this becomes
/// wrong in exactly the way `MeshPass` documents, and ADR-019 has to be reopened first.
class PointSprites
{
public:
  /// A row-major 4x4 plus one `float4`: the view-rotation-projection and the target's size.
  static constexpr std::uint32_t SKY_CONSTANT_COUNT = 20;

  /// Four corners as a triangle strip, so the quad is two bits of `SV_VertexID` and there is no
  /// lookup table.
  static constexpr std::uint32_t VERTICES_PER_SPRITE = 4;

  PointSprites() noexcept;
  ~PointSprites() noexcept;

  PointSprites(const PointSprites&) = delete;
  PointSprites& operator=(const PointSprites&) = delete;
  PointSprites(PointSprites&&) = delete;
  PointSprites& operator=(PointSprites&&) = delete;

  /// The root signature and the pipeline state, from ADR-012's checked-in DXIL.
  ///
  /// The target's formats and sample count are read off `SceneTarget` rather than restated, for the
  /// reason `WorldPass::Create` gives.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// The whole field, once. Allocates a buffer of exactly this size and copies into it; calling it
  /// again reallocates, which is correct for a resolution change and wasteful in a frame loop.
  [[nodiscard]] bool Upload(const GraphicsDevice& _device, const std::vector<StarInstance>& _stars) noexcept;

  [[nodiscard]] std::uint32_t StarCount() const noexcept;

  /// One call for the whole sky.
  ///
  /// _viewRotationProjection is row-major, already multiplied out, and has **the translation
  /// removed**: the sky rotates with the camera and does not translate with it (ADR-018, ADR-019), so
  /// passing a full view-projection here would slide the stars with the pan.
  [[nodiscard]] bool Draw(const GraphicsDevice& _device, const SceneTarget& _sceneTarget,
                          const float (&_viewRotationProjection)[16]) noexcept;

private:
  std::shared_ptr<PointSpritesBinding> m_binding;
};

} // namespace Neuron
