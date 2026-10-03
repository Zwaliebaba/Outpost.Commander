// A mesh in the scene: each vertex placed by its instance's world matrix and the frame's camera (ADR-011, ADR-053).
// cbuffer Frame is declared again, identically, in MeshPS.hlsl and MeshLineVS.hlsl; cbuffer Draw, struct Object and
// VertexOut in MeshLineVS.hlsl, and VertexOut in MeshPS.hlsl. They must match MeshPipeline.cpp.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float3 directionToLight;
  float ambient;
  float3 eyePosition;
  float unused0;
};

// Where this draw's instances start among the frame's.
cbuffer Draw : register(b1)
{
  uint firstObject;
};

// MeshPipeline::Instance: the world matrix by rows, the color, and how far a line is pulled toward the eye.
struct Object
{
  float4 world0;
  float4 world1;
  float4 world2;
  float4 world3;
  float4 color;
  float liftShare;
  float3 unused1;
};

StructuredBuffer<Object> objects : register(t0);

struct VertexIn
{
  float3 position : POSITION;
  float3 normal : NORMAL;
};

struct VertexOut
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  nointerpolation float4 color : COLOR;
};

VertexOut main(VertexIn input, uint instanceId : SV_InstanceID)
{
  const Object drawn = objects[firstObject + instanceId];
  const float4x4 world = float4x4(drawn.world0, drawn.world1, drawn.world2, drawn.world3);
  VertexOut output;
  const float4 worldPosition = mul(float4(input.position, 1.0f), world);
  output.position = mul(worldPosition, viewProjection);
  // The world matrix scales uniformly, so its upper 3x3 turns normals correctly; the pixel shader normalizes them.
  output.normal = mul(input.normal, (float3x3)world);
  output.color = drawn.color;
  return output;
}
