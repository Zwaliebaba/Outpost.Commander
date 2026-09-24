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

void Selection::PositionsOf(std::span<const EntityRecord> _entities, std::vector<float>& _outX, std::vector<float>& _outY) const
{
  _outX.clear();
  _outY.clear();
  for (const WireIdentity identity : m_identities)
  {
    for (const EntityRecord& record : _entities)
    {
      if (record.identity == identity)
      {
        _outX.push_back(static_cast<float>(DequantizePosition(record.positionX)) / static_cast<float>(Neuron::FIXED_ONE));
        _outY.push_back(static_cast<float>(DequantizePosition(record.positionY)) / static_cast<float>(Neuron::FIXED_ONE));
        break;
      }
    }
  }
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

SelectionOutcome Selection::Tap(const CameraPose& _pose, const HitTestRequest& _request, std::span<const EntityRecord> _entities,
                                std::span<const RockPickPoint> _rocks)
{
  const std::vector<PickCandidate> candidates = CandidatesUnderTap(_pose, _request, _entities, _rocks);

  const TapOutcome resolved = ResolveTap(_pose, _request.aspectRatio, _request.authoredX, _request.authoredY, _request.authoredWidth,
                                         _request.authoredHeight, candidates, !m_identities.empty());

  SelectionOutcome outcome;
  outcome.verb = VerbForPick(resolved, !m_identities.empty());
  outcome.worldX = resolved.worldX;
  outcome.worldY = resolved.worldY;
  outcome.target = (resolved.action == TapAction::Occupied) ? resolved.hit.identity : NO_WIRE_IDENTITY;

  // **A TAP ON YOUR OWN MODULE HAS NO VERB** (M2.11, `OpenQuestions.md` Q57). It is picked at the structure tier,
  // so it wins over whatever is underneath, and then it does nothing: the station is the one structure that
  // opens the build panel, and an armed upgrade -- the one thing a module is the target of -- is resolved
  // before this is reached (`ResolvePlacementTap`).
  if (outcome.verb == OrderVerb::OpenBuildPanel)
  {
    for (const EntityRecord& record : _entities)
    {
      if ((record.identity == outcome.target) && IsModule(static_cast<DesignId>(record.designIdentity)))
      {
        outcome.verb = OrderVerb::None;
        break;
      }
    }
  }

  if (outcome.verb == OrderVerb::Mine)
  {
    // THE ROCK AND WHERE IT IS ON THE PLANE -- its field position, not the tap's, and not its drawn height:
    // the ships that cannot mine are sent to the rock, and the simulation has no height (R22).
    outcome.rock = resolved.hit.rock;
    for (const RockPickPoint& rock : _rocks)
    {
      if (rock.rock == outcome.rock)
      {
        outcome.worldX = rock.worldX;
        outcome.worldY = rock.worldY;
        break;
      }
    }
  }

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
