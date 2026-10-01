// A bright star is its sprite's alpha in the star's color, and the blend adds it to the frame (ADR-022). The sprite is
// upright on the screen, as a camera's diffraction spikes are, whichever way the camera turns. Only the alpha is read:
// the sprite is white, and its shape is its alpha. The color is linear; the render target encodes it to sRGB.

Texture2D sprite : register(t0);
SamplerState spriteSampler : register(s0);

struct VertexOut
{
  float4 position : SV_Position;
  float2 corner : TEXCOORD0;
  float3 color : COLOR;
};

float4 main(VertexOut input) : SV_Target
{
  // The corner runs from -1 to 1 with +1 at the top; the texture's v runs from 0 at the top to 1 at the bottom.
  const float2 uv = float2(input.corner.x, -input.corner.y) * 0.5f + 0.5f;
  return float4(input.color * sprite.Sample(spriteSampler, uv).a, 0.0f);
}
