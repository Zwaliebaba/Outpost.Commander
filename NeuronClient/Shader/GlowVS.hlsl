// A glow: a quad facing the camera around a point, made from the vertex index alone, so a frame's glows are one
// instanced draw (ADR-018). cbuffer Frame must match GlowPipeline::FrameConstants and the input GlowPipeline::Glow;
// VertexOut is declared again, identically, in GlowPS.hlsl.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float3 screenRight;
  float unused0;
  float3 screenUp;
  float unused1;
};

struct GlowIn
{
  float3 center : POSITION;
  float radius : TEXCOORD0;
  float4 color : COLOR;
};

struct VertexOut
{
  float4 position : SV_Position;
  float2 corner : TEXCOORD0;
  float4 color : COLOR;
};

// Two triangles over the quad's corners, from -1 to 1 along the screen's right and its up.
static const float2 CORNERS[6] = {float2(-1.0f, 1.0f), float2(1.0f, 1.0f), float2(1.0f, -1.0f),
                                  float2(-1.0f, 1.0f), float2(1.0f, -1.0f), float2(-1.0f, -1.0f)};

VertexOut main(GlowIn input, uint vertexId : SV_VertexID)
{
  VertexOut output;
  const float2 corner = CORNERS[vertexId];
  const float3 worldPosition = input.center + (screenRight * corner.x + screenUp * corner.y) * input.radius;
  output.position = mul(float4(worldPosition, 1.0f), viewProjection);
  output.corner = corner;
  output.color = input.color;
  return output;
}
