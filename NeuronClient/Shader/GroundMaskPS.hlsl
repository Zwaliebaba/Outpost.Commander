// The ground mask: black, as opaque as the shade at this point of the ground, which is blended between the four nearest
// cells' centers so that no cell's edge shows, and darkened below the knee as GroundMaskPipeline::FrameConstants says. The
// blend darkens the scene under it.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float originXMeters;
  float originZMeters;
  float cellMeters;
  uint cellsPerSide;
  float kneeShade;
  float kneeOpacity;
};

struct VertexOut
{
  float4 position : SV_Position;
  float2 ground : TEXCOORD0;
};

// Cell (x, z) at texel (x, z), its shade in red (ADR-052). Must match GroundMaskPipeline::TEXTURE_SIDE.
static const float TEXTURE_SIDE = 512.0f;
Texture2D<float> shades : register(t0);
SamplerState shadesSampler : register(s0);

// The opacity of a shade: in proportion up to the knee, where it reaches kneeOpacity, and the shade itself beyond it, but
// never less than kneeOpacity. It rises with the shade, with no step at the knee, so the blend between cells stays smooth.
// One expression, since fxc takes an early return for a result that may be left unset (X4000).
float Opacity(float shade)
{
  const float belowKnee = shade * (kneeOpacity / max(kneeShade, 1e-6f));
  return kneeShade <= 0.0f ? shade : (shade <= kneeShade ? belowKnee : max(kneeOpacity, shade));
}

float4 main(VertexOut input) : SV_Target
{
  // In cells, from the first cell's center, held between the first cell's center and the last's: beyond them the shade is
  // the nearest cell's. A texel's center is half a texel in, and the sampler blends the four nearest.
  const float last = (float)cellsPerSide - 1.0f;
  const float2 at = clamp((input.ground - float2(originXMeters, originZMeters)) / cellMeters - 0.5f, 0.0f, last);
  return float4(0.0f, 0.0f, 0.0f, Opacity(shades.SampleLevel(shadesSampler, (at + 0.5f) / TEXTURE_SIDE, 0.0f)));
}
