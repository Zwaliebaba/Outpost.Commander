// The ground cursor's pixel half: the ring's texture in the tint §4's six meanings choose.
//
// NOT FOGGED. The cursor is where the commander is pointing, and a pointer that faded into the
// distance would be least visible exactly when he is reaching furthest - which is the opposite of
// what a cursor is for. Every other world pass fogs; this one is told where to be and drawn there.

cbuffer Cursor : register(b1)
{
  float4 g_tint;
};

Texture2D<float4> g_ring : register(t0);
SamplerState g_sampler : register(s0);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
  const float4 texel = g_ring.Sample(g_sampler, uv);
  return texel * g_tint;
}
