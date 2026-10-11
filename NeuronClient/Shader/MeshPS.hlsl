// Flat lighting in the object's color: a key light from one direction and a fill from above and below, no textures and no
// specular (design §11, ADR-011, ADR-090). The color is linear, its instance's (ADR-053); the render target encodes it to
// sRGB. cbuffer Frame is declared again, identically, in MeshVS.hlsl, MeshLineVS.hlsl and MeshLinePS.hlsl, and VertexOut in
// all three; they must match MeshPipeline.cpp.

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

// The light that reaches a surface whose unit normal is normal: the key as far as the surface turns toward it, softened by
// keyWrap, and the fill blended from below to above by how far it looks up. MeshLinePS.hlsl and Outpost::FaceLight work it
// out the same way.
float3 LightAt(float3 normal)
{
  const float3 fill = lerp(groundFill, skyFill, normal.y * 0.5f + 0.5f);
  const float key = saturate((dot(normal, directionToLight) + keyWrap) / (1.0f + keyWrap));
  return fill + keyColor * key;
}

float4 main(VertexOut input) : SV_Target
{
  return float4(input.color.rgb * LightAt(normalize(input.normal)), input.color.a);
}
