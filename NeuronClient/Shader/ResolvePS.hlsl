// The scene's resolve (ADR-050): every sample of the multisampled scene averaged into the back buffer's pixel. The scene is
// read through an sRGB view, so its samples arrive as linear color and are averaged in it, as the hardware resolve of the
// sRGB format averaged them (ADR-040); the back buffer's sRGB view encodes the mean. VertexOut is declared again,
// identically, in ResolveVS.hlsl.

// Must match Renderer::SAMPLE_COUNT.
static const uint SAMPLE_COUNT = 4;

Texture2DMS<float4> scene : register(t0);

struct VertexOut
{
  float4 position : SV_Position;
};

float4 main(VertexOut input) : SV_Target
{
  const int2 pixel = int2(input.position.xy);
  float4 sum = float4(0.0f, 0.0f, 0.0f, 0.0f);
  [unroll] for (uint i = 0; i < SAMPLE_COUNT; ++i)
    sum += scene.Load(pixel, i);
  return sum / (float)SAMPLE_COUNT;
}
