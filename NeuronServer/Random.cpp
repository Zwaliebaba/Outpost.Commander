#include "pch.h"
#include "Random.h"

#include <bit>

namespace
{
std::uint64_t SplitMix64(std::uint64_t& _state) noexcept
{
  std::uint64_t z = (_state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
} // namespace

Neuron::Random::Random(std::uint64_t _seed) noexcept
{
  // SplitMix64 never yields four zeros in a row, so the state is never the all-zero one xoshiro cannot leave.
  for (std::uint64_t& word : m_state)
    word = SplitMix64(_seed);
}

std::uint64_t Neuron::Random::NextUInt64() noexcept
{
  const std::uint64_t result = std::rotl(m_state[1] * 5, 7) * 9;
  const std::uint64_t shifted = m_state[1] << 17;
  m_state[2] ^= m_state[0];
  m_state[3] ^= m_state[1];
  m_state[1] ^= m_state[2];
  m_state[0] ^= m_state[3];
  m_state[2] ^= shifted;
  m_state[3] = std::rotl(m_state[3], 45);
  return result;
}

std::uint32_t Neuron::Random::NextBelow(std::uint32_t _bound) noexcept
{
  // Lemire's method: the high half of a 32-by-32-bit product is uniform in [0, _bound) once the few low halves that
  // would favor some results are drawn again.
  std::uint64_t product = (NextUInt64() >> 32) * _bound;
  auto low = static_cast<std::uint32_t>(product);
  if (low < _bound)
  {
    const std::uint32_t threshold = (0u - _bound) % _bound;
    while (low < threshold)
    {
      product = (NextUInt64() >> 32) * _bound;
      low = static_cast<std::uint32_t>(product);
    }
  }
  return static_cast<std::uint32_t>(product >> 32);
}

double Neuron::Random::NextUnit() noexcept
{
  return static_cast<double>(NextUInt64() >> 11) * 0x1.0p-53;
}
