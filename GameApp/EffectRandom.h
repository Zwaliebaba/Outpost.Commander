#pragma once

namespace Outpost
{
// Randomness for how an effect looks (ADR-026): SplitMix64, written out so that the looks do not hang on a standard
// library's distributions. It is presentation, not the simulation's Random (ADR-009), but it is seeded, so that a test
// sees the same draws every run and one destruction always looks the same.
class EffectRandom
{
public:
  explicit EffectRandom(std::uint64_t _seed) noexcept
    : m_state(_seed)
  {
  }

  [[nodiscard]] std::uint64_t Next() noexcept
  {
    m_state += 0x9E37'79B9'7F4A'7C15ULL;
    std::uint64_t mixed = m_state;
    mixed = (mixed ^ (mixed >> 30)) * 0xBF58'476D'1CE4'E5B9ULL;
    mixed = (mixed ^ (mixed >> 27)) * 0x94D0'49BB'1331'11EBULL;
    return mixed ^ (mixed >> 31);
  }

  // A number in [0, 1), from the top 24 bits of one draw.
  [[nodiscard]] float Unit() noexcept
  {
    return static_cast<float>(Next() >> 40) * 0x1.0p-24f;
  }

  // A number in [-_extent, _extent).
  [[nodiscard]] float Signed(float _extent) noexcept
  {
    return ((2.0f * Unit()) - 1.0f) * _extent;
  }

  // A number in [_low, _high).
  [[nodiscard]] float Between(float _low, float _high) noexcept
  {
    return _low + ((_high - _low) * Unit());
  }

private:
  std::uint64_t m_state;
};
} // namespace Outpost
