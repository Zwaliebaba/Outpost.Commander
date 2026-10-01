// A glow fades from its color at the center to nothing at its rim, and the blend adds it to the scene (ADR-019). The
// color is linear; the render target encodes it to sRGB.

struct VertexOut
{
  float4 position : SV_Position;
  float2 corner : TEXCOORD0;
  float4 color : COLOR;
};

float4 main(VertexOut input) : SV_Target
{
  const float fade = saturate(1.0f - dot(input.corner, input.corner));
  return float4(input.color.rgb * fade * fade, 0.0f);
}
