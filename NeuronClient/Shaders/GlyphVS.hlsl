// M1.13's interface quads, the vertex half: one instanced draw for every plate and every glyph in the
// interface, in the order they were emitted. The corners come from `SV_VertexID` exactly as they do in
// `InterfaceVS`; what varies per instance is a back-buffer rectangle, an atlas rectangle and a color.
//
// **POSITIONS ARRIVE IN PHYSICAL PIXELS, ALREADY WHOLE.** `LayoutText` snapped them and the interface fit
// put them there, so this stage only turns pixels into clip space and never rounds. A glyph was
// rasterized at exactly the size it is drawn (ADR-011), and nothing here scales it.
cbuffer Target : register(b0)
{
  /// x and y are the back buffer's size in pixels; z and w are unused.
  float4 g_target;
};

struct Input
{
  /// Left, top, right, bottom, in back-buffer pixels.
  float4 rect : TEXCOORD0;

  /// u0, v0, u1, v1 into the atlas.
  float4 texture : TEXCOORD1;

  /// Straight alpha, and the bytes the palette states -- see `Neuron::GlyphQuad`.
  float4 color : TEXCOORD2;
};

struct Output
{
  float4 position : SV_Position;
  float2 texture : TEXCOORD0;
  float4 color : TEXCOORD1;
};

Output main(Input _input, uint _vertexId : SV_VertexID)
{
  // A strip of four: 0 top left, 1 top right, 2 bottom left, 3 bottom right.
  const bool atRight = (_vertexId & 1u) != 0u;
  const bool atBottom = (_vertexId & 2u) != 0u;

  const float2 pixel = float2(atRight ? _input.rect.z : _input.rect.x, atBottom ? _input.rect.w : _input.rect.y);
  const float2 size = max(g_target.xy, float2(1.0f, 1.0f));

  // **THE Y AXIS FLIPS HERE**, as it does in `ToClipRect`: pixels count down from the top and clip space
  // counts up from the center.
  Output output;
  output.position = float4(((pixel.x / size.x) * 2.0f) - 1.0f, 1.0f - ((pixel.y / size.y) * 2.0f), 0.0f, 1.0f);
  output.texture = float2(atRight ? _input.texture.z : _input.texture.x, atBottom ? _input.texture.w : _input.texture.y);
  output.color = _input.color;
  return output;
}
