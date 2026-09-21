// TRIAL FILE for ADR-012. The real present shader is M0.15's.
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
