// M0.21b's world pass, the pixel half. One flat color per entity, arriving as root constants.
//
// FLAT AND UNLIT, DELIBERATELY. `TechnicalDesign.md` section 6 describes a lit world with a light
// rig the mesh handoff specifies, and none of that belongs to a step whose job is to prove the
// transform. A lit shape whose normals are wrong and a shape at the wrong place look alike from a
// distance; a flat one can only be in the wrong place.
cbuffer EntityColor : register(b1)
{
  float4 g_color;
};

float4 main(float4 _position : SV_Position) : SV_Target
{
  return g_color;
}
