// ADR-019's galaxy, the bake's pixel half: **a band brighter and wider toward the galactic centre,
// carrying dark dust lanes**, because a band without them reads as a stain rather than as a galaxy.
//
// Written into one face of a cubemap. The face's own basis arrives as constants, so this one shader
// serves all six and the caller decides which face it is drawing.
//
// **EVERYTHING HERE IS LARGE-AREA LUMINANCE AND SO SITS UNDER ADR-019's 12% CEILING.** The ceiling is
// on area rather than on peak for a reason: `ADR-005` leans on the backdrop being near-black to make a
// faceted hull read as deliberate, and a band that crept up to a quarter of white would take that with
// it one commit at a time. The luminances come from `GameClient/SkyLook.h`, which is where the figure
// is stated and checked.

cbuffer Face : register(b0)
{
  /// The cube face's basis in world space. `forward` is the face normal; `right` and `up` span it.
  float4 g_right;
  float4 g_up;
  float4 g_forward;
};

cbuffer Galaxy : register(b1)
{
  /// The galactic pole. The band lies perpendicular to it.
  float4 g_pole;

  /// Which way along the band the centre lies. The bulge sits here.
  float4 g_centre;

  /// thickness away from the centre, thickness at it, dust depth, dust frequency.
  float4 g_shape;

  /// r, g, b, luminance -- at the centre.
  float4 g_core;

  /// r, g, b, luminance -- at the rim.
  float4 g_rim;
};

struct Input
{
  float4 position : SV_Position;
  float2 texcoord : TEXCOORD0;
};

/// A hash of a lattice point to a float in [0, 1). Integer-free and cheap; this is a bake, but six
/// 512-square faces at several octaves is still 1.6 million pixels and there is no reason to be
/// wasteful about it.
float Hash(float3 _lattice)
{
  return frac(sin(dot(_lattice, float3(127.1f, 311.7f, 74.7f))) * 43758.5453123f);
}

/// Value noise on a lattice, smoothed with the usual Hermite curve so the lanes have no grid in them.
/// **A GRID IS THE FAILURE WORTH NAMING HERE**: linear interpolation between lattice points leaves
/// visible creases along the axes, which on a sky reads unmistakably as a rendering artifact.
float ValueNoise(float3 _point)
{
  const float3 cell = floor(_point);
  const float3 within = frac(_point);
  const float3 smoothed = within * within * (3.0f - (2.0f * within));

  const float c000 = Hash(cell + float3(0.0f, 0.0f, 0.0f));
  const float c100 = Hash(cell + float3(1.0f, 0.0f, 0.0f));
  const float c010 = Hash(cell + float3(0.0f, 1.0f, 0.0f));
  const float c110 = Hash(cell + float3(1.0f, 1.0f, 0.0f));
  const float c001 = Hash(cell + float3(0.0f, 0.0f, 1.0f));
  const float c101 = Hash(cell + float3(1.0f, 0.0f, 1.0f));
  const float c011 = Hash(cell + float3(0.0f, 1.0f, 1.0f));
  const float c111 = Hash(cell + float3(1.0f, 1.0f, 1.0f));

  const float x00 = lerp(c000, c100, smoothed.x);
  const float x10 = lerp(c010, c110, smoothed.x);
  const float x01 = lerp(c001, c101, smoothed.x);
  const float x11 = lerp(c011, c111, smoothed.x);

  return lerp(lerp(x00, x10, smoothed.y), lerp(x01, x11, smoothed.y), smoothed.z);
}

/// Four octaves, each half the amplitude and twice the frequency. **Dust lanes are structure at more
/// than one scale** -- a single octave gives evenly sized blobs, which reads as a texture rather than
/// as dust.
float FractalNoise(float3 _point)
{
  float total = 0.0f;
  float amplitude = 0.5f;
  float3 sample = _point;

  for (int octave = 0; octave < 4; ++octave)
  {
    total += ValueNoise(sample) * amplitude;
    sample *= 2.0f;
    amplitude *= 0.5f;
  }

  return total;
}

float4 main(Input _input) : SV_Target
{
  // The face's texel as a world direction. The vertical flip is the usual one: a texture's v counts
  // down and the face basis counts up.
  const float2 across = float2((_input.texcoord.x * 2.0f) - 1.0f, 1.0f - (_input.texcoord.y * 2.0f));
  const float3 direction = normalize(g_forward.xyz + (g_right.xyz * across.x) + (g_up.xyz * across.y));

  // **THE SINE OF THE GALACTIC LATITUDE IS THE WHOLE GEOMETRY.** Zero is on the band and plus or minus
  // one is at a pole.
  const float sineLatitude = dot(direction, normalize(g_pole.xyz));

  // How far around the band the centre is. One at the centre, minus one directly away from it; the
  // half-and-half remap gives a single smooth swell rather than two.
  const float towardCentre = (dot(direction, normalize(g_centre.xyz)) * 0.5f) + 0.5f;

  // **THE BULGE IS THE DIFFERENCE BETWEEN THE TWO THICKNESSES**, and a uniform thickness is the stain
  // this exists to avoid. Squaring the swell concentrates the widening near the centre instead of
  // spreading it over half the sky.
  const float swell = towardCentre * towardCentre;
  const float thickness = lerp(g_shape.x, g_shape.y, swell);

  // A Gaussian across the band. **Both sides of this divide are sines of the galactic latitude**, so
  // the result is in standard deviations and `thickness` is one of them -- see `GameClient/SkyLook.h`,
  // where the figures are, for what that is in degrees.
  const float fromPlane = sineLatitude / max(thickness, 0.0001f);
  const float band = exp(-0.5f * fromPlane * fromPlane);

  // Warm at the core and cooler at the rim, which is what a galaxy actually is: old red stars in the
  // bulge and young blue ones in the arms.
  const float3 colour = lerp(g_rim.rgb, g_core.rgb, swell);
  const float luminance = lerp(g_rim.a, g_core.a, swell);

  // **DUST LANES ARE SUBTRACTIVE AND THEY ARE NOT OPTIONAL.** They are also concentrated ON the band:
  // dust is in the disc, so lanes away from the plane would be a noise field rather than a galaxy. The
  // extra stretch across the plane makes them run ALONG the band, which is how they actually look.
  const float3 lanePoint = float3(direction.x, direction.y, direction.z) * g_shape.w;
  const float3 stretched = lanePoint + (normalize(g_pole.xyz) * dot(lanePoint, normalize(g_pole.xyz)) * 2.0f);
  const float lane = FractalNoise(stretched);

  // Only the darker half of the noise cuts, so the band keeps its brightest parts intact instead of
  // being uniformly dimmed -- a flat multiply would just lower the luminance, which is not a dust lane.
  const float cut = saturate((lane - 0.45f) * 2.2f);
  const float dust = 1.0f - (g_shape.z * cut * band);

  return float4(colour * luminance * band * dust, 1.0f);
}
