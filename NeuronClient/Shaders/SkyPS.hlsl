// ADR-019's backdrop, the pixel half: **one fetch per pixel**, which is the whole point of baking the
// galaxy into a cubemap at match start rather than evaluating its noise every frame.

TextureCube<float4> g_galaxy : register(t0);
SamplerState g_sampler : register(s0);

struct Input
{
  float4 position : SV_Position;
  float3 direction : TEXCOORD0;
};

float4 main(Input _input) : SV_Target
{
  // Normalized here rather than in the vertex stage: interpolating three normalized corner vectors
  // across a triangle does not produce a normalized vector in the middle of it, and a cube fetch with
  // a short vector still picks the right face -- so the error would be a subtle warp toward the
  // corners rather than anything that announces itself.
  return float4(g_galaxy.Sample(g_sampler, normalize(_input.direction)).rgb, 1.0f);
}
