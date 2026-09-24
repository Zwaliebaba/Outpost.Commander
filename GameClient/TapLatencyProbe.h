#pragma once

#include <cstdint>
#include <optional>

namespace Outpost
{

/// Where the watched ship is drawn this frame: its interpolated position in world units, and the heading
/// the mesh is rotated by, which is the record's wire byte (`TechnicalDesign.md` section 4).
///
/// R8: a public aggregate.
struct DrawnPose
{
  float xUnits;
  float yUnits;
  std::uint8_t wireHeading;
};

/// What the first visible response to a tap was.
enum class VisibleChange : std::uint8_t
{
  Heading,
  Position
};

/// One measured tap.
///
/// R8: a public aggregate.
struct TapVisible
{
  std::uint64_t milliseconds;
  VisibleChange change;
};

/// **M0.23'S TAP-TO-VISIBLE INSTRUMENT**: the time from a tap that orders a ship to the first frame in which
/// that ship is drawn differently. `TechnicalDesign.md` section 9.2 defines the onset as the drawn
/// **heading** changing, and until M1.17 the ship had no heading to change, so the instrument watched only
/// its position. **Since M1.17 a ship turns before it flies** (`OpenQuestions.md` Q59), so a position-only
/// instrument timed the whole turn and logged 414 to 665 ms where the response began in under 100. This
/// fires on whichever changes first.
///
/// **IT ARMS ONLY ON A SHIP AT REST**, which now means neither moving nor turning. A tap on a ship still
/// answering an earlier order would time the old motion continuing, not the new order arriving, so that
/// tap is refused and reported as unmeasurable. A missing sample costs nothing and a wrong one reaches an
/// average.
///
/// **A CLOCK IN AND A RESULT OUT, AND NOTHING ELSE** (R20). It held its state in `App.cpp` until this
/// class, which is why the position-only rule could outlive the reason for it without a suite noticing.
class TapLatencyProbe
{
public:
  /// A quarter of a world unit is the wire's own position step (ADR-003), so anything at or below it is
  /// the quantizer rather than movement. Half a unit is above that and far below anything a player would
  /// call a move.
  static constexpr float MOVED_THRESHOLD_UNITS = 0.5f;

  /// Below this between two frames, a ship is not moving. At 100 units a second and 60 frames, a Miner
  /// covers 1.7 units a frame, so the two are far apart.
  static constexpr float AT_REST_THRESHOLD_UNITS = 0.1f;

  /// Every frame the watched ship is drawn. Returns the measurement on the frame it becomes visible, once,
  /// and disarms.
  [[nodiscard]] std::optional<TapVisible> Observe(const DrawnPose& _pose, std::uint64_t _nowMilliseconds) noexcept;

  /// A tap that ordered the watched ship. True, and armed, when it was at rest over the last frame; false
  /// when it was moving or turning, and the tap is not measured.
  [[nodiscard]] bool Arm(std::uint64_t _nowMilliseconds) noexcept;

  [[nodiscard]] bool Armed() const noexcept
  {
    return m_armed;
  }

  /// How far the ship moved over the last frame, and whether its heading changed, for the log's reason
  /// when a tap is refused.
  [[nodiscard]] float MovedLastFrameUnits() const noexcept;
  [[nodiscard]] bool TurnedLastFrame() const noexcept;

  [[nodiscard]] const DrawnPose& Current() const noexcept
  {
    return m_current;
  }

  /// Where the ship was drawn when the probe armed.
  [[nodiscard]] const DrawnPose& From() const noexcept
  {
    return m_from;
  }

private:
  DrawnPose m_current{};
  DrawnPose m_previous{};
  DrawnPose m_from{};
  std::uint64_t m_tapAtMilliseconds = 0;
  bool m_seen = false;
  bool m_armed = false;
};

} // namespace Outpost
