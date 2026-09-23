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

  // Owned by nobody. **No such entity exists before M3**: M2's rocks are the generated field and reach the
  // asteroid tier through `CandidatesUnderTap`'s rock pass, which also skips unowned records rather than
  // letting one through here.
  return PickTier::Asteroid;
}

namespace
{
/// A projected point back into authored pixels, and its distance from the tap. False when it is behind the
/// camera or outside the pick radius.
[[nodiscard]] bool WithinPickRadius(const HitTestRequest& _request, float _screenX, float _screenY, float& _outDistance) noexcept
{
  // The y axis flips: the projection counts up and authored pixels count down.
  const float authoredX = ((_screenX + 1.0f) * 0.5f) * _request.authoredWidth;
  const float authoredY = ((1.0f - _screenY) * 0.5f) * _request.authoredHeight;
  const float dx = authoredX - _request.authoredX;
  const float dy = authoredY - _request.authoredY;
  _outDistance = std::sqrt((dx * dx) + (dy * dy));
  return _outDistance <= Neuron::PICK_RADIUS_AUTHORED_PIXELS;
}
} // namespace

std::vector<PickCandidate> CandidatesUnderTap(const CameraPose& _pose, const HitTestRequest& _request,
                                              std::span<const EntityRecord> _entities, std::span<const RockPickPoint> _rocks)
{
  std::vector<PickCandidate> candidates;

  // M2.8: THE ROCKS, AT THEIR DRAWN CENTERS. The asteroid tier is below every entity's, so a rock under a
  // ship loses to the ship whatever the distances -- `ResolvePick` applies that, not this.
  for (const RockPickPoint& rock : _rocks)
  {
    float screenX = 0.0f;
    float screenY = 0.0f;
    float distance = 0.0f;
    if (WorldToScreen(_pose, _request.aspectRatio, rock.worldX, rock.worldY, rock.liftUnits, screenX, screenY) &&
        WithinPickRadius(_request, screenX, screenY, distance))
    {
      candidates.push_back(PickCandidate{
        .identity = NO_WIRE_IDENTITY, .tier = PickTier::Asteroid, .screenDistanceAuthoredPixels = distance, .rock = rock.rock});
    }
  }

  for (const EntityRecord& record : _entities)
  {
    // **AN UNOWNED ENTITY IS NOT A ROCK.** Rocks are the field above, not records, and nothing unowned is an
    // entity before M3 -- so a record here with no owner is skipped rather than handed the asteroid tier,
    // where a mine order would take it for rock zero.
    if (OwnerOf(record) == NO_PLAYER)
    {
      continue;
    }

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
    // arrived in.
    float distance = 0.0f;
    if (!WithinPickRadius(_request, screenX, screenY, distance))
    {
      continue;
    }

    candidates.push_back(
      PickCandidate{.identity = record.identity, .tier = TierOf(record, _request.player), .screenDistanceAuthoredPixels = distance});
  }

  return candidates;
}

} // namespace Outpost
