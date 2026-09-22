// ADR-019's stars, the pixel half: **a soft radial falloff, because apparent size is the point-spread
// function and not the star**. Every star is a point source; what differs between the tiers is how far
// the eye and the sensor smear it, which is why the brightest stars are ten pixels across and the
// faintest three.
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

  // **FLAT-TOPPED, NOT PEAKED.** A linear falloff leaves a visible disc edge, which on three thousand
  // sprites reads as confetti. The squared falloff this used before had no edge, but it had no top
  // either: on a three-pixel sprite the nearest pixel centre sits a third of the way out and received a
  // third of the peak, so two thirds of the sky rendered at about 16 of 255 and was not seen. The
  // Hermite curve has zero slope at both ends -- a core that holds near full value for the pixel that
  // lands on it, and a shoulder that still fades to nothing before the quad's edge.
  const float intensity = 1.0f - smoothstep(0.0f, 1.0f, radius);

  // **THE PASS BLENDS ADDITIVELY**, so the alpha channel is not what carries this and the colour is
  // premultiplied by the falloff here. Two stars that overlap add, which is what real ones do.
  return float4(_input.colour * intensity, 1.0f);
}
