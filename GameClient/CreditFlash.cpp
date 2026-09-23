#include "pch.h"

#include "CreditFlash.h"

#include <cmath>

namespace Outpost
{

void CreditFlash::Observe(std::uint32_t _credits, std::uint64_t _nowMilliseconds) noexcept
{
  if (!m_seen)
  {
    m_seen = true;
    m_credits = _credits;
    return;
  }
  if (_credits == m_credits)
  {
    return;
  }

  // THE NET CHANGE SINCE THE LAST UPDATE, and its sign is the colour. A delivery and a build starting in the
  // same update read as whichever was larger, which is the truth about the balance.
  m_change = (_credits > m_credits) ? CreditChange::Gain : CreditChange::Spend;
  m_changedAtMilliseconds = _nowMilliseconds;
  m_credits = _credits;
}

void CreditFlash::Reset() noexcept
{
  *this = CreditFlash{};
}

CreditChange CreditFlash::Showing(std::uint64_t _nowMilliseconds) const noexcept
{
  if (m_change == CreditChange::None)
  {
    return CreditChange::None;
  }

  // A clock that went backwards is treated as the moment of the change, which is the harmless answer.
  const std::uint64_t elapsed = (_nowMilliseconds >= m_changedAtMilliseconds) ? (_nowMilliseconds - m_changedAtMilliseconds) : 0;
  const std::uint64_t duration = (m_change == CreditChange::Gain) ? GAIN_DURATION_MILLISECONDS : SPEND_DURATION_MILLISECONDS;
  return (elapsed < duration) ? m_change : CreditChange::None;
}

float CreditFlash::Alpha(std::uint64_t _nowMilliseconds) const noexcept
{
  const CreditChange showing = Showing(_nowMilliseconds);
  if (showing == CreditChange::None)
  {
    return 0.0f;
  }

  const std::uint64_t elapsed = (_nowMilliseconds >= m_changedAtMilliseconds) ? (_nowMilliseconds - m_changedAtMilliseconds) : 0;
  const float timeConstant =
    static_cast<float>((showing == CreditChange::Gain) ? GAIN_TIME_CONSTANT_MILLISECONDS : SPEND_TIME_CONSTANT_MILLISECONDS);
  return std::exp(-static_cast<float>(elapsed) / timeConstant);
}

} // namespace Outpost
