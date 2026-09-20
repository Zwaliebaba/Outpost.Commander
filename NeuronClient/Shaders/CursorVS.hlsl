// The ground cursor's vertex half (Design/Interface.md §4; m1-vertical-slice/K7): a world-space
// quad, already oriented to the ground on the CPU, through the view projection. The basis is built
// there rather than here because its degenerate case - flat ground, where the normal's cross with
// world up is a zero vector - is a branch, and a branch per vertex to fix a case that is true of
// most of this landscape is a branch in the wrong place.

cbuffer SceneConstants : register(b0)
{
  float4x4 g_viewProjection;
  float4 g_cameraPosition;
  float4 g_lightDirection0;
  float4 g_lightColor0;
  float4 g_lightDirection1;
  float4 g_lightColor1;
  float4 g_fog;
  float4 g_fogColor;
};

struct Output
{
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

Output main(float3 position : POSITION, float2 uv : TEXCOORD0)
{
  Output output;
  output.uv = uv;
  output.position = mul(float4(position, 1.0), g_viewProjection);
  return output;
}
