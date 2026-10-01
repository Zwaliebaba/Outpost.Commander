// The interface: quads given in back-buffer pixels from the top-left corner, placed on the screen (ADR-015).
// VertexOut is declared again, identically, in UiPS.hlsl, and the input must match UiPipeline::Vertex.

cbuffer Screen : register(b0)
{
  float2 sizePixels;
  float2 unused;
};

struct VertexIn
{
  float2 position : POSITION;
  float2 texel : TEXCOORD;
  float4 color : COLOR;
};

struct VertexOut
{
  float4 position : SV_Position;
  float2 texel : TEXCOORD;
  float4 color : COLOR;
};

VertexOut main(VertexIn input)
{
  VertexOut output;
  output.position = float4((input.position.x / sizePixels.x) * 2.0f - 1.0f, 1.0f - (input.position.y / sizePixels.y) * 2.0f, 0.0f, 1.0f);
  output.texel = input.texel;
  output.color = input.color;
  return output;
}
