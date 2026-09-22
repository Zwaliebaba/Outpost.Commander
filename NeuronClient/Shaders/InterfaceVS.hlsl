// ADR-011's interface pass, the vertex half: one rectangle from four vertices and no vertex buffer.
// The rectangle arrives as four root constants already in clip space -- the authored coordinates
// went through the INTERFACE fit on the processor, in integers, so nothing here knows the window's
// size and nothing here rounds.
cbuffer ClipRect : register(b0)
{
  // Left, top, right, bottom. TOP IS THE LARGER Y: clip space counts upwards from the center where
  // a back buffer counts rows downwards from the top, and `InterfacePass.cpp` is where that flips.
  float4 g_clipRect;
};

struct Output
{
  float4 position : SV_Position;
};

Output main(uint _vertexId : SV_VertexID)
{
  // A strip of four, in the order a strip wants them: 0 top left, 1 top right, 2 bottom left,
  // 3 bottom right. Two triangles, no index buffer, no shared diagonal drawn twice.
  const bool atRight = (_vertexId & 1u) != 0u;
  const bool atBottom = (_vertexId & 2u) != 0u;

  Output output;
  output.position = float4(atRight ? g_clipRect.z : g_clipRect.x, atBottom ? g_clipRect.w : g_clipRect.y, 0.0f, 1.0f);
  return output;
}
