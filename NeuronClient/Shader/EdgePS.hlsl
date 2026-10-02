// The edges of a mesh's triangles, drawn as lines over it in one flat color, unlit (MeshPipeline::DrawEdges). It takes
// what MeshVS.hlsl writes; the object's constants are declared again, identically, in MeshVS.hlsl and MeshPS.hlsl, and
// must match MeshPipeline.cpp. The color is linear; the render target encodes it to sRGB.

cbuffer Object : register(b1)
{
  row_major float4x4 world;
  float4 color;
};

struct VertexOut
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
};

float4 main(VertexOut input) : SV_Target
{
  return float4(color.rgb, color.a);
}
