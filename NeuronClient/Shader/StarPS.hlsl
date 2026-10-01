// A star fades as a Gaussian from its color at the center to nothing at the quad's inscribed circle, and the blend adds
// it to the frame (ADR-021). The Gaussian is lowered by its value at the rim, so the quad's edge never shows. The color
// is linear; the render target encodes it to sRGB.

struct VertexOut
{
  float4 position : SV_Position;
  float2 corner : TEXCOORD0;
  float3 color : COLOR;
};

static const float REACH = 3.0f;

float4 main(VertexOut input) : SV_Target
{
  const float exponent = -0.5f * REACH * REACH;
  const float rim = exp(exponent);
  const float falloff = saturate((exp(exponent * dot(input.corner, input.corner)) - rim) / (1.0f - rim));
  return float4(input.color * falloff, 0.0f);
}
