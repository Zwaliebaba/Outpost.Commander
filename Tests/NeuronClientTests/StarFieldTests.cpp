#include "pch.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
constexpr std::uint64_t SEED = 20260922;

/// The scene target at ADR-016's 1:1 default.
constexpr std::uint32_t FRAME_PIXELS = 2880 * 1920;

[[nodiscard]] Neuron::StarFieldDescription Shipped() noexcept
{
  return Neuron::StarFieldDescription{};
}

[[nodiscard]] std::array<std::uint32_t, Neuron::MAGNITUDE_TIER_COUNT> CountByTier(const std::vector<Neuron::Star>& _stars)
{
  std::array<std::uint32_t, Neuron::MAGNITUDE_TIER_COUNT> counts{};
  for (const Neuron::Star& star : _stars)
  {
    if (star.tier < Neuron::MAGNITUDE_TIER_COUNT)
    {
      ++counts[star.tier];
    }
  }
  return counts;
}
} // namespace

/// ADR-019's magnitude law. **The brightest tier holding eight is the whole effect**: a field of
/// uniformly bright dots reads as noise and a field with a handful of standouts reads as a sky.
TEST_CLASS(TheMagnitudeTiers)
{
public:
  /// **1 : 3 : 9 : 27 : 81 : 243**, which sums to 364 and so divides NO round number of stars evenly.
  /// 3,000 lands on 8.24, 24.7, 74.2, 222.5, 667.6 and 2,002.7; rounded to nearest with the leftover
  /// given to the faintest tier, that is 8, 25, 74, 223, 668 and 2,002.
  ///
  /// An earlier draft of this suite asserted 222 and 667 against a truncating implementation and called
  /// the result exact. It was neither: truncation gave tier two 24 where it had earned 24.7, and the
  /// three stars it lost went to the faintest tier along with the rest of the remainder.
  TEST_METHOD(ThreeThousandRoundsIntoTheRatioWithTheLeftoverAtTheFaintEnd)
  {
    const std::array<std::uint32_t, Neuron::MAGNITUDE_TIER_COUNT> counts = Neuron::TierCounts(3000);

    Assert::AreEqual(8u, counts[0]);
    Assert::AreEqual(25u, counts[1]);
    Assert::AreEqual(74u, counts[2]);
    Assert::AreEqual(223u, counts[3]);
    Assert::AreEqual(668u, counts[4]);
    Assert::AreEqual(2002u, counts[5]);

    std::uint32_t total = 0;
    for (const std::uint32_t count : counts)
    {
      total += count;
    }
    Assert::AreEqual(3000u, total);
  }

  /// The ratio itself, so a change to it is a change somebody made rather than one that crept in.
  TEST_METHOD(TheRatioIsTheOneTheDesignStates)
  {
    Assert::AreEqual(static_cast<std::size_t>(6), Neuron::MAGNITUDE_TIER_COUNT);
    Assert::AreEqual(1u, Neuron::MAGNITUDE_RATIO[0]);
    Assert::AreEqual(3u, Neuron::MAGNITUDE_RATIO[1]);
    Assert::AreEqual(9u, Neuron::MAGNITUDE_RATIO[2]);
    Assert::AreEqual(27u, Neuron::MAGNITUDE_RATIO[3]);
    Assert::AreEqual(81u, Neuron::MAGNITUDE_RATIO[4]);
    Assert::AreEqual(243u, Neuron::MAGNITUDE_RATIO[5]);
  }

  /// **A REMAINDER GOES TO THE FAINTEST TIER**, because putting it in the brightest would turn eight
  /// standouts into nine and eight is the effect.
  ///
  /// Said as the property rather than as one tier's arithmetic: **every tier but the faintest holds
  /// exactly its rounded share**, so the faintest is the only one that can differ from its share, and
  /// what it differs by is the whole remainder. An earlier draft asserted `count / 364` for the
  /// brightest tier, which pinned TRUNCATION rather than the claim in this test's name -- and so failed
  /// when the rounding it was never about was corrected.
  TEST_METHOD(ARemainderNeverReachesTheBrightestTier)
  {
    for (std::uint32_t count = 364; count < 4000; count += 37)
    {
      const std::array<std::uint32_t, Neuron::MAGNITUDE_TIER_COUNT> counts = Neuron::TierCounts(count);

      std::uint32_t total = 0;
      for (const std::uint32_t tier : counts)
      {
        total += tier;
      }
      Assert::AreEqual(count, total, L"the tiers did not account for every star");

      for (std::size_t tier = 0; (tier + 1) < Neuron::MAGNITUDE_TIER_COUNT; ++tier)
      {
        const std::uint32_t share = ((count * Neuron::MAGNITUDE_RATIO[tier]) + 182u) / 364u;
        Assert::AreEqual(share, counts[tier], L"a tier above the faintest did not hold its rounded share");
      }
    }
  }

  /// Every tier is strictly larger than the one above it, which is what the ratio is for.
  TEST_METHOD(EachTierIsLargerThanTheOneAboveIt)
  {
    const std::array<std::uint32_t, Neuron::MAGNITUDE_TIER_COUNT> counts = Neuron::TierCounts(3000);
    for (std::size_t tier = 1; tier < Neuron::MAGNITUDE_TIER_COUNT; ++tier)
    {
      Assert::IsTrue(counts[tier] > counts[tier - 1], L"a fainter tier held no more stars than a brighter one");
    }
  }
};

/// The field itself.
TEST_CLASS(TheStarField)
{
public:
  /// **THE SAME SEED GIVES THE SAME SKY**, which is what lets two players share one for nothing and is
  /// asserted by running it twice rather than trusted.
  TEST_METHOD(TheSameSeedGivesTheSameSkyTwice)
  {
    const std::vector<Neuron::Star> first = Neuron::GenerateStarField(SEED, Shipped());
    const std::vector<Neuron::Star> second = Neuron::GenerateStarField(SEED, Shipped());
    Assert::IsTrue(first == second, L"the generator disagreed with itself");
  }

  TEST_METHOD(ADifferentSeedGivesADifferentSky)
  {
    const std::vector<Neuron::Star> first = Neuron::GenerateStarField(SEED, Shipped());
    const std::vector<Neuron::Star> second = Neuron::GenerateStarField(SEED + 1, Shipped());
    Assert::IsFalse(first == second);
  }

  /// The tiers come out of the generator in the ratio, which is a different claim from `TierCounts`
  /// producing them.
  TEST_METHOD(TheGeneratedFieldHoldsTheTiersInTheRatio)
  {
    const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(SEED, Shipped());
    Assert::AreEqual(static_cast<std::size_t>(3000), stars.size());

    const std::array<std::uint32_t, Neuron::MAGNITUDE_TIER_COUNT> counts = CountByTier(stars);
    Assert::AreEqual(8u, counts[0]);
    Assert::AreEqual(25u, counts[1]);
    Assert::AreEqual(74u, counts[2]);
    Assert::AreEqual(223u, counts[3]);
    Assert::AreEqual(668u, counts[4]);
    Assert::AreEqual(2002u, counts[5]);
  }

  /// Every direction is on the unit sphere. A star off it would be at the wrong angular size or, at
  /// zero, nowhere at all.
  TEST_METHOD(EveryDirectionIsAUnitVector)
  {
    for (const Neuron::Star& star : Neuron::GenerateStarField(SEED, Shipped()))
    {
      const float length =
        std::sqrt((star.directionX * star.directionX) + (star.directionY * star.directionY) + (star.directionZ * star.directionZ));
      Assert::AreEqual(1.0f, length, 0.001f, L"a star left the unit sphere");
    }
  }

  /// **SIZE FOLLOWS BRIGHTNESS AND IS CONTINUOUS**, because apparent size is the point-spread function
  /// rather than the star -- and because a sky drawn at six sizes reads as six kinds of dot.
  ///
  /// This is the assertion that would have caught the first version on a machine rather than on a
  /// screen. It took one size per tier, so this whole field held exactly six distinct sizes; the test
  /// that stood here checked the two ends were 8 and 1.5 and passed, because both ends were right and
  /// the 2,998 stars between them were the problem.
  TEST_METHOD(EveryStarHasItsOwnSizeRatherThanItsTiers)
  {
    const Neuron::StarFieldDescription field = Shipped();
    const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(SEED, field);

    std::vector<float> sizes;
    sizes.reserve(stars.size());
    for (const Neuron::Star& star : stars)
    {
      Assert::IsTrue(star.sizePixels <= (field.brightestSizePixels + 0.001f), L"a star is larger than the bright end");
      Assert::IsTrue(star.sizePixels >= (field.faintestSizePixels - 0.001f), L"a star is smaller than the faint end");
      sizes.push_back(star.sizePixels);
    }

    // **A SPRITE BELOW ABOUT TWO PIXELS DOES NOT RASTERIZE**: its radial falloff reaches zero at its
    // own edge, so it covers no pixel centre at anything near its peak and the star is absent rather
    // than dim. This is the floor that failure bought.
    Assert::IsTrue(field.faintestSizePixels >= 2.0f, L"the faint end is below the rasterization floor");

    std::sort(sizes.begin(), sizes.end());
    const std::size_t distinct = static_cast<std::size_t>(std::distance(sizes.begin(), std::unique(sizes.begin(), sizes.end())));

    // Six would mean one size per tier, which is exactly the bug. The real figure is in the thousands;
    // a hundred is a threshold no per-tier implementation can reach and no continuous one can miss.
    Assert::IsTrue(distinct > 100, L"the sky is drawn at a handful of sizes rather than a continuum");
  }

  /// The same claim for brightness, which had the same bug for the same reason.
  TEST_METHOD(EveryStarHasItsOwnBrightnessRatherThanItsTiers)
  {
    const Neuron::StarFieldDescription field = Shipped();
    const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(SEED, field);

    std::vector<float> values;
    values.reserve(stars.size());
    for (const Neuron::Star& star : stars)
    {
      values.push_back(std::max(star.red, std::max(star.green, star.blue)));
    }

    std::sort(values.begin(), values.end());
    const std::size_t distinct = static_cast<std::size_t>(std::distance(values.begin(), std::unique(values.begin(), values.end())));
    Assert::IsTrue(distinct > 100, L"the sky is drawn at a handful of brightnesses rather than a continuum");
  }

  /// **TEMPERATURE CORRELATES WITH BRIGHTNESS, AND THAT CORRELATION IS THE DETAIL THAT SELLS IT.** The
  /// bright tiers skew blue-white and the faint ones orange; drawing colour independently of magnitude
  /// gives a sky that is subtly, unnameably wrong. Asserted as a mean over each tier, because the
  /// jitter means one star proves nothing.
  TEST_METHOD(TheBrightTiersSkewBlueAndTheFaintOnesOrange)
  {
    const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(SEED, Shipped());

    std::array<double, Neuron::MAGNITUDE_TIER_COUNT> blueOverRed{};
    std::array<std::uint32_t, Neuron::MAGNITUDE_TIER_COUNT> counts{};
    for (const Neuron::Star& star : stars)
    {
      blueOverRed[star.tier] += static_cast<double>(star.blue) / static_cast<double>(star.red);
      ++counts[star.tier];
    }

    double previous = 1000.0;
    for (std::size_t tier = 0; tier < Neuron::MAGNITUDE_TIER_COUNT; ++tier)
    {
      const double mean = blueOverRed[tier] / counts[tier];
      Assert::IsTrue(mean < previous, L"a fainter tier was not warmer than the one above it");
      previous = mean;
    }

    // And the two ends land either side of white, which is what "blue-white" and "orange" mean.
    Assert::IsTrue((blueOverRed[0] / counts[0]) > 1.0, L"the brightest tier is not blue-white");
    Assert::IsTrue((blueOverRed[Neuron::MAGNITUDE_TIER_COUNT - 1] / counts[Neuron::MAGNITUDE_TIER_COUNT - 1]) < 1.0,
                   L"the faintest tier is not warm");
  }

  /// **STAR DENSITY RISES TOWARD THE GALACTIC PLANE.** Counted as how many stars fall in the third of
  /// the sphere nearest the plane against a uniform sphere's share, which is exactly a third by area.
  TEST_METHOD(DensityRisesTowardThePlane)
  {
    Neuron::StarFieldDescription description = Shipped();
    const std::vector<Neuron::Star> biased = Neuron::GenerateStarField(SEED, description);

    description.planeConcentration = 1.0f;
    const std::vector<Neuron::Star> uniform = Neuron::GenerateStarField(SEED, description);

    const auto nearThePlane = [&description](const std::vector<Neuron::Star>& _stars)
    {
      const float length = std::sqrt((description.poleX * description.poleX) + (description.poleY * description.poleY) +
                                     (description.poleZ * description.poleZ));
      // **NOT `near`.** `<windows.h>` defines it as an empty macro, the same trap `TechnicalDesign.md`
      // section 1 names for `small` -- and it reaches a test file through the precompiled header.
      std::uint32_t within = 0;
      for (const Neuron::Star& star : _stars)
      {
        const float alongPole =
          ((star.directionX * description.poleX) + (star.directionY * description.poleY) + (star.directionZ * description.poleZ)) / length;
        if (std::fabs(alongPole) < (1.0f / 3.0f))
        {
          ++within;
        }
      }
      return within;
    };

    const std::uint32_t biasedNearPlane = nearThePlane(biased);
    const std::uint32_t uniformNearPlane = nearThePlane(uniform);

    // A uniform sphere puts exactly a third of its area within this band, so the unbiased field should
    // land near a thousand of three thousand -- and the biased one well above it.
    Assert::IsTrue(uniformNearPlane > 900, L"the unbiased field is not uniform");
    Assert::IsTrue(uniformNearPlane < 1100, L"the unbiased field is not uniform");
    Assert::IsTrue(biasedNearPlane > (uniformNearPlane + 200), L"the bias toward the plane is not visible");
  }

  /// **THE CEILING IS ON AREA RATHER THAN PEAK** (ADR-019 decision 4), and this is the number it is
  /// about. Three thousand sprites of a handful of pixels each come to a tiny fraction of the frame,
  /// which is what lets the brightest tier reach 45% at all.
  TEST_METHOD(TheFieldsLitAreaIsFarUnderTheCeiling)
  {
    const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(SEED, Shipped());
    const float fraction = Neuron::LitAreaFraction(stars, FRAME_PIXELS);

    Assert::IsTrue(fraction > 0.0f, L"the field lights nothing at all");
    Assert::IsTrue(fraction < 0.001f, L"the star field alone is approaching the area ceiling");
  }

  /// **NO STAR EXCEEDS THE POINT CEILING**, and only the brightest tier reaches it.
  TEST_METHOD(NoStarPassesFortyFivePercent)
  {
    for (const Neuron::Star& star : Neuron::GenerateStarField(SEED, Shipped()))
    {
      const float peak = std::max(star.red, std::max(star.green, star.blue));
      Assert::IsTrue(peak <= 0.4501f, L"a star passed the point ceiling");
      if (star.tier > 0)
      {
        Assert::IsTrue(peak < 0.4f, L"a tier below the brightest reached the point ceiling");
      }
    }
  }

  /// An empty field is an empty vector rather than a crash, which is what a count of zero should mean.
  TEST_METHOD(AnEmptyFieldIsEmpty)
  {
    Neuron::StarFieldDescription description = Shipped();
    description.starCount = 0;
    Assert::AreEqual(static_cast<std::size_t>(0), Neuron::GenerateStarField(SEED, description).size());
    Assert::AreEqual(0.0f, Neuron::LitAreaFraction({}, FRAME_PIXELS));
  }

  /// **A DEGENERATE POLE DOES NOT COLLAPSE THE SKY INTO A LINE.** The basis is built against the pole's
  /// smallest component for exactly this reason, and a zero pole falls back rather than dividing.
  TEST_METHOD(ADegeneratePoleStillGivesASphere)
  {
    for (const auto pole : {std::array<float, 3>{0.0f, 0.0f, 0.0f}, std::array<float, 3>{1.0f, 0.0f, 0.0f},
                            std::array<float, 3>{0.0f, 1.0f, 0.0f}, std::array<float, 3>{0.0f, 0.0f, 1.0f}})
    {
      Neuron::StarFieldDescription description = Shipped();
      description.poleX = pole[0];
      description.poleY = pole[1];
      description.poleZ = pole[2];

      const std::vector<Neuron::Star> stars = Neuron::GenerateStarField(SEED, description);
      Assert::AreEqual(static_cast<std::size_t>(3000), stars.size());
      for (const Neuron::Star& star : stars)
      {
        const float length =
          std::sqrt((star.directionX * star.directionX) + (star.directionY * star.directionY) + (star.directionZ * star.directionZ));
        Assert::AreEqual(1.0f, length, 0.001f);
      }
    }
  }

  /// **THE SKY TAKES STREAM 2 AND SAYS SO.** `GameLogic/Sessions.h` took 1; two generators differing
  /// only in stream never produce the same sequence, which is what stops the sky and the session tokens
  /// advancing one stream between them.
  TEST_METHOD(TheSkyHasItsOwnStream)
  {
    Assert::AreEqual(static_cast<std::uint64_t>(2), Neuron::STAR_FIELD_STREAM);
  }
};

} // namespace NeuronClientTests
