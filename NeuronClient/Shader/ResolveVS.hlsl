// The scene's resolve (ADR-050): one triangle over the whole back buffer, made from the vertex index alone. VertexOut is
// declared again, identically, in ResolvePS.hlsl.

struct VertexOut
{
  float4 position : SV_Position;
};

VertexOut main(uint vertexId : SV_VertexID)
{
  // (-1, 1), (3, 1) and (-1, -3) in clip space: a triangle twice the screen's size, which the rasterizer cuts to the screen.
  const float2 corner = float2(vertexId == 1 ? 3.0f : -1.0f, vertexId == 2 ? -3.0f : 1.0f);
  VertexOut output;
  output.position = float4(corner, 0.0f, 1.0f);
  return output;
}
