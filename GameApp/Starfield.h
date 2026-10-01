#pragma once

namespace Outpost
{
// The galaxy's place on the sky (ADR-021): its plane is the great circle round the pole, and its center is the bulge
// in the middle of the band. Both are unit vectors in the world, at right angles to each other.
struct GalacticFrame
{
  DirectX::XMFLOAT3 center;
  DirectX::XMFLOAT3 pole;
};

[[nodiscard]] GalacticFrame StarfieldGalaxy() noexcept;

// The sky behind the battlefield (ADR-021): stars scattered over the whole sky, and a Milky Way band across the default
// view made of stars alone, with a bulge at its center, clouds where the stars crowd and dust lanes where they thin out.
// The stars come from a fixed seed, so the sky is the same every time. Their spreads are in pixels on the 1920×1080
// reference frame (ADR-006), so the sky looks the same at any resolution.
[[nodiscard]] std::vector<Neuron::StarPipeline::Star> BuildStarfield();
} // namespace Outpost
