#pragma once

#include "Camera.h"
#include "HudLayout.h"

#include "GameCore.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// **HOW LONG AN ALERT STAYS: FOUR SECONDS** from the last hit in its cluster, fading over them (ADR-020). Long enough
/// to look up and see it; short enough that a raid that stopped stops shouting. Not tuned.
inline constexpr std::uint64_t ALERT_LIFETIME_MILLISECONDS = 4000;

/// **HITS THIS CLOSE ARE ONE ALERT** (the handoff's *Damage alert*, rule 5): a fleet caught in the open is one indicator
/// with a count rather than forty. Half the home field's width. Not tuned.
inline constexpr float ALERT_CLUSTER_UNITS = 1500.0f;

/// The handoff's cap: at most three at once, and a fourth replaces the oldest.
inline constexpr std::size_t MAX_ALERTS = 3;

/// The handoff's body, for a left-edge indicator: 96 along the edge and 120 into the frame.
inline constexpr std::int32_t ALERT_ALONG_PIXELS = 96;
inline constexpr std::int32_t ALERT_DEPTH_PIXELS = 120;

/// Never closer than this along an edge: the body and 16 of clear space. Two that would be closer merge.
inline constexpr std::int32_t ALERT_SEPARATION_PIXELS = 112;

/// How far an alert's body stays from every panel band.
inline constexpr std::int32_t ALERT_PANEL_CLEAR_PIXELS = 16;

/// **ONE PLACE YOU ARE BEING HURT** (M3.3b): the identities hit there and when. R8: a public aggregate.
struct AlertCluster
{
  float worldX = 0.0f;
  float worldY = 0.0f;
  std::uint64_t firstMilliseconds = 0;
  std::uint64_t lastMilliseconds = 0;

  /// Every one of yours hit or killed here, once each: the count is how many, not how many shots.
  std::vector<WireIdentity> hit;
};

/// **THE ATTACKS ON YOU** (M3.3b, ADR-020): a fire event naming one of yours, or a removal of one of yours, folded into
/// a cluster by place and time. **Client-side and from data the client already holds**: no wire bytes, no host change.
class DamageAlerts
{
public:
  /// One of yours, _identity at the world point, was hit or died at _nowMilliseconds.
  void Note(WireIdentity _identity, float _worldX, float _worldY, std::uint64_t _nowMilliseconds);

  /// Drops every cluster whose last hit is `ALERT_LIFETIME_MILLISECONDS` old.
  void Expire(std::uint64_t _nowMilliseconds) noexcept;

  void Clear() noexcept;

  [[nodiscard]] std::span<const AlertCluster> Clusters() const noexcept
  {
    return m_clusters;
  }

private:
  std::vector<AlertCluster> m_clusters;
};

enum class AlertEdge : std::uint8_t
{
  Left,
  Right,
  Top,
  Bottom
};

/// One indicator as the interface draws it, in authored pixels. R8: a public aggregate.
struct AlertPlacement
{
  AlertEdge edge = AlertEdge::Left;

  /// The whole body, which is also the hit rectangle: 120 x 96 on a side edge, 96 x 120 on the top or bottom.
  HudRect body{};

  std::uint32_t count = 0;

  /// One at the last hit, to nothing at the end of its lifetime.
  float alpha = 1.0f;

  /// Where a tap on it recenters the camera.
  float worldX = 0.0f;
  float worldY = 0.0f;

  std::uint64_t lastMilliseconds = 0;
};

/// **WHERE THE ALERTS GO THIS FRAME** (the handoff's placement rules):
///
/// 1. **Nothing for a cluster already on screen.**
/// 2. The edge is where the ray from the frame's center, along the cluster's bearing, leaves the frame. **The bearing
///    comes from the plane, not the projection**, so a cluster behind the camera still points the right way.
/// 3. The body is centered where that ray crosses, then clamped to stay on the edge and at least 16 clear of every
///    one of _panels.
/// 4. Two on one edge closer than `ALERT_SEPARATION_PIXELS` merge, summing their counts, at the newer one's place.
/// 5. At most `MAX_ALERTS`, the newest kept.
[[nodiscard]] std::vector<AlertPlacement> PlaceAlerts(const DamageAlerts& _alerts, const CameraPose& _camera, float _aspectRatio,
                                                      std::span<const HudRect> _panels, std::uint64_t _nowMilliseconds);

} // namespace Outpost
