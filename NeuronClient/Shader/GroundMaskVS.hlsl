// The ground mask's square on the plane y = 0, made from the vertex index alone. cbuffer Frame must match
// GroundMaskPipeline::FrameConstants; it and VertexOut are declared again, identically, in GroundMaskPS.hlsl.

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

// Two triangles over the square's corners, from its origin to the far corner.
static const float2 CORNERS[6] = {float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f),
                                  float2(0.0f, 0.0f), float2(1.0f, 1.0f), float2(0.0f, 1.0f)};

VertexOut main(uint vertexId : SV_VertexID)
{
  const float side = cellMeters * (float)cellsPerSide;
  const float2 ground = float2(originXMeters, originZMeters) + CORNERS[vertexId] * side;
  VertexOut output;
  output.position = mul(float4(ground.x, 0.0f, ground.y, 1.0f), viewProjection);
  output.ground = ground;
  return output;
}
