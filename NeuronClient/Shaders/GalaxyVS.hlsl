// ADR-019's galaxy, the bake's vertex half: one triangle that overhangs the viewport, generated from
// the vertex identifier with no vertex buffer, exactly as `PresentVS` does.
//
// **THIS RUNS SIX TIMES AND ONLY ONCE**, at match start -- once per cube face, into a 512 square
// render target. Every frame after that samples the result with one fetch, which is the whole reason
// the band is baked rather than evaluated: the dust-lane noise below is far too expensive to pay per
// pixel per frame and does not change, because the sky takes no time input at all.
struct Output
{
  float4 position : SV_Position;

  /// Across the face, zero to one. The pixel stage turns this into a world direction using the face
  /// basis it is given -- which is what makes one shader serve all six faces.
  float2 texcoord : TEXCOORD0;
};

Output main(uint _vertexId : SV_VertexID)
{
  Output output;
  output.texcoord = float2((_vertexId << 1) & 2, _vertexId & 2);
  output.position = float4((output.texcoord * float2(2.0f, -2.0f)) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
  return output;
}
