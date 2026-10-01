#pragma once

namespace Outpost
{
// The sky behind the battlefield (ADR-022): stars spread evenly over the whole sky, with the spread of light and color
// that stars at every distance have. The brightest few are drawn with the starburst sprite, the rest as points. The
// stars come from a fixed seed, so the sky is the same every time. Their radii are in pixels on the 1920×1080 reference
// frame (ADR-006), so the sky looks the same at any resolution.
struct Starfield
{
  // Gaussian points, which reach their rim three standard deviations out.
  std::vector<Neuron::StarPipeline::Star> points;
  // The brightest stars, for the sprite: each its sprite's whole square.
  std::vector<Neuron::StarPipeline::Star> bursts;
};

[[nodiscard]] Starfield BuildStarfield();
} // namespace Outpost
