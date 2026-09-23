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

TEST_CLASS(TheGpuFrameSplit)
{
public:
  TEST_METHOD(ItSplitsTheFrameAtItsTwoMarks)
  {
    // A 10 MHz clock, so a tick is a tenth of a microsecond: 800 us of world, 50 of present and 300
    // of interface.
    const std::uint64_t ticks[Neuron::GPU_FRAME_MARKS]{1000, 9000, 9500, 12500};
    Neuron::GpuFrameSplit split{};

    Assert::IsTrue(Neuron::SplitGpuFrame(ticks, 10000000, split));
    Assert::AreEqual(800ULL, split.worldMicroseconds);
    Assert::AreEqual(50ULL, split.presentMicroseconds);
    Assert::AreEqual(300ULL, split.interfaceMicroseconds);
  }

  TEST_METHOD(AStaleMarkIsRefusedRatherThanReported)
  {
    // A frame that skipped a mark leaves an older frame's value in its slot, which is before this
    // frame began. Reporting it would be a negative span wrapped to an enormous one.
    const std::uint64_t ticks[Neuron::GPU_FRAME_MARKS]{5000, 400, 5600, 7000};
    Neuron::GpuFrameSplit split{.worldMicroseconds = 1, .presentMicroseconds = 2, .interfaceMicroseconds = 3};

    Assert::IsFalse(Neuron::SplitGpuFrame(ticks, 10000000, split));
    Assert::AreEqual(1ULL, split.worldMicroseconds, L"a refused split leaves the output untouched");
  }

  TEST_METHOD(NoFrequencyIsNoFigure)
  {
    const std::uint64_t ticks[Neuron::GPU_FRAME_MARKS]{0, 1, 2, 3};
    Neuron::GpuFrameSplit split{};

    Assert::IsFalse(Neuron::SplitGpuFrame(ticks, 0, split));
  }

  TEST_METHOD(AnEmptySpanIsZeroNotRefused)
  {
    // Two marks written back to back with nothing between them is a real, empty pass.
    const std::uint64_t ticks[Neuron::GPU_FRAME_MARKS]{100, 200, 200, 300};
    Neuron::GpuFrameSplit split{};

    Assert::IsTrue(Neuron::SplitGpuFrame(ticks, 1000000, split));
    Assert::AreEqual(0ULL, split.presentMicroseconds);
  }
};

} // namespace NeuronClientTests
