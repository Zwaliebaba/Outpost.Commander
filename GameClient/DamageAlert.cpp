#include "pch.h"

#include "DamageAlert.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Outpost
{

namespace
{
constexpr float AUTHORED_WIDTH = static_cast<float>(Neuron::INTERFACE_AUTHORED_WIDTH);
constexpr float AUTHORED_HEIGHT = static_cast<float>(Neuron::INTERFACE_AUTHORED_HEIGHT);

/// The length of an edge, along it.
[[nodiscard]] std::int32_t EdgeLength(AlertEdge _edge) noexcept
{
  return ((_edge == AlertEdge::Left) || (_edge == AlertEdge::Right)) ? Neuron::INTERFACE_AUTHORED_HEIGHT : Neuron::INTERFACE_AUTHORED_WIDTH;
}

/// The strip an edge's indicators occupy, across the edge: [start, end) on the other axis.
void CrossRange(AlertEdge _edge, std::int32_t& _outStart, std::int32_t& _outEnd) noexcept
{
  switch (_edge)
  {
  case AlertEdge::Left:
  case AlertEdge::Top:
    _outStart = 0;
    _outEnd = ALERT_DEPTH_PIXELS;
    break;
  case AlertEdge::Right:
    _outStart = Neuron::INTERFACE_AUTHORED_WIDTH - ALERT_DEPTH_PIXELS;
    _outEnd = Neuron::INTERFACE_AUTHORED_WIDTH;
    break;
  case AlertEdge::Bottom:
    _outStart = Neuron::INTERFACE_AUTHORED_HEIGHT - ALERT_DEPTH_PIXELS;
    _outEnd = Neuron::INTERFACE_AUTHORED_HEIGHT;
    break;
  }
}

[[nodiscard]] bool IsSide(AlertEdge _edge) noexcept
{
  return (_edge == AlertEdge::Left) || (_edge == AlertEdge::Right);
}

/// Whether a body starting at _along on _edge keeps its clear space from _panel.
[[nodiscard]] bool ClearOf(AlertEdge _edge, std::int32_t _along, const HudRect& _panel) noexcept
{
  std::int32_t crossStart = 0;
  std::int32_t crossEnd = 0;
  CrossRange(_edge, crossStart, crossEnd);
  const std::int32_t panelCrossStart = IsSide(_edge) ? _panel.x : _panel.y;
  const std::int32_t panelCrossEnd = panelCrossStart + (IsSide(_edge) ? _panel.w : _panel.h);
  if ((panelCrossEnd + ALERT_PANEL_CLEAR_PIXELS <= crossStart) || (panelCrossStart - ALERT_PANEL_CLEAR_PIXELS >= crossEnd))
  {
    return true;
  }
  const std::int32_t panelAlongStart = IsSide(_edge) ? _panel.y : _panel.x;
  const std::int32_t panelAlongEnd = panelAlongStart + (IsSide(_edge) ? _panel.h : _panel.w);
  return ((_along + ALERT_ALONG_PIXELS + ALERT_PANEL_CLEAR_PIXELS) <= panelAlongStart) ||
         (_along >= (panelAlongEnd + ALERT_PANEL_CLEAR_PIXELS));
}

/// **THE CLAMP** (rule 3): the allowed start nearest _desired, or false when the edge has no room at all.
[[nodiscard]] bool Clamp(AlertEdge _edge, std::int32_t _desired, std::span<const HudRect> _panels, std::int32_t& _outAlong) noexcept
{
  const std::int32_t lowest = ALERT_PANEL_CLEAR_PIXELS;
  const std::int32_t highest = EdgeLength(_edge) - ALERT_ALONG_PIXELS - ALERT_PANEL_CLEAR_PIXELS;
  const auto allowed = [&](std::int32_t _along) noexcept
  {
    if ((_along < lowest) || (_along > highest))
    {
      return false;
    }
    return std::all_of(_panels.begin(), _panels.end(), [&](const HudRect& _panel) noexcept { return ClearOf(_edge, _along, _panel); });
  };

  // THE CANDIDATES: where it wants to be, the two ends, and both sides of every panel. The nearest allowed one wins.
  std::vector<std::int32_t> candidates{std::clamp(_desired, lowest, highest), lowest, highest};
  for (const HudRect& panel : _panels)
  {
    const std::int32_t start = IsSide(_edge) ? panel.y : panel.x;
    const std::int32_t end = start + (IsSide(_edge) ? panel.h : panel.w);
    candidates.push_back(start - ALERT_PANEL_CLEAR_PIXELS - ALERT_ALONG_PIXELS);
    candidates.push_back(end + ALERT_PANEL_CLEAR_PIXELS);
  }
  bool found = false;
  for (const std::int32_t candidate : candidates)
  {
    if (allowed(candidate) && (!found || (std::abs(candidate - _desired) < std::abs(_outAlong - _desired))))
    {
      _outAlong = candidate;
      found = true;
    }
  }
  return found;
}

[[nodiscard]] HudRect BodyAt(AlertEdge _edge, std::int32_t _along) noexcept
{
  switch (_edge)
  {
  case AlertEdge::Left:
    return HudRect{0, _along, ALERT_DEPTH_PIXELS, ALERT_ALONG_PIXELS};
  case AlertEdge::Right:
    return HudRect{Neuron::INTERFACE_AUTHORED_WIDTH - ALERT_DEPTH_PIXELS, _along, ALERT_DEPTH_PIXELS, ALERT_ALONG_PIXELS};
  case AlertEdge::Top:
    return HudRect{_along, 0, ALERT_ALONG_PIXELS, ALERT_DEPTH_PIXELS};
  case AlertEdge::Bottom:
    return HudRect{_along, Neuron::INTERFACE_AUTHORED_HEIGHT - ALERT_DEPTH_PIXELS, ALERT_ALONG_PIXELS, ALERT_DEPTH_PIXELS};
  }
  return HudRect{};
}

[[nodiscard]] std::int32_t AlongOf(const AlertPlacement& _placement) noexcept
{
  return IsSide(_placement.edge) ? _placement.body.y : _placement.body.x;
}
} // namespace

void DamageAlerts::Note(WireIdentity _identity, float _worldX, float _worldY, std::uint64_t _nowMilliseconds)
{
  for (AlertCluster& cluster : m_clusters)
  {
    const float dx = cluster.worldX - _worldX;
    const float dy = cluster.worldY - _worldY;
    if (((dx * dx) + (dy * dy)) <= (ALERT_CLUSTER_UNITS * ALERT_CLUSTER_UNITS))
    {
      if (std::find(cluster.hit.begin(), cluster.hit.end(), _identity) == cluster.hit.end())
      {
        cluster.hit.push_back(_identity);
      }
      cluster.lastMilliseconds = _nowMilliseconds;
      return;
    }
  }

  // A FOURTH REPLACES THE OLDEST (rule 6).
  if (m_clusters.size() >= MAX_ALERTS)
  {
    const auto oldest = std::min_element(m_clusters.begin(), m_clusters.end(), [](const AlertCluster& _a, const AlertCluster& _b)
                                         { return _a.firstMilliseconds < _b.firstMilliseconds; });
    m_clusters.erase(oldest);
  }
  m_clusters.push_back(AlertCluster{
    .worldX = _worldX, .worldY = _worldY, .firstMilliseconds = _nowMilliseconds, .lastMilliseconds = _nowMilliseconds, .hit = {_identity}});
}

void DamageAlerts::Expire(std::uint64_t _nowMilliseconds) noexcept
{
  std::erase_if(m_clusters, [&](const AlertCluster& _cluster)
                { return _nowMilliseconds >= (_cluster.lastMilliseconds + ALERT_LIFETIME_MILLISECONDS); });
}

void DamageAlerts::Clear() noexcept
{
  m_clusters.clear();
}

std::vector<AlertPlacement> PlaceAlerts(const DamageAlerts& _alerts, const CameraPose& _camera, float _aspectRatio,
                                        std::span<const HudRect> _panels, std::uint64_t _nowMilliseconds)
{
  std::vector<AlertPlacement> placed;
  for (const AlertCluster& cluster : _alerts.Clusters())
  {
    // RULE 1: ON SCREEN ALREADY, SO NOTHING.
    float screenX = 0.0f;
    float screenY = 0.0f;
    if (PlaneToScreen(_camera, _aspectRatio, cluster.worldX, cluster.worldY, screenX, screenY) && (std::abs(screenX) <= 1.0f) &&
        (std::abs(screenY) <= 1.0f))
    {
      continue;
    }

    // RULE 2: THE BEARING ON THE PLANE, TURNED INTO THE CAMERA'S FRAME. Forward across the plane is up on the screen,
    // and right is forward turned a quarter clockwise -- so a cluster behind the camera points down, not back up.
    const float dx = cluster.worldX - _camera.focusX;
    const float dy = cluster.worldY - _camera.focusY;
    const float forwardX = std::cos(_camera.headingRadians);
    const float forwardY = std::sin(_camera.headingRadians);
    const float right = (dx * forwardY) - (dy * forwardX);
    const float up = (dx * forwardX) + (dy * forwardY);
    if ((right == 0.0f) && (up == 0.0f))
    {
      continue;
    }

    // Where the ray from the frame's center, in authored pixels (y down), leaves the frame.
    const float directionX = right;
    const float directionY = -up;
    const float halfWidth = AUTHORED_WIDTH * 0.5f;
    const float halfHeight = AUTHORED_HEIGHT * 0.5f;
    const float reachX = (directionX != 0.0f) ? (halfWidth / std::abs(directionX)) : std::numeric_limits<float>::max();
    const float reachY = (directionY != 0.0f) ? (halfHeight / std::abs(directionY)) : std::numeric_limits<float>::max();

    AlertEdge edge = AlertEdge::Left;
    float center = 0.0f;
    if (reachX < reachY)
    {
      edge = (directionX < 0.0f) ? AlertEdge::Left : AlertEdge::Right;
      center = halfHeight + (directionY * reachX);
    }
    else
    {
      edge = (directionY < 0.0f) ? AlertEdge::Top : AlertEdge::Bottom;
      center = halfWidth + (directionX * reachY);
    }

    std::int32_t along = 0;
    if (!Clamp(edge, static_cast<std::int32_t>(std::lround(center)) - (ALERT_ALONG_PIXELS / 2), _panels, along))
    {
      continue;
    }

    const float age = static_cast<float>(_nowMilliseconds - std::min(_nowMilliseconds, cluster.lastMilliseconds));
    placed.push_back(AlertPlacement{.edge = edge,
                                    .body = BodyAt(edge, along),
                                    .count = static_cast<std::uint32_t>(cluster.hit.size()),
                                    .alpha = std::clamp(1.0f - (age / static_cast<float>(ALERT_LIFETIME_MILLISECONDS)), 0.0f, 1.0f),
                                    .worldX = cluster.worldX,
                                    .worldY = cluster.worldY,
                                    .lastMilliseconds = cluster.lastMilliseconds});
  }

  // RULE 4: TWO THAT WOULD COLLIDE MERGE, at the newer one's place, with their counts summed. Newest first, so the
  // newer of any pair is the one kept.
  std::sort(placed.begin(), placed.end(),
            [](const AlertPlacement& _a, const AlertPlacement& _b) { return _a.lastMilliseconds > _b.lastMilliseconds; });
  std::vector<AlertPlacement> merged;
  for (const AlertPlacement& candidate : placed)
  {
    bool absorbed = false;
    for (AlertPlacement& kept : merged)
    {
      if ((kept.edge == candidate.edge) && (std::abs(AlongOf(kept) - AlongOf(candidate)) < ALERT_SEPARATION_PIXELS))
      {
        kept.count += candidate.count;
        absorbed = true;
        break;
      }
    }
    if (!absorbed)
    {
      merged.push_back(candidate);
    }
  }

  // RULE 6: AT MOST THREE, the newest.
  if (merged.size() > MAX_ALERTS)
  {
    merged.resize(MAX_ALERTS);
  }
  return merged;
}

} // namespace Outpost
