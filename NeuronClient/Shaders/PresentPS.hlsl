// TRIAL FILE for ADR-012. The real present shader is M0.15's.
Texture2D<float4> g_scene : register(t0);
SamplerState g_sceneSampler : register(s0);

float4 main(float4 _position : SV_Position, float2 _texcoord : TEXCOORD0) : SV_Target
{
  return g_scene.Sample(g_sceneSampler, _texcoord);
}
