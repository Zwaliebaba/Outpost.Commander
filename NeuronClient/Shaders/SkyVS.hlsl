// ADR-019's backdrop, the vertex half: one overhanging triangle from the vertex identifier, carrying a
// world-space view ray to the pixel stage.
//
// **THE SKY IS FIXED IN WORLD SPACE, WHICH IS WHAT MAKES IT A COMPASS** (ADR-019, ADR-018). It rotates
// with heading and pitch and does NOT translate with pan -- so the camera's position never reaches
// this shader and only its three axes do. A sky that slid with the focus would read as a painted
// backdrop moving behind the fleet, which is precisely the tell this avoids.
//
// **DEPTH IS FORCED TO THE FAR PLANE** so the pass can be drawn LAST with the depth test on: it shades
// no pixel the fleet already covers, which is why it costs a fraction of a full-screen pass in a busy
// frame rather than all of one.

cbuffer Ray : register(b0)
{
  /// The camera's right axis, already scaled by the tangent of half the vertical field of view and by
  /// the aspect ratio. Adding it at a normalized x of one lands exactly on the frame's edge.
  float4 g_right;

  /// The up axis, scaled by the same tangent.
  float4 g_up;

  /// The forward axis, unit.
  float4 g_forward;
};

struct Output
{
  float4 position : SV_Position;

  /// World space, NOT normalized -- interpolating a normalized vector across a triangle does not give
  /// a normalized one, so the pixel stage normalizes rather than assuming.
  float3 direction : TEXCOORD0;
};

Output main(uint _vertexId : SV_VertexID)
{
  const float2 texcoord = float2((_vertexId << 1) & 2, _vertexId & 2);
  const float2 normalized = (texcoord * float2(2.0f, -2.0f)) + float2(-1.0f, 1.0f);

  Output output;

  // z of one against a w of one is a depth of exactly 1.0: the far plane. The pass uses LESS_EQUAL
  // rather than LESS, because the target clears to 1.0 and a strict test would reject every pixel --
  // a sky that is entirely absent, which looks like the pass never ran.
  output.position = float4(normalized, 1.0f, 1.0f);
  output.direction = g_forward.xyz + (g_right.xyz * normalized.x) + (g_up.xyz * normalized.y);
  return output;
}
