#include "pch.h"

#include "GroupSelection.h"

#include <cmath>

namespace Outpost
{

namespace
{
/// Where a record lands in authored pixels, or false when it is behind the camera. The same
/// projection `CandidatesUnderTap` makes, and it is duplicated here rather than shared because the
/// two want different answers from it: one wants the distance to a tap, this wants the point.
[[nodiscard]] bool AuthoredPointOf(const CameraPose& _pose, const HitTestRequest& _request, const EntityRecord& _record, float& _outX,
                                   float& _outY) noexcept
{
  const float worldX = static_cast<float>(DequantizePosition(_record.positionX)) / static_cast<float>(Neuron::FIXED_ONE);
  const float worldY = static_cast<float>(DequantizePosition(_record.positionY)) / static_cast<float>(Neuron::FIXED_ONE);

  float screenX = 0.0f;
  float screenY = 0.0f;
  if (!PlaneToScreen(_pose, _request.aspectRatio, worldX, worldY, screenX, screenY))
  {
    return false;
  }

  _outX = ((screenX + 1.0f) * 0.5f) * _request.authoredWidth;
  _outY = ((1.0f - screenY) * 0.5f) * _request.authoredHeight;
  return true;
}

[[nodiscard]] const EntityRecord* FindRecord(std::span<const EntityRecord> _entities, std::uint16_t _identity) noexcept
{
  for (const EntityRecord& record : _entities)
  {
    if (record.identity == _identity)
    {
      return &record;
    }
  }
  return nullptr;
}
} // namespace

std::vector<std::uint16_t> ShipsInGroupCircle(const CameraPose& _pose, const HitTestRequest& _request,
                                              std::span<const EntityRecord> _entities, std::uint16_t _anchorIdentity)
{
  std::vector<std::uint16_t> taken;

  const EntityRecord* anchor = FindRecord(_entities, _anchorIdentity);
  if (anchor == nullptr)
  {
    return taken;
  }

  // **CENTERED ON THE SHIP AND NOT ON THE FINGER**, because the finger is covering the ship
  // (`Interface.md` section 4). It is also what makes the circle stable while the fleet moves: the
  // finger is where it was, the ship is where the newest snapshot put it.
  float centerX = 0.0f;
  float centerY = 0.0f;
  if (!AuthoredPointOf(_pose, _request, *anchor, centerX, centerY))
  {
    return taken;
  }

  const auto anchorDesign = static_cast<DesignId>(anchor->designIdentity);
  const PlayerId anchorOwner = OwnerOf(*anchor);
  if (anchorOwner == NO_PLAYER)
  {
    // Nobody's ship anchors nothing. A double tap on an asteroid is not an expansion.
    return taken;
  }

  for (const EntityRecord& record : _entities)
  {
    // **OWN SHIPS ONLY, SAME DESIGN ONLY.** Both are ADR-017's and both are what makes the circle a
    // rule a player can predict rather than a lasso.
    if (OwnerOf(record) != anchorOwner)
    {
      continue;
    }
    if (static_cast<DesignId>(record.designIdentity) != anchorDesign)
    {
      continue;
    }

    float x = 0.0f;
    float y = 0.0f;
    if (!AuthoredPointOf(_pose, _request, record, x, y))
    {
      continue;
    }

    const float dx = x - centerX;
    const float dy = y - centerY;

    // **AT OR INSIDE**, so a ship exactly on the edge is taken. `TechnicalDesign.md` section 8 names
    // that case by itself, because "on the edge" is where a radius test is decided by a rounding
    // nobody chose.
    if (((dx * dx) + (dy * dy)) <= (GROUP_RADIUS_AUTHORED_PIXELS * GROUP_RADIUS_AUTHORED_PIXELS))
    {
      taken.push_back(record.identity);
    }
  }

  return taken;
}

ExpansionOutcome ExpandSelection(Selection& _selection, const CameraPose& _pose, const HitTestRequest& _request,
                                 std::span<const EntityRecord> _entities, std::uint16_t _anchorIdentity, std::uint16_t _tappedIdentity)
{
  ExpansionOutcome outcome;
  outcome.selected = _selection.Count();

  // **THE SAME ENTITY, MATCHED ON IDENTITY.** Two taps on DIFFERENT ships stay two single taps --
  // ADR-017's third case, and the reason this compares identities rather than screen positions: the
  // fleet is moving, which is when the gesture is used.
  if ((_anchorIdentity == 0) || (_anchorIdentity != _tappedIdentity))
  {
    return outcome;
  }

  const std::vector<std::uint16_t> group = ShipsInGroupCircle(_pose, _request, _entities, _anchorIdentity);
  if (group.empty())
  {
    return outcome;
  }

  // **IT CAN ONLY ADD.** The first tap already selected the anchor and already fired; this upgrades
  // that result, so a selection that was already expanded and is tapped again simply stays expanded.
  for (const std::uint16_t identity : group)
  {
    _selection.Add(identity);
  }

  outcome.expanded = true;
  outcome.selected = _selection.Count();
  return outcome;
}

} // namespace Outpost
