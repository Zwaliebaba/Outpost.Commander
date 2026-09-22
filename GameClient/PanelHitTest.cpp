#include "pch.h"

#include "PanelHitTest.h"

namespace Outpost
{

void HudHitTable::Clear() noexcept
{
  m_targets.clear();
  m_blockers.clear();
}

void HudHitTable::AddTarget(const HudTarget& _target)
{
  m_targets.push_back(_target);
}

void HudHitTable::AddBlocker(const HudRect& _panel)
{
  m_blockers.push_back(_panel);
}

HudHit HudHitTable::Test(float _authoredX, float _authoredY) const noexcept
{
  for (const HudTarget& target : m_targets)
  {
    if (target.hit.Contains(_authoredX, _authoredY))
    {
      return HudHit{.consumed = true, .action = target.action, .argument = target.argument};
    }
  }

  for (const HudRect& panel : m_blockers)
  {
    if (panel.Contains(_authoredX, _authoredY))
    {
      return HudHit{.consumed = true, .action = HudAction::None, .argument = 0};
    }
  }

  return HudHit{};
}

void QuitConfirm::Arm(std::uint64_t _nowMilliseconds) noexcept
{
  m_armedAtMilliseconds = _nowMilliseconds;
  m_armed = true;
}

void QuitConfirm::Disarm() noexcept
{
  m_armed = false;
}

bool QuitConfirm::IsArmed(std::uint64_t _nowMilliseconds) const noexcept
{
  // `<` rather than `<=`: at exactly four seconds it has expired. A clock that went backwards reads as
  // still armed rather than as a huge interval, which is the safe side of that mistake.
  return m_armed && ((_nowMilliseconds < m_armedAtMilliseconds) || ((_nowMilliseconds - m_armedAtMilliseconds) < QUIT_ARMED_MILLISECONDS));
}

} // namespace Outpost
