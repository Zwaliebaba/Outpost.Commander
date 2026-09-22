// M1.13's interface quads, the pixel half: the atlas's coverage times the instance's alpha. A plate
// samples the atlas's block of full coverage and comes out as its color; a glyph samples its own slot and
// comes out as antialiased text. One shader for both is what makes the interface a single draw.
Texture2D<float> g_atlas : register(t0);

// **POINT, AND THAT IS NOT A SHORTCUT.** A glyph is drawn at exactly the size it was rasterized and at a
// whole-pixel position, so each fragment's texture coordinate lands in the middle of one texel. A linear
// filter here would blur a stem into its neighbor for nothing.
SamplerState g_point : register(s0);

float4 main(float4 _position : SV_Position, float2 _texture : TEXCOORD0, float4 _color : TEXCOORD1) : SV_Target
{
  const float coverage = g_atlas.Sample(g_point, _texture);
  return float4(_color.rgb, _color.a * coverage);
}
