// The glow's passes (ADR-091): one triangle over the whole level written, made from the vertex index alone, as the resolve's
// is (ADR-050). VertexOut is declared again, identically, in BloomBrightPS.hlsl, BloomDownPS.hlsl and BloomUpPS.hlsl.

struct VertexOut
{
  float4 position : SV_Position;
};

VertexOut main(uint vertexId : SV_VertexID)
{
  // (-1, 1), (3, 1) and (-1, -3) in clip space: a triangle twice the target's size, which the rasterizer cuts to it.
  const float2 corner = float2(vertexId == 1 ? 3.0f : -1.0f, vertexId == 2 ? -3.0f : 1.0f);
  VertexOut output;
  output.position = float4(corner, 0.0f, 1.0f);
  return output;
}
