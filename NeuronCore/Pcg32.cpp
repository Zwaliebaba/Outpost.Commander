// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Pcg32.h"

namespace Neuron
{

Pcg32::Pcg32(std::uint64_t _seed, std::uint64_t _stream) noexcept
{
  // PCG's own seeding routine, and the order matters: the increment is established first, the
  // state is stepped once from zero, the seed is added, and the state is stepped again. Adding
  // the seed to a zero state and calling that ready would leave two nearby seeds producing two
  // nearby first outputs, which is exactly what the second step exists to destroy.
  m_state = 0;
  m_increment = (_stream << 1u) | 1u;
  static_cast<void>(Next());
  m_state += _seed;
  static_cast<void>(Next());
}

std::uint32_t Pcg32::Next() noexcept
{
  const std::uint64_t previous = m_state;

  // Unsigned, so the wrap at 2^64 is defined rather than merely usual. This is the whole LCG.
  m_state = (previous * MULTIPLIER) + m_increment;

  // XSH RR: xorshift the high bits down, then rotate by an amount taken from the top five bits.
  // The rotation is what removes the LCG's short-period low bits from the output -- a plain
  // truncation would hand a caller the worst part of the state.
  const std::uint32_t xorshifted = static_cast<std::uint32_t>(((previous >> 18u) ^ previous) >> 27u);
  const std::uint32_t rotation = static_cast<std::uint32_t>(previous >> 59u);

  // The `& 31` is not decoration: a shift by 32 is undefined, and `rotation` is zero one time in
  // thirty-two. Spelled this way it is a rotate for every rotation value including that one.
  return (xorshifted >> rotation) | (xorshifted << ((0u - rotation) & 31u));
}

std::uint32_t Pcg32::NextBelow(std::uint32_t _bound) noexcept
{
  if (_bound == 0)
  {
    return 0;
  }

  // 2^32 modulo the bound, computed without a 64-bit intermediate: the count of values at the
  // bottom of the range that would make the modulo uneven. Draw again while inside it.
  const std::uint32_t threshold = (0u - _bound) % _bound;

  for (;;)
  {
    const std::uint32_t draw = Next();
    if (draw >= threshold)
    {
      return draw % _bound;
    }
  }
}

std::int32_t Pcg32::NextInRange(std::int32_t _lowest, std::int32_t _highest) noexcept
{
  // Unsigned throughout, because the span of the full int32 range is 2^32 and every signed
  // intermediate on the way to it overflows.
  const std::uint32_t lowest = static_cast<std::uint32_t>(_lowest);
  const std::uint32_t span = (static_cast<std::uint32_t>(_highest) - lowest) + 1u;

  // A span of zero here means the whole 32-bit range, which wrapped rather than being empty --
  // the one case NextBelow cannot express, and the one where every draw is already in range.
  const std::uint32_t offset = (span == 0) ? Next() : NextBelow(span);
  return static_cast<std::int32_t>(lowest + offset);
}

} // namespace Neuron
