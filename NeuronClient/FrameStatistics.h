#pragma once

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>

namespace Neuron
{

/// What a run of frames cost, accumulated one sample at a time.
///
/// IT IS HERE RATHER THAN IN THE SHELL BECAUSE IT IS ARITHMETIC (R20). A mean is a thing a
/// packaged application can get wrong invisibly, and the figures it produces are written into
/// `ADR-016` and `TechnicalDesign.md` section 9.5 as measurements -- so the one part of the
/// measurement that is not an observation has a suite over it.
///
/// **It holds no samples.** The maximum and the mean come from running totals, which is why this
/// costs eight words regardless of how long a run is; what it gives up is any percentile, and a
/// percentile over a vsync-bound run mostly measures the vertical blank anyway.
class FrameStatistics
{
public:
  /// Samples below this are discarded as a measurement artifact rather than a fast frame. A GPU
  /// timestamp pair that straddles nothing reads as zero, and averaging those in silently drags
  /// the mean toward a figure no frame ever achieved.
  static constexpr std::uint64_t MINIMUM_SAMPLE_MICROSECONDS = 1;

  constexpr void Add(std::uint64_t _microseconds) noexcept
  {
    if (_microseconds < MINIMUM_SAMPLE_MICROSECONDS)
    {
      ++m_discardedCount;
      return;
    }

    m_totalMicroseconds += _microseconds;
    ++m_count;

    if ((m_count == 1) || (_microseconds < m_minimumMicroseconds))
    {
      m_minimumMicroseconds = _microseconds;
    }
    if (_microseconds > m_maximumMicroseconds)
    {
      m_maximumMicroseconds = _microseconds;
    }
  }

  constexpr void Reset() noexcept
  {
    *this = FrameStatistics{};
  }

  [[nodiscard]] constexpr std::uint64_t Count() const noexcept
  {
    return m_count;
  }

  [[nodiscard]] constexpr std::uint64_t DiscardedCount() const noexcept
  {
    return m_discardedCount;
  }

  [[nodiscard]] constexpr std::uint64_t MinimumMicroseconds() const noexcept
  {
    return m_minimumMicroseconds;
  }

  [[nodiscard]] constexpr std::uint64_t MaximumMicroseconds() const noexcept
  {
    return m_maximumMicroseconds;
  }

  /// Rounded to nearest rather than truncated, because a frame time is quoted to the microsecond
  /// and truncation biases every figure in this design's measurements the same way.
  [[nodiscard]] constexpr std::uint64_t MeanMicroseconds() const noexcept
  {
    return (m_count == 0) ? 0 : ((m_totalMicroseconds + (m_count / 2)) / m_count);
  }

private:
  std::uint64_t m_totalMicroseconds = 0;
  std::uint64_t m_count = 0;
  std::uint64_t m_discardedCount = 0;
  std::uint64_t m_minimumMicroseconds = 0;
  std::uint64_t m_maximumMicroseconds = 0;
};

/// One frame's GPU time in the three spans `TechnicalDesign.md` section 9.6 compares. The world is
/// everything drawn into the scene target, the present is the scaled blit into the back buffer, and
/// the interface is everything after it, text included (ADR-011's order).
struct GpuFrameSplit
{
  std::uint64_t worldMicroseconds;
  std::uint64_t presentMicroseconds;
  std::uint64_t interfaceMicroseconds;
};

/// The four timestamps a frame writes, in the order it writes them: begun, world drawn, present
/// scaled, ended.
inline constexpr std::size_t GPU_FRAME_MARKS = 4;

/// Converts one frame's four timestamps into its three spans. **False, and the output untouched,
/// when there is no honest figure**: no frequency, or marks out of order, which is what a frame that
/// skipped a mark leaves behind -- its slot still holds an older frame's value.
///
/// Each span is truncated to the microsecond on its own, as `GraphicsDevice::LastFrameGpuMicroseconds`
/// truncates the whole, so the three can sum to up to two microseconds less than it.
[[nodiscard]] constexpr bool SplitGpuFrame(const std::uint64_t (&_ticks)[GPU_FRAME_MARKS], std::uint64_t _ticksPerSecond,
                                           GpuFrameSplit& _outSplit) noexcept
{
  if (_ticksPerSecond == 0)
  {
    return false;
  }
  for (std::size_t index = 1; index < GPU_FRAME_MARKS; ++index)
  {
    if (_ticks[index] < _ticks[index - 1])
    {
      return false;
    }
  }

  constexpr std::uint64_t MICROSECONDS_PER_SECOND = 1000000;
  const auto span = [&](std::size_t _from) { return ((_ticks[_from + 1] - _ticks[_from]) * MICROSECONDS_PER_SECOND) / _ticksPerSecond; };
  _outSplit = GpuFrameSplit{.worldMicroseconds = span(0), .presentMicroseconds = span(1), .interfaceMicroseconds = span(2)};
  return true;
}

} // namespace Neuron
