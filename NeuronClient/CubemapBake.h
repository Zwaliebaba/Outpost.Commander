#pragma once

#include "GraphicsDevice.h"
#include "SceneTarget.h"

#include <cstdint>
#include <memory>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason `GraphicsDevice`'s binding is.
struct CubemapBakeBinding;

/// **THE GALAXY, BAKED ONCE AND SAMPLED WITH ONE FETCH FOREVER AFTER** (ADR-019).
///
/// Two things that belong together because they are two ends of one resource: this renders a band into
/// a 512-square cubemap at match start, and it draws that cubemap as the frame's backdrop. Splitting
/// them would put the texture's owner and its only reader in different files for no gain.
///
/// **IT KNOWS NOTHING ABOUT A SKY OR A GAME** (R9). The band's shape, orientation, palette and
/// luminance all arrive as twenty floats whose meaning is `GameClient/SkyLook.h`'s -- this class knows
/// that it renders six faces with a pixel shader and samples the result.
///
/// **WHY BAKE AT ALL.** The band carries four octaves of value noise for its dust lanes, which is far
/// too expensive to pay per pixel per frame -- and completely pointless to, because ADR-019 takes no
/// time input at all and the result would be identical every frame forever. The bake is 1.6 million
/// pixels once; the draw is one fetch each.
class CubemapBake
{
public:
  /// Per face. Six faces of 512 square at four bytes a texel is 6.3 MB, which is the figure ADR-019
  /// states and the reason the format below is a packed float rather than a 16-bit one.
  static constexpr std::uint32_t FACE_PIXELS = 512;

  static constexpr std::uint32_t FACE_COUNT = 6;

  /// Three `float4`: the face's right, up and forward in world space.
  static constexpr std::uint32_t FACE_CONSTANT_COUNT = 12;

  /// Five `float4`: pole, centre, shape, core, rim -- `Outpost::GalaxyConstants`' own layout.
  static constexpr std::uint32_t GALAXY_CONSTANT_COUNT = 20;

  /// Three `float4`: the camera's right and up already scaled by the field of view, and its forward.
  static constexpr std::uint32_t RAY_CONSTANT_COUNT = 12;

  CubemapBake() noexcept;
  ~CubemapBake() noexcept;

  CubemapBake(const CubemapBake&) = delete;
  CubemapBake& operator=(const CubemapBake&) = delete;
  CubemapBake(CubemapBake&&) = delete;
  CubemapBake& operator=(CubemapBake&&) = delete;

  /// The cubemap, its six render-target views, the view that samples it, and both pipeline states --
  /// the one that fills it and the one that draws it -- from ADR-012's checked-in DXIL.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;

  /// Whether `Bake` has run since `Create`. `DrawBackdrop` before this is true would sample an
  /// uninitialized texture, so it declines instead.
  [[nodiscard]] bool IsBaked() const noexcept;

  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// **SIX FACES IN ONE PASS**, recorded into the caller's command list. Call it once, at match start,
  /// BEFORE the scene target is bound -- it binds its own render targets and does not put the caller's
  /// back.
  ///
  /// _galaxy is `Outpost::GalaxyConstants` in its declared order.
  [[nodiscard]] bool Bake(const GraphicsDevice& _device, const float (&_galaxy)[GALAXY_CONSTANT_COUNT]) noexcept;

  /// The backdrop, into the currently bound scene target. **Drawn LAST, with the depth test on**, so
  /// it shades no pixel the fleet already covers.
  ///
  /// _ray is the camera's right and up axes already scaled by the tangent of half the vertical field
  /// of view (and the right axis by the aspect ratio), then its forward axis -- each padded to a
  /// `float4`. **The camera's POSITION is deliberately absent**: the sky rotates with the camera and
  /// does not translate with it (ADR-018, ADR-019).
  [[nodiscard]] bool DrawBackdrop(const GraphicsDevice& _device, const SceneTarget& _sceneTarget,
                                  const float (&_ray)[RAY_CONSTANT_COUNT]) noexcept;

private:
  std::shared_ptr<CubemapBakeBinding> m_binding;
};

} // namespace Neuron
