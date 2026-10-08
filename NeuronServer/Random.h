#pragma once

namespace Neuron
{
// The simulation's one source of randomness (ADR-002 decision 8, ADR-009): xoshiro256** 1.0 by Blackman and Vigna,
// seeded through SplitMix64. The algorithm is written here rather than taken from <random>, whose distributions differ
// between standard libraries, so the same seed gives the same draws on every build. Changing it breaks every recorded
// replay, and RandomTests pins its output.
class Random
{
public:
  explicit Random(std::uint64_t _seed) noexcept;

  [[nodiscard]] std::uint64_t NextUInt64() noexcept;

  // A number from 0 to _bound - 1, each equally likely. _bound must be at least 1.
  [[nodiscard]] std::uint32_t NextBelow(std::uint32_t _bound) noexcept;

  // A number in [0, 1), from the top 53 bits of one draw.
  [[nodiscard]] double NextUnit() noexcept;

  // The generator's state, for a world's save, and the state a save held (ADR-077): with it the generator draws on as it
  // would have.
  [[nodiscard]] const std::array<std::uint64_t, 4>& State() const noexcept
  {
    return m_state;
  }
  void SetState(const std::array<std::uint64_t, 4>& _state) noexcept
  {
    m_state = _state;
  }

  // Two generators are equal when their next draws are, which is what a replay compares.
  friend bool operator==(const Random&, const Random&) = default;

private:
  std::array<std::uint64_t, 4> m_state{};
};
} // namespace Neuron