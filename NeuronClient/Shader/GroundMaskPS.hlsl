// The ground mask: black, as opaque as the shade at this point of the ground, which is blended between the four nearest
// cells' centers so that no cell's edge shows. The blend darkens the scene under it.

cbuffer Frame : register(b0)
{
  row_major float4x4 viewProjection;
  float originXMeters;
  float originZMeters;
  float cellMeters;
  uint cellsPerSide;
};

struct VertexOut
{
  float4 position : SV_Position;
  float2 ground : TEXCOORD0;
};

// Row by row along x, rows in order of z.
StructuredBuffer<float> shades : register(t0);

float ShadeOf(int2 _cell)
{
  const int last = (int)cellsPerSide - 1;
  const uint2 cell = (uint2)clamp(_cell, int2(0, 0), int2(last, last));
  return shades[cell.y * cellsPerSide + cell.x];
}

float4 main(VertexOut input) : SV_Target
{
  // In cells, from the first cell's center.
  const float2 at = (input.ground - float2(originXMeters, originZMeters)) / cellMeters - 0.5f;
  const float2 corner = floor(at);
  const int2 low = (int2)corner;
  const float2 blend = at - corner;
  const float nearRow = lerp(ShadeOf(low), ShadeOf(low + int2(1, 0)), blend.x);
  const float farRow = lerp(ShadeOf(low + int2(0, 1)), ShadeOf(low + int2(1, 1)), blend.x);
  return float4(0.0f, 0.0f, 0.0f, lerp(nearRow, farRow, blend.y));
}
