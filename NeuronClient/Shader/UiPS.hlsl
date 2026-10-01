// The interface: the quad's color, with the glyph atlas's coverage as its alpha (ADR-015). A solid rectangle samples a
// block of full coverage. The color is linear; the render target encodes it to sRGB.

Texture2D<float> atlas : register(t0);
SamplerState atlasSampler : register(s0);

struct VertexOut
{
  float4 position : SV_Position;
  float2 texel : TEXCOORD;
  float4 color : COLOR;
};

float4 main(VertexOut input) : SV_Target
{
  const float coverage = atlas.Sample(atlasSampler, input.texel);
  return float4(input.color.rgb, input.color.a * coverage);
}