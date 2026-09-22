// ADR-019's stars, the pixel half: **a soft radial falloff, because apparent size is the point-spread
// function and not the star**. Every star is a point source; what differs between the tiers is how far
// the eye and the sensor smear it, which is why the brightest tier is eight pixels across and the
// faintest is one and a half.
//
// **NOTHING TWINKLES AND THERE ARE NO DIFFRACTION SPIKES.** There is no atmosphere, so there is no
// scintillation; spikes are an artifact of a telescope's spider vanes and the player is not looking
// through one. Both are the kind of thing that arrives later as "a little movement" without anybody
// deciding it, so ADR-019 rules them out by name.

struct Input
{
  float4 position : SV_Position;

  /// Minus one to plus one across the quad.
  float2 offset : TEXCOORD0;

  /// Already desaturated and already scaled by the tier's value.
  float3 colour : TEXCOORD1;
};

float4 main(Input _input) : SV_Target
{
  const float radius = length(_input.offset);

  // Squared rather than linear: a linear falloff leaves a visible disc edge, which on three thousand
  // sprites reads as confetti rather than as a sky. Squaring puts the shoulder inside the quad so the
  // sprite fades to nothing before it reaches the corner.
  const float falloff = saturate(1.0f - radius);
  const float intensity = falloff * falloff;

  // **THE PASS BLENDS ADDITIVELY**, so the alpha channel is not what carries this and the colour is
  // premultiplied by the falloff here. A star over the baked band adds to it rather than replacing it,
  // which is what a real one does.
  return float4(_input.colour * intensity, 1.0f);
}
