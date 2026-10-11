// The scene's resolve (ADR-050): every sample of the multisampled scene averaged into the back buffer's pixel. The scene is
// read through an sRGB view, so its samples arrive as linear color and are averaged in it, as the hardware resolve of the
// sRGB format averaged them (ADR-040); the back buffer's sRGB view encodes the mean. The glow's first level, which holds
// the whole glow, is added to it in linear color, upsampled by a three by three tent of bilinear taps (ADR-091). VertexOut is
// declared again, identically, in ResolveVS.hlsl.

// Must match Renderer::SAMPLE_COUNT.
static const uint SAMPLE_COUNT = 4;

// The size of a texel of the glow's first level and of the back buffer, as shares of each, and how much of the glow is
// added. Must match Renderer.cpp's ResolveConstants.
cbuffer Resolve : register(b0)
{
  float2 glowTexel;
  float2 targetTexel;
  float glowScale;
  float3 unused0;
};

Texture2DMS<float4> scene : register(t0);
Texture2D<float4> glow : register(t1);
SamplerState linearClamp : register(s0);

struct VertexOut
{
  float4 position : SV_Position;
};

float3 GlowTap(float2 uv, float2 texels)
{
  return glow.SampleLevel(linearClamp, uv + texels * glowTexel, 0.0f).rgb;
}

float4 main(VertexOut input) : SV_Target
{
  const int2 pixel = int2(input.position.xy);
  float4 sum = float4(0.0f, 0.0f, 0.0f, 0.0f);
  [unroll] for (uint i = 0; i < SAMPLE_COUNT; ++i)
    sum += scene.Load(pixel, i);
  const float4 mean = sum / (float)SAMPLE_COUNT;

  const float2 uv = input.position.xy * targetTexel;
  float3 glowed = GlowTap(uv, float2(-1.0f, -1.0f)) + GlowTap(uv, float2(1.0f, -1.0f)) + GlowTap(uv, float2(-1.0f, 1.0f)) +
                  GlowTap(uv, float2(1.0f, 1.0f));
  glowed += 2.0f * (GlowTap(uv, float2(0.0f, -1.0f)) + GlowTap(uv, float2(-1.0f, 0.0f)) + GlowTap(uv, float2(1.0f, 0.0f)) +
                    GlowTap(uv, float2(0.0f, 1.0f)));
  glowed += 4.0f * GlowTap(uv, float2(0.0f, 0.0f));
  return float4(mean.rgb + glowed * (glowScale / 16.0f), mean.a);
}
