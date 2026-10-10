#pragma once

namespace Neuron
{
// A line on the screen, from one point to another in back-buffer pixels and widthPixels wide, which UiPipeline draws as
// one quad at any angle (ADR-015).
struct ScreenSegment
{
  DirectX::XMFLOAT2 from{};
  DirectX::XMFLOAT2 to{};
  float widthPixels = 1.0f;

  // The quad's corners, in the order UiPipeline draws a rectangle's: left of from, as one walks from from to to, then left
  // of to, right of to and right of from, each half the width out to the side. Its ends reach half the width past from and
  // to, so that segments meeting at a corner close it. None for a segment of no length.
  [[nodiscard]] std::optional<std::array<DirectX::XMFLOAT2, 4>> Corners() const noexcept;
};
} // namespace Neuron
