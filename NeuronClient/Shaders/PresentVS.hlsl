// R13's scaled present, the vertex half: one triangle that overhangs the viewport, generated from
// the vertex identifier, with no vertex buffer and no input layout. Where it lands on the back
// buffer is the viewport, which is M0.12's world fit -- nothing here knows the window's size.
struct Output
{
  float4 position : SV_Position;
  float2 texcoord : TEXCOORD0;
};

Output main(uint _vertexId : SV_VertexID)
{
  Output output;
  output.texcoord = float2((_vertexId << 1) & 2, _vertexId & 2);
  output.position = float4((output.texcoord * float2(2.0f, -2.0f)) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
  return output;
}
