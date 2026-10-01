// A star: a quad a few pixels across round a direction at infinity, made from the vertex index alone, so the sky is one
// instanced draw (ADR-021). cbuffer Frame must match StarPipeline::FrameConstants and the input StarPipeline::Star;
// VertexOut and REACH are declared again, identically, in StarPS.hlsl.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float2 clipPerPixel;
  float2 unused;
};

struct StarIn
{
  float3 direction : POSITION;
  float spread : TEXCOORD0;
  float3 color : COLOR;
};

struct VertexOut
{
  float4 position : SV_Position;
  float2 corner : TEXCOORD0;
  float3 color : COLOR;
};

// How many standard deviations of its Gaussian the quad reaches from the star's center.
static const float REACH = 3.0f;

// A strip of two triangles over the quad's corners, from -1 to 1 across the screen and up it.
static const float2 CORNERS[4] = {float2(-1.0f, 1.0f), float2(1.0f, 1.0f), float2(-1.0f, -1.0f), float2(1.0f, -1.0f)};

VertexOut main(StarIn input, uint vertexId : SV_VertexID)
{
  VertexOut output;
  // A direction has no position, so w = 0 leaves the camera's translation out and the star stays at infinity.
  float4 position = mul(float4(input.direction, 0.0f), viewProjection);
  // The offset is scaled by w, so that after the divide it is the same number of pixels wherever the star is.
  const float2 corner = CORNERS[vertexId];
  position.xy += corner * (REACH * input.spread) * clipPerPixel * position.w;
  // Halfway into the depth range, so that only a star behind the camera, where w < 0, is clipped. Depth is not tested.
  position.z = 0.5f * position.w;
  output.position = position;
  output.corner = corner;
  output.color = input.color;
  return output;
}
