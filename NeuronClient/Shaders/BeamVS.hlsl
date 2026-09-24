// A beam, the vertex half: **one instanced quad from one world point to another**, whose four corners come from
// the vertex identifier rather than from a vertex buffer. It draws a tracer (ADR-004) and a mining beam, and it
// knows neither: two points, a width and a color arrive per instance.
//
// **THE WIDTH IS IN WORLD UNITS AND LIES ACROSS THE BEAM ON THE PLANE**, so a beam narrows with distance the way
// the hulls it joins do. It is not turned to face the camera: from the steep angles `Interface.md` section 5's
// camera uses, a quad flat on the plane reads as a line, and from the shallowest it thins, which is honest.

cbuffer Beams : register(b0)
{
  /// View times projection, row-major, the same matrix the world's hulls are drawn with.
  row_major float4x4 g_viewProjection;
};

struct Input
{
  float3 from : TEXCOORD0;
  float width : TEXCOORD1;
  float3 to : TEXCOORD2;

  /// Already scaled by the beam's brightness, because the pass blends additively.
  float3 color : TEXCOORD3;
};

struct Output
{
  float4 position : SV_Position;

  /// Minus one to plus one across the beam. The pixel stage's soft edge is measured on this.
  float across : TEXCOORD0;

  float3 color : TEXCOORD1;
};

Output main(Input _input, uint _vertexId : SV_VertexID)
{
  // **A FOUR-VERTEX TRIANGLE STRIP**, as the stars are: bit 0 is which end, bit 1 which side.
  const float along = ((_vertexId & 1u) != 0u) ? 1.0f : 0.0f;
  const float side = ((_vertexId & 2u) != 0u) ? 1.0f : -1.0f;

  // Across the beam on the plane. A beam of no length has no direction; it draws nothing rather than a NaN.
  const float2 span = _input.to.xy - _input.from.xy;
  const float length2 = dot(span, span);
  const float2 normal = (length2 > 0.0f) ? (float2(-span.y, span.x) * rsqrt(length2)) : float2(0.0f, 0.0f);

  const float3 center = lerp(_input.from, _input.to, along);
  const float3 corner = center + float3(normal * (side * _input.width * 0.5f), 0.0f);

  // **THE VECTOR GOES ON THE LEFT**, as in every other world shader here: these matrices are row-major and apply
  // to row vectors.
  Output output;
  output.position = mul(float4(corner, 1.0f), g_viewProjection);
  output.across = side;
  output.color = _input.color;
  return output;
}
