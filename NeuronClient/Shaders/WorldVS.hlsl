// M0.21b's world pass, the vertex half: one arrow per entity, from five vertices and no vertex
// buffer. The transform arrives as root constants already multiplied out on the processor -- world
// times view times projection -- so nothing here composes matrices and nothing here knows where the
// camera is.
//
// ROW_MAJOR IS NOT OPTIONAL. HLSL packs a float4x4 column-major by default and `Camera.h`'s Matrix4
// is row-major, so without this word every frame would need a transpose on the processor to say the
// same thing. One word here against work on every entity.
cbuffer WorldTransform : register(b0)
{
  row_major float4x4 g_worldViewProjection;
};

struct Output
{
  float4 position : SV_Position;
};

Output main(uint _vertexId : SV_VertexID)
{
  // AN ARROW RATHER THAN A SQUARE, and the reason is in the plan: a heading drawn wrongly has to be
  // visible. A square turns and looks identical at every heading, so it would hide exactly the bug
  // M0.19's heading interpolation could have.
  //
  // Five vertices as a triangle strip: a long nose at +X -- heading zero points along +X, matching
  // `Camera.h` -- and a notched tail, which makes the back distinguishable from the front as well
  // as the sides from each other.
  //
  // IT LIES IN THE PLANE, at Z = 0 in local space. The entity transform rotates about Z and
  // translates within the plane, so the shape stays on it (ADR-001: the simulation is
  // two-dimensional and nothing is simulated above or below).
  float2 local;
  switch (_vertexId)
  {
  case 0:
    local = float2(30.0f, 0.0f);     // the nose
    break;
  case 1:
    local = float2(-18.0f, 14.0f);   // port quarter
    break;
  case 2:
    local = float2(-8.0f, 0.0f);     // the tail notch
    break;
  case 3:
    local = float2(-18.0f, -14.0f);  // starboard quarter
    break;
  default:
    local = float2(30.0f, 0.0f);     // back to the nose, closing the strip
    break;
  }

  Output output;
  output.position = mul(float4(local.x, local.y, 0.0f, 1.0f), g_worldViewProjection);
  return output;
}
