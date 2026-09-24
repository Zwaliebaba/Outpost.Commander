// A beam, the pixel half: **bright along the middle and fading to nothing at both edges**, so a thin quad reads
// as a line of light rather than a hard strip.

struct Input
{
  float4 position : SV_Position;

  /// Minus one to plus one across the beam.
  float across : TEXCOORD0;

  float3 color : TEXCOORD1;
};

float4 main(Input _input) : SV_Target
{
  // The same flat-topped Hermite falloff the stars use, across one axis instead of around a point.
  const float intensity = 1.0f - smoothstep(0.0f, 1.0f, abs(_input.across));

  // **THE PASS BLENDS ADDITIVELY**, so the color carries the brightness and alpha carries nothing.
  return float4(_input.color * intensity, 1.0f);
}
