#pragma once

#include "GlyphAtlas.h"
#include "GraphicsDevice.h"
#include "SwapChain.h"
#include "TextLayout.h"

#include <cstdint>
#include <memory>
#include <span>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason `GraphicsDevice`'s binding is.
struct TextRendererBinding;

/// **THE INTERFACE'S QUADS, INSTANCED, IN ONE DRAW** (M1.13, ADR-009, ADR-011).
///
/// Every plate, rule, bar and glyph the interface draws is a `GlyphQuad`, and this draws a list of them
/// with one `DrawInstanced` into whatever render target is bound -- which is the back buffer, after the
/// present step and before `Present`, exactly where `InterfacePass` draws and for the reason it does.
/// **THE ORDER OF THE LIST IS THE ORDER ON SCREEN**: there is no depth, so a scrim emitted after a panel
/// covers the panel's text, and a caller that wants a plate under its text emits the plate first.
///
/// **IT KNOWS NOTHING ABOUT A PANEL** (R9). What a credit balance looks like, and where, is
/// `GameClient`'s; this takes quads in back-buffer pixels and a texture to sample.
///
/// **A RING OF THREE INSTANCE BUFFERS**, for the reason `InstanceRing` gives: the list changes every
/// frame, and a single buffer would be rewritten while the graphics processor was still drawing the last
/// frame's copy. `Record` is called at most once a frame and steps the ring itself.
class TextRenderer
{
public:
  /// `InstanceRing`'s depth, and for the same reason: one more than the frames the device keeps in
  /// flight, so a buffer is never written while any submitted frame could still read it.
  static constexpr std::uint32_t FRAME_COUNT = 3;

  /// Quads a frame. The worst frame in the handoff -- four selection groups, the build panel, the quit
  /// confirm and a five-digit balance -- is about 350 quads; the headroom is for M2's cargo chips and
  /// M3's alerts rather than for anything that exists.
  static constexpr std::uint32_t MAXIMUM_QUADS = 2048;

  /// Four constants: the back buffer's size, and two unused.
  static constexpr std::uint32_t TARGET_CONSTANT_COUNT = 4;

  TextRenderer() noexcept;
  ~TextRenderer() noexcept;

  TextRenderer(const TextRenderer&) = delete;
  TextRenderer& operator=(const TextRenderer&) = delete;
  TextRenderer(TextRenderer&&) = delete;
  TextRenderer& operator=(TextRenderer&&) = delete;

  /// The root signature, the pipeline state from ADR-012's checked-in DXIL, and the instance ring. The
  /// back buffer's format is read off the swap chain rather than restated.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const SwapChain& _swapChain) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// Records the whole list as one instanced draw. A list longer than `MAXIMUM_QUADS` is **truncated at
  /// the end rather than written past it**, which loses whatever was emitted last -- the overlays -- and
  /// is why the capacity has the headroom it has. Returns how many quads were drawn.
  ///
  /// It resets the viewport and scissor to the whole back buffer, as `InterfacePass::Record` does, so an
  /// element can sit in a letterbox bar.
  [[nodiscard]] std::uint32_t Record(const GraphicsDevice& _device, const SwapChain& _swapChain, const GlyphAtlas& _atlas,
                                     std::span<const GlyphQuad> _quads) noexcept;

private:
  std::shared_ptr<TextRendererBinding> m_binding;
};

} // namespace Neuron
