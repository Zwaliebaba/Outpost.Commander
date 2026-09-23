#include "pch.h"

#include "Selection.h"

#include <algorithm>

namespace Outpost
{

bool Selection::Contains(WireIdentity _identity) const noexcept
{
  return std::find(m_identities.begin(), m_identities.end(), _identity) != m_identities.end();
}

void Selection::ReplaceWith(WireIdentity _identity)
{
  m_identities.clear();
  m_identities.push_back(_identity);
}

void Selection::Add(WireIdentity _identity)
{
  if (!Contains(_identity))
  {
    m_identities.push_back(_identity);
  }
}

std::size_t Selection::RetainLiving(std::span<const EntityRecord> _entities)
{
  const std::size_t before = m_identities.size();

  // **THE WHOLE PACKED IDENTITY, NOT THE INDEX.** A slot reused since the selection was made carries
  // a new generation, so comparing the whole value is what stops the selection silently becoming
  // somebody else's ship -- which is the same guard `InterpolateRecord` applies for the same reason.
  const auto missing = [&_entities](WireIdentity _identity) noexcept
  {
    for (const EntityRecord& record : _entities)
    {
      if (record.identity == _identity)
      {
        return false;
      }
    }
    return true;
  };

  m_identities.erase(std::remove_if(m_identities.begin(), m_identities.end(), missing), m_identities.end());
  return before - m_identities.size();
}

OrderVerb VerbForPick(const TapOutcome& _outcome, bool _hasSelection) noexcept
{
  switch (_outcome.action)
  {
  case TapAction::Nothing:
    return OrderVerb::None;

  case TapAction::MoveTo:
    // The tap resolver has already applied the rule that empty space with nothing selected does
    // nothing; this is the other half of that row.
    return _hasSelection ? OrderVerb::MoveTo : OrderVerb::None;

  case TapAction::Occupied:
    break;
  }

  switch (_outcome.hit.tier)
  {
  case PickTier::OwnShip:
    // **REPLACES THE SELECTION**, which is section 4's row and is why a double tap can expand it
    // (ADR-017): the first tap has already acted.
    return OrderVerb::Select;

  case PickTier::OwnStructure:
    // **THE SELECTION IS UNCHANGED.** The one row of the table that is about the interface rather
    // than about the world.
    return OrderVerb::OpenBuildPanel;

  case PickTier::Hostile:
    // An attack needs something to attack WITH. With nothing selected the tap has no verb, which is
    // the same rule the empty-space row states and is not stated separately in section 4.
    return _hasSelection ? OrderVerb::Attack : OrderVerb::None;

  case PickTier::Asteroid:
    return _hasSelection ? OrderVerb::Mine : OrderVerb::None;

  case PickTier::EmptySpace:
    return _hasSelection ? OrderVerb::MoveTo : OrderVerb::None;
  }

  return OrderVerb::None;
}

SelectionOutcome Selection::Tap(const CameraPose& _pose, const HitTestRequest& _request, std::span<const EntityRecord> _entities)
{
  const std::vector<PickCandidate> candidates = CandidatesUnderTap(_pose, _request, _entities);

  const TapOutcome resolved = ResolveTap(_pose, _request.aspectRatio, _request.authoredX, _request.authoredY, _request.authoredWidth,
                                         _request.authoredHeight, candidates, !m_identities.empty());

  SelectionOutcome outcome;
  outcome.verb = VerbForPick(resolved, !m_identities.empty());
  outcome.worldX = resolved.worldX;
  outcome.worldY = resolved.worldY;
  outcome.target = (resolved.action == TapAction::Occupied) ? resolved.hit.identity : NO_WIRE_IDENTITY;

  if (outcome.verb == OrderVerb::Select)
  {
    // **IT FIRES NOW AND EXPANDS LATER** (ADR-017). Nothing is deferred waiting to see whether a
    // second tap arrives, so no tap in this game got slower -- and an expansion abandoned halfway
    // leaves one ship selected rather than nothing.
    ReplaceWith(outcome.target);
    outcome.selectionChanged = true;
  }

  return outcome;
}

} // namespace Outpost
