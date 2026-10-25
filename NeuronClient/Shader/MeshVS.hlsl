// A mesh in the scene: each vertex placed by its instance's world matrix and the frame's camera (ADR-011, ADR-053), in the
// color of its paint (ADR-095). cbuffer Frame is declared again, identically, in MeshPS.hlsl, MeshLineVS.hlsl and
// MeshLinePS.hlsl; cbuffer Draw, struct Object and PaintOf in MeshLineVS.hlsl; and VertexOut in MeshPS.hlsl. A line's
// VertexOut, in MeshLineVS.hlsl and MeshLinePS.hlsl, is the same without the openness, which a line does not take
// (ADR-092). They must match MeshPipeline.cpp.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float3 directionToLight;
  float keyWrap;
  float3 eyePosition;
  float occlusionStrength;
  float3 keyColor;
  float unused1;
  float3 skyFill;
  float unused2;
  float3 groundFill;
  float unused3;
};

// Where this draw's instances start among the frame's.
cbuffer Draw : register(b1)
{
  uint firstObject;
};

// MeshPipeline::Instance: the world matrix by rows, the color of each paint, how far a line is pulled toward the eye, and
// whether the third paint glows.
struct Object
{
  float4 world0;
  float4 world1;
  float4 world2;
  float4 world3;
  float4 color;
  float4 secondColor;
  float4 thirdColor;
  float liftShare;
  float thirdGlows;
  float2 unused1;
};

StructuredBuffer<Object> objects : register(t0);

struct VertexIn
{
  float3 position : POSITION;
  float3 normal : NORMAL;
  float openness : OPENNESS;
  uint paint : PAINT;
};

struct VertexOut
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  nointerpolation float4 color : COLOR;
  nointerpolation float glow : GLOW;
  float openness : OPENNESS;
};

// The color of a vertex's paint on its instance, and whether it glows: only the third paint may.
float4 PaintOf(Object drawn, uint paint, out float glow)
{
  glow = paint == 2 ? drawn.thirdGlows : 0.0f;
  return paint == 1 ? drawn.secondColor : paint == 2 ? drawn.thirdColor : drawn.color;
}

VertexOut main(VertexIn input, uint instanceId : SV_InstanceID)
{
  const Object drawn = objects[firstObject + instanceId];
  const float4x4 world = float4x4(drawn.world0, drawn.world1, drawn.world2, drawn.world3);
  VertexOut output;
  const float4 worldPosition = mul(float4(input.position, 1.0f), world);
  output.position = mul(worldPosition, viewProjection);
  // The world matrix scales uniformly, so its upper 3x3 turns normals correctly; the pixel shader normalizes them.
  output.normal = mul(input.normal, (float3x3)world);
  output.color = PaintOf(drawn, input.paint, output.glow);
  output.openness = input.openness;
  return output;
}
