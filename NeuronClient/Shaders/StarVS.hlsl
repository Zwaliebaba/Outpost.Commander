// ADR-019's stars, the vertex half: **about 3,000 instanced quads**, one draw call, whose four corners
// come from the vertex identifier rather than from a vertex buffer. Only the per-star instance data is
// in memory -- a direction, a size and an already-lit colour, thirty-two bytes each, which is about 94
// KiB for the whole sky.
//
// **SIZE IS IN SCENE-TARGET PIXELS AND SO THE WORLD SCALE REACHES IT** (ADR-016): the target's size
// arrives as a constant and the quad is built in normalized space, so a star is the same size on the
// glass at every world resolution. A quad sized in world units would grow and shrink with the zoom,
// which is the one thing a star at infinity must not do.
//
// **NOTHING HERE TAKES A TIME.** ADR-019 is explicit: no twinkle, no shimmer, no per-frame update.

cbuffer Sky : register(b0)
{
  /// View times projection with the TRANSLATION REMOVED, row-major. The sky rotates with the camera
  /// and does not translate with it (ADR-018), so the eye is the origin as far as this shader knows.
  row_major float4x4 g_viewRotationProjection;

  /// x and y are the scene target's size in pixels; z and w are unused.
  float4 g_target;
};

struct Input
{
  /// World space, on the unit sphere.
  float3 direction : TEXCOORD0;

  /// In scene-target pixels.
  float size : TEXCOORD1;

  /// Already desaturated and already scaled by the tier's value, so the pixel stage multiplies by a
  /// falloff and nothing else.
  float3 colour : TEXCOORD2;
};

struct Output
{
  float4 position : SV_Position;

  /// Minus one to plus one across the quad. The pixel stage's radial falloff is measured on this.
  float2 offset : TEXCOORD0;

  float3 colour : TEXCOORD1;
};

Output main(Input _input, uint _vertexId : SV_VertexID)
{
  // **A FOUR-VERTEX TRIANGLE STRIP**, so the corner is two bits of the identifier and there is no
  // lookup table and no branch. A six-vertex list would need one of the two, and the mapping from six
  // identifiers back to four corners is easy to write down wrong -- it is not `id` and `id - 1`.
  const float2 quad = float2(((_vertexId & 1u) != 0u) ? 1.0f : -1.0f, ((_vertexId & 2u) != 0u) ? 1.0f : -1.0f);

  // **THE STAR IS AT INFINITY AND THE DISTANCE BELOW DOES NOT SET ITS DEPTH.** Any positive distance
  // puts it in front of the camera with a correct w for the perspective divide; the depth is then
  // forced to the far plane outright, so the choice of distance cannot drift into a star that clips
  // against the far plane at one zoom and not another.
  // **THE VECTOR GOES ON THE LEFT.** These matrices are row-major and applied to ROW vectors, exactly
  // as `ShipVS` and `WorldVS` do it -- `mul(matrix, vector)` compiles perfectly well and silently
  // applies the transpose, which scatters every star to a plausible-looking wrong place.
  //
  // The `1.0f` multiplies the matrix's last row, which the caller has zeroed to remove the camera's
  // translation -- so it contributes nothing and a direction may be passed as a position.
  float4 clip = mul(float4(_input.direction * 1000.0f, 1.0f), g_viewRotationProjection);

  // A pixel is two normalized units over the target's width, and adding `delta * w` before the divide
  // shifts the result by exactly `delta` after it. So the quad keeps its size in pixels regardless of
  // how far away the arithmetic above decided the star was.
  //
  // **THE SIZE IS A DIAMETER AND THE QUAD RUNS MINUS ONE TO PLUS ONE**, so it is halved here. Without
  // the half every sprite is twice as wide as ADR-019 says and covers four times the area -- and
  // `Neuron::LitAreaFraction`, which is what the 12% ceiling is actually checked against, measures the
  // radius as half of this same figure. The two have to mean the same thing or the assertion is over a
  // sky nobody drew.
  const float2 pixels = (quad * _input.size * 0.5f) / max(g_target.xy, float2(1.0f, 1.0f));
  clip.xy += pixels * 2.0f * clip.w;

  // Exactly the far plane, matched by a LESS_EQUAL test -- see `SkyVS`.
  clip.z = clip.w;

  Output output;
  output.position = clip;
  output.offset = quad;
  output.colour = _input.colour;
  return output;
}
