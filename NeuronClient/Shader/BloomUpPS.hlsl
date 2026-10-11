// One level of the glow added into the level above it, which is twice its size (ADR-091): a three by three tent of bilinear
// taps a texel of the smaller level apart, weighed by the spread, which the blend adds to what the down pass left there.
// cbuffer Pass is declared again, identically, in BloomBrightPS.hlsl and BloomDownPS.hlsl, and VertexOut in BloomVS.hlsl;
// they must match Bloom.cpp.

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
  const float2 uv = input.position.xy * targetTexel;
  float3 sum = Tap(uv, float2(-1.0f, -1.0f)) + Tap(uv, float2(1.0f, -1.0f)) + Tap(uv, float2(-1.0f, 1.0f)) + Tap(uv, float2(1.0f, 1.0f));
  sum += 2.0f * (Tap(uv, float2(0.0f, -1.0f)) + Tap(uv, float2(-1.0f, 0.0f)) + Tap(uv, float2(1.0f, 0.0f)) + Tap(uv, float2(0.0f, 1.0f)));
  sum += 4.0f * Tap(uv, float2(0.0f, 0.0f));
  return float4(sum * (weight / 16.0f), 1.0f);
}
