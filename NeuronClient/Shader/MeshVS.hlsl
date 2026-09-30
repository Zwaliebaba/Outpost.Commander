// A mesh in the scene: each vertex placed by its object's world matrix and the frame's camera (ADR-011).
// The two constant buffers are declared again, identically, in MeshPS.hlsl, and must match MeshPipeline.cpp.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float3 directionToLight;
  float ambient;
};

cbuffer Object : register(b1)
{
  row_major float4x4 world;
  float4 color;
};

struct VertexIn
{
  float3 position : POSITION;
  float3 normal : NORMAL;
};

struct VertexOut
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
};

VertexOut main(VertexIn input)
{
  VertexOut output;
  const float4 worldPosition = mul(float4(input.position, 1.0f), world);
  output.position = mul(worldPosition, viewProjection);
  // The world matrix scales uniformly, so its upper 3x3 turns normals correctly; the pixel shader normalizes them.
  output.normal = mul(input.normal, (float3x3)world);
  return output;
}
