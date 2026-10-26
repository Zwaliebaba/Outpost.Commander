// Flat lighting in the color of the face's paint: a key light from one direction and a fill from above and below, no
// textures and no specular (ADR-011, ADR-090). The color is linear, its instance's (ADR-053, ADR-095); the render target
// encodes it to sRGB. A face is darkened by how shut in it is, as the baker worked it out, by occlusionStrength's share
// (ADR-092). A face whose paint glows is drawn in its color as it is, which no light reaches and no shade darkens.
// cbuffer Frame is declared again, identically, in MeshVS.hlsl, MeshLineVS.hlsl and MeshLinePS.hlsl, and VertexOut in
// MeshVS.hlsl; they must match MeshPipeline.cpp.

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

struct VertexOut
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  nointerpolation float4 color : COLOR;
  nointerpolation float glow : GLOW;
  float openness : OPENNESS;
};

// The light that reaches a surface whose unit normal is normal: the key as far as the surface turns toward it, softened by
// keyWrap, and the fill blended from below to above by how far it looks up. MeshLinePS.hlsl and Outpost::FaceLight work it
// out the same way; FaceLight darkens it by the openness as main does.
float3 LightAt(float3 normal)
{
  const float3 fill = lerp(groundFill, skyFill, normal.y * 0.5f + 0.5f);
  const float key = saturate((dot(normal, directionToLight) + keyWrap) / (1.0f + keyWrap));
  return fill + keyColor * key;
}

float4 main(VertexOut input) : SV_Target
{
  if (input.glow > 0.5f)
    return input.color;
  const float shutIn = 1.0f - saturate(input.openness);
  return float4(input.color.rgb * LightAt(normalize(input.normal)) * (1.0f - occlusionStrength * shutIn), input.color.a);
}
