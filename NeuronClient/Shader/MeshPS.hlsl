// Flat lighting in the object's color: one directional light and an ambient floor, no textures and no specular
// (design §11, ADR-011). The color is linear, its instance's (ADR-053); the render target encodes it to sRGB.
// cbuffer Frame is declared again, identically, in MeshVS.hlsl and MeshLineVS.hlsl, and VertexOut in both; they must
// match MeshPipeline.cpp.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float3 directionToLight;
  float ambient;
  float3 eyePosition;
  float unused0;
};

struct VertexOut
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  nointerpolation float4 color : COLOR;
};

float4 main(VertexOut input) : SV_Target
{
  const float diffuse = saturate(dot(normalize(input.normal), directionToLight));
  const float light = ambient + (1.0f - ambient) * diffuse;
  return float4(input.color.rgb * light, input.color.a);
}
