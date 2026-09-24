// M1.9's ship pass, the vertex half: one CMO mesh, many instances.
//
// ROW_MAJOR IS NOT OPTIONAL. HLSL packs a float4x4 column-major by default and `Camera.h`'s Matrix4
// is row-major, so without this word every frame would need a transpose on the processor to say the
// same thing. One word here against work on every entity. WorldVS says the same and for the same
// reason; the two must not disagree.
cbuffer ShipTransform : register(b0)
{
  row_major float4x4 g_viewProjection;
};

struct Vertex
{
  // The mesh, in ITS OWN coordinates -- already converted from the handoff's Y-up frame to this
  // camera's Z-up one on the processor (`GameClient/HullMesh.h`), because a conversion in a shader
  // is a conversion nothing can test.
  float3 local : POSITION;
  float3 normal : NORMAL;

  // x is the team selector and y is the hull tone, both 0 to 1 -- the file's red and green channels
  // (`manifest.json`'s vertexColorConvention).
  float2 shade : TEXCOORD0;

  // PER INSTANCE. Position on the plane, then the cosine and sine of the heading -- precomputed,
  // because a `sincos` per vertex to turn one entity is work this arrangement exists to avoid.
  float4 instanceTransform : TEXCOORD1;

  // PER INSTANCE. The owning player's colour, which is what lets ONE instanced draw cover every
  // ship of a shape regardless of owner -- and in w, a brightness on the whole hull, one unless it is a
  // wreck (M3.4).
  float4 instanceTeam : TEXCOORD2;
};

struct Output
{
  float4 position : SV_Position;
  float3 normal : NORMAL;
  float2 shade : TEXCOORD0;
  float3 team : TEXCOORD1;
  float brightness : TEXCOORD2;
};

Output main(Vertex _input)
{
  const float c = _input.instanceTransform.z;
  const float s = _input.instanceTransform.w;

  // A ROTATION ABOUT Z, because Z is the axis out of the plane (ADR-001, `Camera.h`). Heading zero
  // looks along +x, which is the same convention `EntityTransform` uses -- and this is that
  // transform, folded into the vertex stage so the processor sends four floats rather than a matrix
  // an entity.
  //
  // NOTHING SCALES. ADR-005: the authored extent IS the object's size and is never scaled at draw
  // time, so there is no scale factor here to get wrong.
  float3 world;
  world.x = (_input.local.x * c) - (_input.local.y * s) + _input.instanceTransform.x;
  world.y = (_input.local.x * s) + (_input.local.y * c) + _input.instanceTransform.y;
  world.z = _input.local.z;

  // The same rotation, and no translation. It stays unit because a rotation is a rotation.
  float3 normal;
  normal.x = (_input.normal.x * c) - (_input.normal.y * s);
  normal.y = (_input.normal.x * s) + (_input.normal.y * c);
  normal.z = _input.normal.z;

  Output output;
  output.position = mul(float4(world, 1.0f), g_viewProjection);
  output.normal = normal;
  output.shade = _input.shade;
  output.team = _input.instanceTeam.rgb;
  output.brightness = _input.instanceTeam.w;
  return output;
}
