#pragma once

#include "HudLayout.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// What a tap on the interface asks for. **Nothing here is an order to the world**: the interface is
/// tested first (`design_handoff_hud` *Pick order*), and a tap that lands on it never reaches the world
/// hit test at all.
enum class HudAction : std::uint8_t
{
  /// Inside a panel but on no target -- the tap is consumed and does nothing. A tap on a credit balance
  /// that fell through to a move order would send the fleet to wherever the balance happens to be drawn.
  None,

  /// Narrows the selection to one design. The argument is the `DesignId`.
  SelectGroup,

  /// Deselects everything, and is **the only way to**: a tap on empty space is already a move order.
  ClearSelection,

  /// Starts building a design. The argument is the `DesignId`.
  Build,

  /// Cancels the item in progress, for the refund `OpenQuestions.md` Q35 settled.
  CancelBuild,

  /// The first of the quit's two taps.
  ArmQuit,

  /// The confirm's safe half.
  StayInMatch,

  /// The second of the quit's two taps, and the only thing in the interface that ends the match.
  ConfirmQuit
};

/// One rectangle the hit test knows about. R8: a public aggregate.
struct HudTarget
{
  HudRect hit{};
  TouchTier tier = TouchTier::Floor;
  HudAction action = HudAction::None;
  std::uint8_t argument = 0;

  /// Which surface it belongs to -- `sel`, `build`, `system` -- because clear space is asserted between
  /// targets that can be on screen together, which is the targets of one surface (`CheckHudGeometry.py`
  /// draws the same line).
  char surface = ' ';
};

/// What a tap hit. R8: a public aggregate.
struct HudHit
{
  /// True when the tap landed anywhere on the interface -- a target or merely a panel. **A consumed tap
  /// never reaches the world.**
  bool consumed = false;

  HudAction action = HudAction::None;
  std::uint8_t argument = 0;
};

/// **THE INTERFACE'S HIT TABLE**: the rectangles a tap can land on, rebuilt with the panels each frame.
///
/// A "button" here is a rectangle this table knows about and nothing more -- there is no Windows Runtime
/// control anywhere in this tree (R18). Targets and blockers are separate lists because they answer
/// different questions: a target has a verb, and a blocker is a panel whose area belongs to the interface
/// even where there is nothing to press.
class HudHitTable
{
public:
  void Clear() noexcept;

  void AddTarget(const HudTarget& _target);

  /// A panel's area. A tap inside it is consumed whether or not it lands on a target.
  void AddBlocker(const HudRect& _panel);

  /// **TARGETS FIRST, THEN BLOCKERS, THEN THE WORLD.** No two targets overlap -- the suite asserts clear
  /// space between every pair -- so the first containing target is the only one.
  [[nodiscard]] HudHit Test(float _authoredX, float _authoredY) const noexcept;

  [[nodiscard]] std::span<const HudTarget> Targets() const noexcept
  {
    return m_targets;
  }

  [[nodiscard]] std::span<const HudRect> Blockers() const noexcept
  {
    return m_blockers;
  }

private:
  std::vector<HudTarget> m_targets;
  std::vector<HudRect> m_blockers;
};

/// **THE QUIT ARMS ON THE FIRST TAP, QUITS ON THE SECOND, AND DISARMS ITSELF AFTER FOUR SECONDS**
/// (`Interface.md` §6). There is no pause, no save and no rejoin, so a single stray contact would
/// otherwise end a five-minute match with no recovery path anywhere in the system.
///
/// It holds no clock: the caller passes the time in, which is what lets the suite pin the expiry to the
/// millisecond rather than sleeping for it -- the split `JoinState` took for the same reason.
class QuitConfirm
{
public:
  void Arm(std::uint64_t _nowMilliseconds) noexcept;
  void Disarm() noexcept;

  /// True from the arming tap until `QUIT_ARMED_MILLISECONDS` later, and never after.
  [[nodiscard]] bool IsArmed(std::uint64_t _nowMilliseconds) const noexcept;

private:
  std::uint64_t m_armedAtMilliseconds = 0;
  bool m_armed = false;
};

} // namespace Outpost
