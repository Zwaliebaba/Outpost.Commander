// M1.9's ship pass, the pixel half: the hull ramp, the team colour and the handoff's light rig.
//
// **THE ALBEDO ARRIVES ENTIRELY THROUGH THE VERTEX COLOUR.** The CMO material's diffuse is white on
// purpose (`manifest.json`), so a pixel stage that ignored these two channels would render every hull
// white -- which is the failure the manifest names by itself, and which looks like a lighting problem
// rather than a missing lookup.
cbuffer ShipLook : register(b1)
{
  // The three stops of the hull ramp, which the vertex colour's green channel indexes: DEEP at 0,
  // BASE at 128, EDGE at 255. `GameClient/MeshCatalog.g.h` carries the hex these come from and
  // `HullToneColor` is the processor's statement of the same ramp.
  float4 g_hullDeep;
  float4 g_hullBase;
  float4 g_hullEdge;

  // xyz is a direction TOWARD the light; w is the intensity.
  float4 g_keyLight;
  float4 g_fillLight;

  // xyz is the ambient colour, MULTIPLIED BY ALBEDO and never added flat -- which is what stops it
  // lifting the backdrop (`manifest.json`'s light rig). w is its intensity.
  float4 g_ambient;
};

struct Input
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  float2 shade : TEXCOORD0;
  float3 team : TEXCOORD1;
  float brightness : TEXCOORD2;
};

float4 main(Input _input) : SV_Target
{
  // The two-segment ramp: DEEP to BASE over the first half of the channel, BASE to EDGE over the
  // second. The content quantizes the channel to the three stops, so nothing lands between them
  // today -- and it is written as a ramp anyway, because the processor's `HullToneColor` does this
  // and two answers to one question is how a palette starts to drift.
  const float tone = saturate(_input.shade.y);
  const float3 lower = lerp(g_hullDeep.rgb, g_hullBase.rgb, saturate(tone * 2.0f));
  const float3 upper = lerp(g_hullBase.rgb, g_hullEdge.rgb, saturate((tone - 0.5f) * 2.0f));
  const float3 hull = (tone < 0.5f) ? lower : upper;

  // **THE RED CHANNEL SELECTS**: 0 takes the hull palette, 1 takes the owner's colour.
  const float3 albedo = lerp(hull, _input.team, saturate(_input.shade.x));

  const float3 normal = normalize(_input.normal);

  // FLAT SHADING IS ALREADY IN THE GEOMETRY. The handoff splits faces and bakes per-face normals
  // (`manifest.json`'s conventions), so this is an ordinary Lambert term and the facets come from
  // the mesh rather than from a derivative here.
  const float key = saturate(dot(normal, normalize(g_keyLight.xyz))) * g_keyLight.w;
  const float fill = saturate(dot(normal, normalize(g_fillLight.xyz))) * g_fillLight.w;

  const float3 lit = albedo * (key + fill + (g_ambient.rgb * g_ambient.w));
  return float4(saturate(lit * _input.brightness), 1.0f);
}
