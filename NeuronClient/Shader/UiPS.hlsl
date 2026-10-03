// The interface: the quad's color, with the glyph atlas's coverage as its alpha (ADR-015). A solid rectangle samples a
// block of full coverage. A hatched one gives a negative u, and is striped here instead: diagonal stripes rising to the
// right, v pixels wide every -u pixels, laid on the screen's pixels (ADR-030). An image quad gives u past IMAGE_U_OFFSET,
// and takes its coverage from the image at u less it (ADR-052). The color is linear; the render target encodes it to sRGB.

// Must match IMAGE_U_OFFSET in UiPipeline.cpp.
static const float IMAGE_U_OFFSET = 2.0f;

Texture2D<float> atlas : register(t0);
Texture2D<float> image : register(t1);
SamplerState atlasSampler : register(s0);

struct VertexOut
{
  float4 position : SV_Position;
  float2 texel : TEXCOORD;
  float4 color : COLOR;
};

float4 main(VertexOut input) : SV_Target
{
  float coverage;
  if (input.texel.x < 0.0f)
  {
    const float period = -input.texel.x;
    coverage = fmod(input.position.x + input.position.y, period) < input.texel.y ? 1.0f : 0.0f;
  }
  else if (input.texel.x >= IMAGE_U_OFFSET)
  {
    // The image has no mipmaps either.
    coverage = image.SampleLevel(atlasSampler, float2(input.texel.x - IMAGE_U_OFFSET, input.texel.y), 0.0f);
  }
  else
  {
    // A level of the atlas, which has no mipmaps, so that sampling needs no derivatives inside the branch.
    coverage = atlas.SampleLevel(atlasSampler, input.texel, 0.0f);
  }
  return float4(input.color.rgb, input.color.a * coverage);
}