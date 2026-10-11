// One level of the glow from the level above it, which is twice its size (ADR-091): thirteen bilinear taps, in five boxes
// of four that overlap, the middle box weighed as much as the four corner boxes together. It blurs as it halves, and a thin
// bright line does not flicker as it moves across the larger level's texels. cbuffer Pass is declared again, identically, in
// BloomBrightPS.hlsl and BloomUpPS.hlsl, and VertexOut in BloomVS.hlsl; they must match Bloom.cpp.

cbuffer Pass : register(b0)
{
  float2 sourceTexel;
  float2 targetTexel;
  float thresholdLuminance;
  float kneeLuminance;
  float weight;
  float unused0;
};

Texture2D<float4> source : register(t0);
SamplerState linearClamp : register(s0);

struct VertexOut
{
  float4 position : SV_Position;
};

float3 Tap(float2 uv, float2 texels)
{
  return source.SampleLevel(linearClamp, uv + texels * sourceTexel, 0.0f).rgb;
}

float4 main(VertexOut input) : SV_Target
{
  // The middle of this pixel, which falls on a corner between four of the level above.
  const float2 uv = input.position.xy * targetTexel;
  const float3 a = Tap(uv, float2(-2.0f, -2.0f));
  const float3 b = Tap(uv, float2(0.0f, -2.0f));
  const float3 c = Tap(uv, float2(2.0f, -2.0f));
  const float3 d = Tap(uv, float2(-1.0f, -1.0f));
  const float3 e = Tap(uv, float2(1.0f, -1.0f));
  const float3 f = Tap(uv, float2(-2.0f, 0.0f));
  const float3 g = Tap(uv, float2(0.0f, 0.0f));
  const float3 h = Tap(uv, float2(2.0f, 0.0f));
  const float3 i = Tap(uv, float2(-1.0f, 1.0f));
  const float3 j = Tap(uv, float2(1.0f, 1.0f));
  const float3 k = Tap(uv, float2(-2.0f, 2.0f));
  const float3 l = Tap(uv, float2(0.0f, 2.0f));
  const float3 m = Tap(uv, float2(2.0f, 2.0f));
  float3 sum = (d + e + i + j) * 0.125f;
  sum += (a + b + g + f) * 0.03125f;
  sum += (b + c + h + g) * 0.03125f;
  sum += (f + g + l + k) * 0.03125f;
  sum += (g + h + m + l) * 0.03125f;
  return float4(sum, 1.0f);
}
