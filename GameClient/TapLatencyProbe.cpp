#include "pch.h"

#include "TapLatencyProbe.h"

#include <cmath>

namespace Outpost
{

std::optional<TapVisible> TapLatencyProbe::Observe(const DrawnPose& _pose, std::uint64_t _nowMilliseconds) noexcept
{
  // The first frame is its own previous, so a ship seen once is at rest rather than having arrived from
  // the origin.
  m_previous = m_seen ? m_current : _pose;
  m_current = _pose;
  m_seen = true;

  if (!m_armed)
  {
    return std::nullopt;
  }

  // HEADING FIRST, because a ship that turns before it flies shows the turn first, and on a frame where
  // both have changed the turn is what began the response.
  const bool turned = m_current.wireHeading != m_from.wireHeading;
  const bool moved = (std::fabs(m_current.xUnits - m_from.xUnits) > MOVED_THRESHOLD_UNITS) ||
                     (std::fabs(m_current.yUnits - m_from.yUnits) > MOVED_THRESHOLD_UNITS);
  if (!turned && !moved)
  {
    return std::nullopt;
  }

  m_armed = false;
  const std::uint64_t elapsed = (_nowMilliseconds > m_tapAtMilliseconds) ? (_nowMilliseconds - m_tapAtMilliseconds) : 0;
  return TapVisible{.milliseconds = elapsed, .change = turned ? VisibleChange::Heading : VisibleChange::Position};
}

bool TapLatencyProbe::Arm(std::uint64_t _nowMilliseconds) noexcept
{
  if ((MovedLastFrameUnits() >= AT_REST_THRESHOLD_UNITS) || TurnedLastFrame())
  {
    m_armed = false;
    return false;
  }

  m_armed = true;
  m_tapAtMilliseconds = _nowMilliseconds;
  m_from = m_current;
  return true;
}

float TapLatencyProbe::MovedLastFrameUnits() const noexcept
{
  return std::fabs(m_current.xUnits - m_previous.xUnits) + std::fabs(m_current.yUnits - m_previous.yUnits);
}

bool TapLatencyProbe::TurnedLastFrame() const noexcept
{
  return m_current.wireHeading != m_previous.wireHeading;
}

} // namespace Outpost
