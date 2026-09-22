#include "pch.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

TEST_CLASS(TheFrameStatistics)
{
public:
  TEST_METHOD(AnEmptyRunReportsNothingRatherThanZero)
  {
    // A mean of no samples is not zero microseconds, and a gate that read it as one would write a
    // figure into ADR-016 that no frame ever achieved. Count is what says whether there is a
    // figure at all.
    const Neuron::FrameStatistics statistics;
    Assert::AreEqual(0ULL, statistics.Count());
    Assert::AreEqual(0ULL, statistics.MeanMicroseconds());
  }

  TEST_METHOD(ItTakesTheMeanTheMinimumAndTheMaximum)
  {
    Neuron::FrameStatistics statistics;
    for (const std::uint64_t sample : {std::uint64_t{400}, std::uint64_t{600}, std::uint64_t{500}})
    {
      statistics.Add(sample);
    }

    Assert::AreEqual(3ULL, statistics.Count());
    Assert::AreEqual(500ULL, statistics.MeanMicroseconds());
    Assert::AreEqual(400ULL, statistics.MinimumMicroseconds());
    Assert::AreEqual(600ULL, statistics.MaximumMicroseconds());
  }

  TEST_METHOD(TheMeanRoundsRatherThanTruncating)
  {
    // 401 and 402 average to 401.5. Truncation would report 401 and would bias every figure this
    // class produces the same way -- which for a measurement quoted in a design document matters
    // more than the half microsecond does.
    Neuron::FrameStatistics statistics;
    statistics.Add(401);
    statistics.Add(402);
    Assert::AreEqual(402ULL, statistics.MeanMicroseconds());
  }

  TEST_METHOD(AZeroSampleIsDiscardedRatherThanAveragedIn)
  {
    // A timestamp pair that straddles nothing reads as zero. Those are measurement artifacts and
    // counting them drags the mean toward a frame time no frame had.
    Neuron::FrameStatistics statistics;
    statistics.Add(0);
    statistics.Add(1000);
    statistics.Add(0);

    Assert::AreEqual(1ULL, statistics.Count());
    Assert::AreEqual(2ULL, statistics.DiscardedCount());
    Assert::AreEqual(1000ULL, statistics.MeanMicroseconds());
    Assert::AreEqual(1000ULL, statistics.MinimumMicroseconds());
  }

  TEST_METHOD(TheFirstSampleIsBothTheMinimumAndTheMaximum)
  {
    // The minimum starts at zero and has to be seeded by the first real sample rather than
    // compared against, which is the off-by-one this class would otherwise have.
    Neuron::FrameStatistics statistics;
    statistics.Add(16000);
    Assert::AreEqual(16000ULL, statistics.MinimumMicroseconds());
    Assert::AreEqual(16000ULL, statistics.MaximumMicroseconds());
  }

  TEST_METHOD(ResetClearsEverything)
  {
    Neuron::FrameStatistics statistics;
    statistics.Add(5000);
    statistics.Add(0);
    statistics.Reset();

    Assert::AreEqual(0ULL, statistics.Count());
    Assert::AreEqual(0ULL, statistics.DiscardedCount());
    Assert::AreEqual(0ULL, statistics.MaximumMicroseconds());
  }

  TEST_METHOD(ALongRunDoesNotOverflowOrDrift)
  {
    // Sixty seconds at sixty hertz, which is the length of run the gate actually takes. The
    // running total is the thing that could overflow and this is the order of magnitude it has to
    // survive; the mean must still come back exactly.
    Neuron::FrameStatistics statistics;
    for (std::uint32_t frame = 0; frame < 3600; ++frame)
    {
      statistics.Add(16667);
    }

    Assert::AreEqual(3600ULL, statistics.Count());
    Assert::AreEqual(16667ULL, statistics.MeanMicroseconds());
  }
};

} // namespace NeuronClientTests
