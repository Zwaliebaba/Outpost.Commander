// The glow's first level (ADR-091): each of its pixels covers two by two of the scene's. It averages each scene pixel's
// samples, read through an sRGB view so that they arrive as linear color, as the resolve does (ADR-050), keeps of each pixel
// the share of its light above the threshold, and averages the four. A pixel is kept by its luminance, eased in over the knee
// either side of the threshold, so that nothing starts to glow at a hard edge. cbuffer PassConstants is declared again,
// identically, in BloomDownPS.hlsl and BloomUpPS.hlsl, and VertexOut in BloomVS.hlsl; they must match Bloom.cpp. A cbuffer
// cannot be named Pass: fxc reads pass as an effect's keyword whatever its case.

// Must match Renderer::SAMPLE_COUNT.
static const uint SAMPLE_COUNT = 4;
// The luminance weights of linear sRGB.
static const float3 LUMINANCE = float3(0.2126f, 0.7152f, 0.0722f);

cbuffer PassConstants : register(b0)
{
  float2 sourceTexel;
  float2 targetTexel;
  float thresholdLuminance;
  float kneeLuminance;
  float weight;
  float unused0;
};

Texture2DMS<float4> scene : register(t0);

struct VertexOut
{
  float4 position : SV_Position;
};

// The share of a color's light that glows: none below the threshold less the knee, a curve up to the threshold and the
// knee, and all of what is above the threshold beyond it.
float3 Bright(float3 color)
{
  const float luminance = dot(color, LUMINANCE);
  const float soft = clamp(luminance - thresholdLuminance + kneeLuminance, 0.0f, 2.0f * kneeLuminance);
  const float curve = soft * soft / (4.0f * kneeLuminance + 1e-5f);
  return color * (max(curve, luminance - thresholdLuminance) / max(luminance, 1e-5f));
}

float4 main(VertexOut input) : SV_Target
{
  const int2 first = int2(input.position.xy) * 2;
  float3 sum = float3(0.0f, 0.0f, 0.0f);
  [unroll] for (int y = 0; y < 2; ++y)
  {
    [unroll] for (int x = 0; x < 2; ++x)
    {
      float3 pixel = float3(0.0f, 0.0f, 0.0f);
      [unroll] for (uint i = 0; i < SAMPLE_COUNT; ++i)
        pixel += scene.Load(first + int2(x, y), i).rgb;
      sum += Bright(pixel / (float)SAMPLE_COUNT);
    }
  }
  return float4(sum * 0.25f, 1.0f);
}
