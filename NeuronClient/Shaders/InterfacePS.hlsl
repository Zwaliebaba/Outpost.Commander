// ADR-011's interface pass, the pixel half: the color it was handed, and nothing else. A plate, a
// bar and a text quad's background are all this shader; the glyph atlas at M1.12 is what first
// needs a second one.
cbuffer QuadColor : register(b1)
{
  float4 g_color;
};

float4 main(float4 _position : SV_Position) : SV_Target
{
  return g_color;
}
