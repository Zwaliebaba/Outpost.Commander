// R13's scaled present, the pixel half: one fetch from the scene target. The sampler is bound per
// frame -- point for R13's 1:1 and exact-multiple cases, bilinear for everything else -- so the
// filter is a descriptor rather than a branch, and this shader has no idea which one it got.
Texture2D<float4> g_scene : register(t0);
SamplerState g_sceneSampler : register(s0);

float4 main(float4 _position : SV_Position, float2 _texcoord : TEXCOORD0) : SV_Target
{
  return g_scene.Sample(g_sceneSampler, _texcoord);
}
