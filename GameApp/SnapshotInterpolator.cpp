#include "pch.h"
#include "SnapshotInterpolator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

Outpost::SnapshotInterpolator::SnapshotInterpolator(std::uint32_t _ticksPerSecond)
  : m_ticksPerSecond(static_cast<double>(_ticksPerSecond))
{
}

void Outpost::SnapshotInterpolator::Receive(Snapshot _snapshot)
{
  if (!m_history.empty() && _snapshot.tick <= m_history.back().tick)
    return;
  m_history.push_back(std::move(_snapshot));
  if (m_history.size() > HISTORY)
    m_history.pop_front();
  m_sinceNewestSeconds = 0.0;
}

void Outpost::SnapshotInterpolator::Advance(float _elapsedSeconds) noexcept
{
  if (m_history.empty())
    return;
  m_sinceNewestSeconds += _elapsedSeconds;

  // Where the view should be: the newest tick, plus the time since it arrived, less the delay. It is a saw tooth that
  // jitters by a frame with each arrival, so the clock follows it loosely rather than exactly.
  const auto newestTick = static_cast<double>(m_history.back().tick);
  const double target = newestTick + (m_sinceNewestSeconds * m_ticksPerSecond) - DELAY_TICKS;
  if (!m_clockStarted)
  {
    m_viewTick = target;
    m_clockStarted = true;
  }
  else
  {
    m_viewTick += _elapsedSeconds * m_ticksPerSecond;
    const double error = target - m_viewTick;
    m_viewTick = std::abs(error) > CLOCK_SNAP_TICKS ? target : m_viewTick + (error * CLOCK_CORRECTION);
  }
  // Never ahead of what has arrived, and never behind what is kept.
  m_viewTick = std::clamp(m_viewTick, static_cast<double>(m_history.front().tick), newestTick);
}

std::vector<Outpost::EntityView> Outpost::SnapshotInterpolator::Entities() const
{
  if (m_history.empty())
    return {};

  // The newer of the two snapshots around the view's tick: the first one past it, or the newest.
  const auto newer =
    std::ranges::find_if(m_history, [this](const Snapshot& _snapshot) { return static_cast<double>(_snapshot.tick) > m_viewTick; });
  if (newer == m_history.end() || newer == m_history.begin())
    return (newer == m_history.end() ? m_history.back() : m_history.front()).entities;
  const Snapshot& to = *newer;
  const Snapshot& from = *std::prev(newer);
  const auto fraction = static_cast<float>((m_viewTick - static_cast<double>(from.tick)) / static_cast<double>(to.tick - from.tick));

  // Both snapshots list their entities in identifier order (Simulation::BuildSnapshot), so one pass pairs them.
  std::vector<EntityView> entities = to.entities;
  auto previous = from.entities.begin();
  for (EntityView& entity : entities)
  {
    previous =
      std::lower_bound(previous, from.entities.end(), entity.id, [](const EntityView& _view, EntityId _id) { return _view.id < _id; });
    if (previous == from.entities.end() || previous->id != entity.id)
      continue;
    entity.position = {.xMeters = std::lerp(previous->position.xMeters, entity.position.xMeters, fraction),
                       .zMeters = std::lerp(previous->position.zMeters, entity.position.zMeters, fraction)};
    entity.headingRadians = InterpolateHeading(previous->headingRadians, entity.headingRadians, fraction);
  }
  return entities;
}

float Outpost::InterpolateHeading(float _from, float _to, float _fraction) noexcept
{
  constexpr float TURN = 2.0f * std::numbers::pi_v<float>;
  const float difference = std::remainder(_to - _from, TURN);
  return _from + (difference * _fraction);
}
