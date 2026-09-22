#pragma once

#include "Blackbody.h"
#include "NeuronCore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Neuron
{

/// A point field on a unit sphere, from a seed.
/// [`ADR-019`](../Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md): **about 3,000 instanced
/// quads**, one draw call, generated once and never updated.
///
/// **IT IS THE ENGINE'S AND IT KNOWS NOTHING ABOUT THIS SKY** (R9). The tier ratios, the sizes, the
/// temperatures, where the galactic plane points and how far the colour is desaturated all arrive as a
/// description; `GameClient/SkyLook.h` is what fills one in.
///
/// **SIX TIERS, AND THE BRIGHTEST HOLDING EIGHT IS THE WHOLE EFFECT.** Real star counts multiply by
/// about 2.5 a magnitude step, so the ratio is **1 : 3 : 9 : 27 : 81 : 243**. A field of uniformly
/// bright dots reads as noise; a field with a handful of standouts reads as a sky.
///
/// **NOTHING HERE TAKES A TIME.** ADR-019 is explicit: no twinkle, no shimmer, no per-frame update. The
/// camera orbits and pitches, so the sky already moves against the frame, and "add a little movement"
/// is the kind of thing that arrives later without anybody deciding it.

inline constexpr std::size_t MAGNITUDE_TIER_COUNT = 6;

/// **1 : 3 : 9 : 27 : 81 : 243**, brightest first. They sum to 364, which divides no round number of
/// stars evenly -- 3,000 lands on 8.24, 24.7, 74.2, 222.5, 667.6 and 2,002.7. Rounded to nearest with
/// the leftover given to the faintest tier, that is **8, 25, 74, 223, 668 and 2,002**, and `TierCounts`
/// below is where that happens.
inline constexpr std::array<std::uint32_t, MAGNITUDE_TIER_COUNT> MAGNITUDE_RATIO{1, 3, 9, 27, 81, 243};

/// What a star field should look like. **Every figure in it belongs to whoever fills it in.**
///
/// R8: a public aggregate.
struct StarFieldDescription
{
  /// ADR-019's "about 3,000", and the reason it is not arbitrary: the whole naked-eye sky to magnitude
  /// six holds roughly 5,000 to 6,000 stars, so a realistic count and a cheap count are the same count.
  std::uint32_t starCount = 3000;

  /// In scene-target pixels, brightest end first and faintest last, and **every value between them
  /// occurs** -- the size is drawn continuously rather than taken from the star's tier. The sizes are
  /// the point-spread function rather than the star.
  ///
  /// **THE FAINT END IS A RASTERIZATION FLOOR AND NOT A TASTE.** ADR-019 said 1.5, and a 1.5-pixel
  /// quad whose radial falloff reaches zero at its own edge is a sub-pixel dot: it covers one pixel
  /// centre at best, at a fraction of its peak, and most of the faint tiers simply did not appear on
  /// the device. Below about two pixels a sprite is not dim, it is absent -- and a star that vanishes
  /// is worse than one that is too bright, because nothing on the screen says it was meant to be
  /// there. 2.4 cleared the floor and was still not seen, because the nearest pixel centre got a third
  /// of the peak; 3.0 with `StarPS.hlsl`'s flat-topped falloff gets it about three quarters.
  float brightestSizePixels = 10.0f;
  float faintestSizePixels = 3.0f;

  /// The value the centre of a sprite reaches, as a fraction of full white. **ADR-019 caps the
  /// brightest at 0.45** and puts the ceiling on lit AREA rather than on peak.
  ///
  /// The faint end was 0.08, then 0.18, and neither was seen on the device -- first because a band
  /// outshone it and then because the sprite never delivered its peak to a pixel. See
  /// `GameClient/SkyLook.cpp` for the derivation of the figure it has now.
  float brightestValue = 0.45f;
  float faintestValue = 0.24f;

  /// **TEMPERATURE CORRELATES WITH BRIGHTNESS, AND THAT CORRELATION IS THE DETAIL THAT SELLS IT.** Hot
  /// stars are luminous, so the bright tiers skew blue-white and the faint ones orange; drawing colour
  /// independently of magnitude gives a sky that is subtly, unnameably wrong.
  /// **THE RANGE IS WIDER THAN THE REAL ONE ON PURPOSE**, because the desaturation below throws most
  /// of it away again. 3,000 K is an orange dwarf and 15,000 K is a hot blue-white B star; both exist
  /// in quantity and the pair spans what the naked eye can actually tell apart.
  float brightestKelvin = 15000.0f;
  float faintestKelvin = 3000.0f;

  /// How far a star's temperature may wander from the one its magnitude implies, in kelvin. **The
  /// correlation is a tendency and not a rule** -- a red supergiant is bright AND cool -- so without a
  /// wide jitter the sky is a colour gradient by brightness, which is a pattern the eye picks out.
  float temperatureJitterKelvin = 1600.0f;

  /// How much of the blackbody tint survives. ADR-019 said "roughly 20%", which with the old narrow
  /// temperature range left every star the same off-white -- the tints were there and none of them was
  /// visible. **Real stars do read very nearly white, but not identically white**, and the difference
  /// between those two is the whole texture of a sky.
  float saturation = 0.38f;

  /// **STAR DENSITY RISES TOWARD THE GALACTIC PLANE**, which is what ties the two halves of the sky
  /// into one thing rather than a star field with a stripe painted over it. One is a uniform sphere;
  /// above one pulls stars toward the plane, and the exponent is applied to the sine of the latitude.
  float planeConcentration = 2.3f;

  /// The galactic pole, in world space -- the axis the plane lies perpendicular to. Normalized on use,
  /// so a caller may state a direction rather than a unit vector.
  float poleX = 0.0f;
  float poleY = 0.35f;
  float poleZ = 0.94f;
};

/// One star, ready to be an instance.
///
/// R8: a public aggregate.
struct Star
{
  /// A unit direction in world space. **The sky is fixed in world space**, which is what makes it a
  /// compass: it rotates with heading and pitch and does not translate with pan (ADR-019, ADR-018).
  float directionX = 0.0f;
  float directionY = 0.0f;
  float directionZ = 1.0f;

  /// In scene-target pixels, so the world scale reaches it (ADR-016).
  float sizePixels = 1.5f;

  /// Already desaturated, and already scaled by the tier's value -- so the sprite shader multiplies by
  /// a falloff and nothing else.
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;

  /// Which tier it came from, brightest first. Carried so a suite can count them.
  std::uint8_t tier = 0;

  [[nodiscard]] friend constexpr bool operator==(const Star&, const Star&) noexcept = default;
};

/// How many stars each tier gets, for a total. **The remainder goes to the FAINTEST tier**, because
/// putting it in the brightest would change eight into nine and eight is the effect.
[[nodiscard]] std::array<std::uint32_t, MAGNITUDE_TIER_COUNT> TierCounts(std::uint32_t _starCount) noexcept;

/// The field. **The same seed gives the same sky**, which is what lets two players share one for free
/// and is the property a suite asserts by running it twice.
[[nodiscard]] std::vector<Star> GenerateStarField(std::uint64_t _seed, const StarFieldDescription& _description);

/// **THE LIT AREA THE FIELD ADDS, AS A FRACTION OF THE FRAME** -- ADR-019's ceiling is on area rather
/// than on peak, and this is the number that ceiling is about.
///
/// A sprite's soft radial falloff means it does not contribute its whole disc. `StarPS.hlsl`'s
/// `1 - smoothstep(0, 1, r)` integrates to exactly 0.30 of the disc -- 2 x (1/2 - 3/4 + 2/5) -- which
/// is the constant below. It is still an estimate rather than a measurement of the rendered frame,
/// because rasterization samples the falloff at pixel centres rather than integrating it.
[[nodiscard]] float LitAreaFraction(const std::vector<Star>& _stars, std::uint32_t _framePixels) noexcept;

/// **PCG32's STREAM FOR THE SKY.** `GameLogic/Sessions.h` took stream 1 and said M2's generator owns
/// the rest; this takes 2 and says the same. Two generators differing only in stream never produce the
/// same sequence, which is what stops the sky and the session tokens advancing one stream between them.
inline constexpr std::uint64_t STAR_FIELD_STREAM = 2;

} // namespace Neuron
