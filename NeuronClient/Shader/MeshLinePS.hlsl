// A line in the scene, as bright as the light on a surface that faces the way it does, in its own hue: a line carries
// whose a model is, or what a ring or a border means, and the light's warmth and coolness are the faces' (ADR-090). Its
// normal is the mean of its two faces' (ADR-027). cbuffer Frame is declared again, identically, in MeshVS.hlsl,
// MeshLineVS.hlsl and MeshPS.hlsl, and VertexOut in all three; they must match MeshPipeline.cpp.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float3 directionToLight;
  float keyWrap;
  float3 eyePosition;
  float unused0;
  float3 keyColor;
  float unused1;
  float3 skyFill;
  float unused2;
  float3 groundFill;
  float unused3;
};

struct VertexOut
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  nointerpolation float4 color : COLOR;
};

// The luminance weights of linear sRGB.
static const float3 LUMINANCE = float3(0.2126f, 0.7152f, 0.0722f);

// MeshPS.hlsl's light, which Outpost::FaceLight works out the same way.
float3 LightAt(float3 normal)
{
  const float3 fill = lerp(groundFill, skyFill, normal.y * 0.5f + 0.5f);
  const float key = saturate((dot(normal, directionToLight) + keyWrap) / (1.0f + keyWrap));
  return fill + keyColor * key;
}

float4 main(VertexOut input) : SV_Target
{
  return float4(input.color.rgb * dot(LightAt(normalize(input.normal)), LUMINANCE), input.color.a);
}
