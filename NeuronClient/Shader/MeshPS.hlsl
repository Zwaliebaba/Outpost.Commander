// Flat lighting in the object's color: one directional light and an ambient floor, no textures and no specular
// (design §11, ADR-011). The color is linear; the render target encodes it to sRGB.
// The two constant buffers are declared again, identically, in MeshVS.hlsl and MeshLineVS.hlsl, and must match
// MeshPipeline.cpp.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float3 directionToLight;
  float ambient;
  float3 eyePosition;
  float unused0;
};

cbuffer Object : register(b1)
{
  row_major float4x4 world;
  float4 color;
  float liftShare;
  float3 unused1;
};

struct VertexOut
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
};

float4 main(VertexOut input) : SV_Target
{
  const float diffuse = saturate(dot(normalize(input.normal), directionToLight));
  const float light = ambient + (1.0f - ambient) * diffuse;
  return float4(color.rgb * light, color.a);
}