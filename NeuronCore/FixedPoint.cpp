// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "FixedPoint.h"

namespace Neuron
{

std::int64_t Sqrt(std::int64_t _value) noexcept
{
  if (_value <= 0)
  {
    return 0;
  }

  // Unsigned throughout: the intermediates below go above 2^62 for a large input, and a signed
  // overflow there would be undefined where an unsigned one is merely wrong.
  std::uint64_t remainder = static_cast<std::uint64_t>(_value);
  std::uint64_t result = 0;

  // The highest power of four at or below the input. Starting anywhere higher costs iterations
  // that can only shift zeros; starting lower gets the wrong answer.
  std::uint64_t bit = std::uint64_t{1} << 62;
  while (bit > remainder)
  {
    bit >>= 2;
  }

  // Digit by digit in base four, which is long division against the identity
  // (2r + b)^2 = 4r^2 + 4rb + b^2. Exact on a perfect square and floored between two, with no
  // division, no table and nothing platform-dependent anywhere in it.
  while (bit != 0)
  {
    if (remainder >= result + bit)
    {
      remainder -= result + bit;
      result = (result >> 1) + bit;
    }
    else
    {
      result >>= 1;
    }
    bit >>= 2;
  }

  return static_cast<std::int64_t>(result);
}

} // namespace Neuron
