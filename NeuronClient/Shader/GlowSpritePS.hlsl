// A glow with a sprite is the sprite's color times the glow's, and the blend adds it to the scene (ADR-019, ADR-023):
// DeepSpaceOutpost's particle, its texture modulating its color. The texture's bytes are sRGB but read through a UNORM
// view, so they are taken to linear here, as the glow's color already is. The color is linear; the render target
// encodes it to sRGB. VertexOut is declared again, identically, in GlowVS.hlsl.

Texture2D sprite : register(t0);
SamplerState spriteSampler : register(s0);

struct VertexOut
{
  float4 position : SV_Position;
  float2 corner : TEXCOORD0;
  float4 color : COLOR;
};

float4 main(VertexOut input) : SV_Target
{
  // The corner runs from -1 to 1 with +1 at the top; the texture's v runs from 0 at the top to 1 at the bottom.
  const float2 uv = float2(input.corner.x, -input.corner.y) * 0.5f + 0.5f;
  const float3 texel = pow(saturate(sprite.Sample(spriteSampler, uv).rgb), 2.2f);
  return float4(input.color.rgb * texel, 0.0f);
}
