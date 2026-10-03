// A line in the scene, placed as MeshVS.hlsl places a mesh, then pulled liftShare of the way toward the eye. A point
// moved along its own sightline stays where it is on the screen, so the line does not stand off a silhouette as a lift
// along the surface's normal did, yet it is nearer than the surface it lies on and wins the depth test (ADR-040).
// cbuffer Frame, cbuffer Draw, struct Object and VertexOut are declared again, identically, in MeshVS.hlsl, and must
// match MeshPipeline.cpp (ADR-053).

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float3 directionToLight;
  float ambient;
  float3 eyePosition;
  float unused0;
};

cbuffer Draw : register(b1)
{
  uint firstObject;
};

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
  const float3 worldPosition = mul(float4(input.position, 1.0f), world).xyz;
  const float3 pulled = lerp(worldPosition, eyePosition, drawn.liftShare);
  output.position = mul(float4(pulled, 1.0f), viewProjection);
  // The world matrix scales uniformly, so its upper 3x3 turns normals correctly; the pixel shader normalizes them.
  output.normal = mul(input.normal, (float3x3)world);
  output.color = drawn.color;
  return output;
}
