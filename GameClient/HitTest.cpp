#include "pch.h"

#include "HitTest.h"

#include <cmath>

namespace Outpost
{

PlayerId OwnerOf(const EntityRecord& _record) noexcept
{
  return _record.owner;
}

PickTier TierOf(const EntityRecord& _record, PlayerId _player) noexcept
{
  const PlayerId owner = OwnerOf(_record);
  const auto design = static_cast<DesignId>(_record.designIdentity);

  if ((owner != NO_PLAYER) && (owner == _player))
  {
    // **A STATION IS A STRUCTURE AND A SHIP IS A SHIP**, and the difference is what the tier order
    // exists for: a tap that lands on both takes the ship, because replacing the selection is the
    // commoner intent than opening the build panel.
    return (design == DesignId::Station) ? PickTier::OwnStructure : PickTier::OwnShip;
  }

  if (owner != NO_PLAYER)
  {
    return PickTier::Hostile;
  }

  // Owned by nobody. At M1 that is nothing at all; from M2 it is an asteroid, which is the tier
  // `Interface.md` section 4's mine order resolves against.
  return PickTier::Asteroid;
}

std::vector<PickCandidate> CandidatesUnderTap(const CameraPose& _pose, const HitTestRequest& _request,
                                              std::span<const EntityRecord> _entities)
{
  std::vector<PickCandidate> candidates;

  for (const EntityRecord& record : _entities)
  {
    const float worldX = static_cast<float>(DequantizePosition(record.positionX)) / static_cast<float>(Neuron::FIXED_ONE);
    const float worldY = static_cast<float>(DequantizePosition(record.positionY)) / static_cast<float>(Neuron::FIXED_ONE);

    float screenX = 0.0f;
    float screenY = 0.0f;
    if (!PlaneToScreen(_pose, _request.aspectRatio, worldX, worldY, screenX, screenY))
    {
      // Behind the camera. A candidate that projected to the far side of the eye would be
      // selectable through the floor, which is the one way a hit test can be worse than useless.
      continue;
    }

    // Back into authored pixels, which is the space the radius is stated in and the space the tap
    // arrived in. The y axis flips: the projection counts up and authored pixels count down.
    const float authoredX = ((screenX + 1.0f) * 0.5f) * _request.authoredWidth;
    const float authoredY = ((1.0f - screenY) * 0.5f) * _request.authoredHeight;

    const float dx = authoredX - _request.authoredX;
    const float dy = authoredY - _request.authoredY;
    const float distance = std::sqrt((dx * dx) + (dy * dy));

    if (distance > Neuron::PICK_RADIUS_AUTHORED_PIXELS)
    {
      continue;
    }

    candidates.push_back(
      PickCandidate{.identity = record.identity, .tier = TierOf(record, _request.player), .screenDistanceAuthoredPixels = distance});
  }

  return candidates;
}

} // namespace Outpost
