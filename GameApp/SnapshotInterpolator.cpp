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

  // The server's clock is the wall time less the snapshot's tick. A snapshot that arrives earlier than the clock says sets
  // it back at once, and a later one moves it by a share of how late it is, so that it follows the earliest arrivals and,
  // slowly, a server that falls behind (ADR-013 decision 3).
  const double offset = m_seconds - (static_cast<double>(_snapshot.tick) / m_ticksPerSecond);
  double lateness = 0.0;
  if (!m_offsetSeconds.has_value() || offset <= *m_offsetSeconds)
    m_offsetSeconds = offset;
  else
  {
    lateness = offset - *m_offsetSeconds;
    *m_offsetSeconds += lateness * CLOCK_FOLLOW;
  }
  m_lateness.emplace_back(m_seconds, lateness);
  while (m_lateness.front().first < m_seconds - LATENESS_WINDOW_SECONDS)
    m_lateness.pop_front();
  m_latenessSorted.clear();
  for (const auto& [arrived, late] : m_lateness)
    m_latenessSorted.push_back(late);
  // The nearest rank, as Tools/FrameTimes.py takes a percentile.
  const std::size_t rank = std::max<std::size_t>(1, ((m_latenessSorted.size() * LATENESS_PERCENT) + 99) / 100) - 1;
  std::ranges::nth_element(m_latenessSorted, m_latenessSorted.begin() + static_cast<std::ptrdiff_t>(rank));
  m_delayTicks = std::clamp(LEAST_DELAY_TICKS + (m_latenessSorted[rank] * m_ticksPerSecond), LEAST_DELAY_TICKS, MOST_DELAY_TICKS);

  m_history.push_back(std::move(_snapshot));
  // The oldest goes once the view has passed the one after it, and at the most kept whatever the view needs.
  while (m_history.size() > MOST_HISTORY || (m_history.size() > HISTORY && static_cast<double>(m_history[1].tick) <= m_viewTick))
    m_history.pop_front();
}

void Outpost::SnapshotInterpolator::Advance(float _elapsedSeconds) noexcept
{
  m_seconds += _elapsedSeconds;
  if (m_history.empty() || !m_offsetSeconds.has_value())
    return;

  // Where the view should be: the server's tick now, less the delay.
  const double target = ((m_seconds - *m_offsetSeconds) * m_ticksPerSecond) - m_delayTicks;
  if (!m_clockStarted)
  {
    m_viewTick = target;
    m_clockStarted = true;
  }
  else
  {
    // On at the tick rate, pulled toward the target; never back, and no faster than the catch-up pace.
    const double step = _elapsedSeconds * m_ticksPerSecond;
    const double pulled = m_viewTick + step + ((target - (m_viewTick + step)) * CLOCK_CORRECTION);
    m_viewTick = std::clamp(pulled, m_viewTick, m_viewTick + (step * CATCH_UP_PACE));
  }
  // Never ahead of what has arrived, and never behind what is kept.
  m_viewTick = std::clamp(m_viewTick, static_cast<double>(m_history.front().tick), static_cast<double>(m_history.back().tick));
}

std::vector<Outpost::EntityView> Outpost::SnapshotInterpolator::Entities() const
{
  std::vector<EntityView> entities;
  Entities(entities);
  return entities;
}

void Outpost::SnapshotInterpolator::Entities(std::vector<EntityView>& _into) const
{
  if (m_history.empty())
  {
    _into.clear();
    return;
  }

  // The newer of the two snapshots around the view's tick: the first one past it, or the newest.
  const auto newer = NewerThanView();
  if (newer == m_history.end() || newer == m_history.begin())
  {
    const std::vector<EntityView>& only = (newer == m_history.end() ? m_history.back() : m_history.front()).entities;
    _into.assign(only.begin(), only.end());
    return;
  }
  const Snapshot& to = *newer;
  const Snapshot& from = *std::prev(newer);
  const auto fraction = static_cast<float>((m_viewTick - static_cast<double>(from.tick)) / static_cast<double>(to.tick - from.tick));

  // Both snapshots list their entities in identifier order (Simulation::BuildSnapshot), so one pass pairs them.
  _into.assign(to.entities.begin(), to.entities.end());
  auto previous = from.entities.begin();
  for (EntityView& entity : _into)
  {
    previous =
      std::lower_bound(previous, from.entities.end(), entity.id, [](const EntityView& _view, EntityId _id) { return _view.id < _id; });
    if (previous == from.entities.end() || previous->id != entity.id)
      continue;
    entity.position = {.xMeters = std::lerp(previous->position.xMeters, entity.position.xMeters, fraction),
                       .zMeters = std::lerp(previous->position.zMeters, entity.position.zMeters, fraction)};
    entity.headingRadians = InterpolateHeading(previous->headingRadians, entity.headingRadians, fraction);
  }
}

std::vector<Outpost::EntityMotion> Outpost::SnapshotInterpolator::Motions() const
{
  const auto newer = NewerThanView();
  if (m_history.empty() || newer == m_history.end() || newer == m_history.begin())
    return {};
  const Snapshot& to = *newer;
  const Snapshot& from = *std::prev(newer);
  const auto seconds = static_cast<float>(static_cast<double>(to.tick - from.tick) / m_ticksPerSecond);

  std::vector<EntityMotion> motions;
  motions.reserve(to.entities.size());
  auto previous = from.entities.begin();
  for (const EntityView& entity : to.entities)
  {
    previous =
      std::lower_bound(previous, from.entities.end(), entity.id, [](const EntityView& _view, EntityId _id) { return _view.id < _id; });
    if (previous == from.entities.end() || previous->id != entity.id)
      continue;
    const float meters =
      std::hypot(entity.position.xMeters - previous->position.xMeters, entity.position.zMeters - previous->position.zMeters);
    const float turned = InterpolateHeading(previous->headingRadians, entity.headingRadians, 1.0f) - previous->headingRadians;
    motions.push_back({.id = entity.id, .speedMetersPerSecond = meters / seconds, .turnRadiansPerSecond = turned / seconds});
  }
  return motions;
}

std::deque<Outpost::Snapshot>::const_iterator Outpost::SnapshotInterpolator::NewerThanView() const
{
  return std::ranges::find_if(m_history, [this](const Snapshot& _snapshot) { return static_cast<double>(_snapshot.tick) > m_viewTick; });
}

float Outpost::InterpolateHeading(float _from, float _to, float _fraction) noexcept
{
  constexpr float TURN = 2.0f * std::numbers::pi_v<float>;
  const float difference = std::remainder(_to - _from, TURN);
  return _from + (difference * _fraction);
}