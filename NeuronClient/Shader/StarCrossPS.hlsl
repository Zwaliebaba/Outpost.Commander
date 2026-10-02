// A bright star is a cross of two lines a pixel wide along the screen's axes, with a dot at its center, and the blend
// adds it to the frame (ADR-028): a vector-drawn star, in the line art of the asteroids' ridges (ADR-027). Each arm fades
// from the star's color at the center to nothing at the quad's edge. The lines are a pixel of the screen wide, whatever
// the star's size or the screen's resolution, as the ridges are. The color is linear; the render target encodes it to
// sRGB.

struct VertexOut
{
  float4 position : SV_Position;
  float2 corner : TEXCOORD0;
  float3 color : COLOR;
};

// The center dot's radius, in pixels.
static const float CORE_PIXELS = 1.0f;

float4 main(VertexOut input) : SV_Target
{
  // The corner runs from -1 to 1 across the quad, so one pixel of the screen is this far in it. The quad faces the
  // screen, upright, so the corner's x changes only across the screen and its y only up it.
  const float2 perPixel = max(fwidth(input.corner), 1e-6f);
  // How many pixels this one is from the vertical arm and from the horizontal arm.
  const float2 pixels = abs(input.corner) / perPixel;
  // Each arm is a line a pixel wide filtered over a pixel, full on its center line and nothing a pixel off it, so it
  // stays smooth as the camera turns the sky. It fades linearly toward its tips.
  const float across = saturate(1.0f - pixels.y) * (1.0f - abs(input.corner.x));
  const float up = saturate(1.0f - pixels.x) * (1.0f - abs(input.corner.y));
  const float core = saturate(1.0f + CORE_PIXELS - length(pixels));
  return float4(input.color * max(max(across, up), core), 0.0f);
}
