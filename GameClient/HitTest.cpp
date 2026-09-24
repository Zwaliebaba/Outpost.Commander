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
    // commoner intent than opening the build panel. **A module is a structure too** (M2.11,
    // `Interface.md` section 1's pick order puts it with the station).
    return ((design == DesignId::Station) || IsModule(design)) ? PickTier::OwnStructure : PickTier::OwnShip;
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
/// A projected point back into authored pixels. The y axis flips: the projection counts up and authored
/// pixels count down.
void ToAuthored(const HitTestRequest& _request, float _screenX, float _screenY, float& _outX, float& _outY) noexcept
{
  _outX = ((_screenX + 1.0f) * 0.5f) * _request.authoredWidth;
  _outY = ((1.0f - _screenY) * 0.5f) * _request.authoredHeight;
}

/// A projected point's distance from the tap, in authored pixels.
[[nodiscard]] float DistanceFromTap(const HitTestRequest& _request, float _screenX, float _screenY) noexcept
{
  float authoredX = 0.0f;
  float authoredY = 0.0f;
  ToAuthored(_request, _screenX, _screenY, authoredX, authoredY);
  const float dx = authoredX - _request.authoredX;
  const float dy = authoredY - _request.authoredY;
  return std::sqrt((dx * dx) + (dy * dy));
}

/// A projected point's distance from the tap, and whether it is within the pick radius. False when it is
/// outside.
[[nodiscard]] bool WithinPickRadius(const HitTestRequest& _request, float _screenX, float _screenY, float& _outDistance) noexcept
{
  _outDistance = DistanceFromTap(_request, _screenX, _screenY);
  return _outDistance <= Neuron::PICK_RADIUS_AUTHORED_PIXELS;
}

/// **WHETHER A POINT ON THE PLANE IS UNDER A HULL**: within half its size (Q37's `sizeUnits`) of its center,
/// measured on the plane, where the hull's footprint is a circle whatever the camera is doing. Measuring on
/// the glass instead would stretch the footprint along whichever axis the pitch does not foreshorten, and a
/// tap on the ground just behind a station would pick it. False for a design the catalog does not know.
[[nodiscard]] bool UnderHull(const EntityRecord& _record, float _worldX, float _worldY, float _tapWorldX, float _tapWorldY) noexcept
{
  if (_record.designIdentity >= Designs().size())
  {
    return false;
  }
  const float half = static_cast<float>(Hull(Design(static_cast<DesignId>(_record.designIdentity)).hull).sizeUnits) * 0.5f;
  const float dx = _tapWorldX - _worldX;
  const float dy = _tapWorldY - _worldY;
  return ((dx * dx) + (dy * dy)) <= (half * half);
}
} // namespace

std::vector<PickCandidate> CandidatesUnderTap(const CameraPose& _pose, const HitTestRequest& _request,
                                              std::span<const EntityRecord> _entities, std::span<const RockPickPoint> _rocks)
{
  std::vector<PickCandidate> candidates;

  // THE TAP ON THE PLANE, once, for `UnderHull`. A tap above the horizon has no point on the plane, and
  // then only the pick radius can find anything.
  float tapWorldX = 0.0f;
  float tapWorldY = 0.0f;
  const bool tapOnPlane = ScreenToPlane(_pose, _request.aspectRatio, ((_request.authoredX / _request.authoredWidth) * 2.0f) - 1.0f,
                                        1.0f - ((_request.authoredY / _request.authoredHeight) * 2.0f), tapWorldX, tapWorldY);

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
    //
    // **THE PICK RADIUS IS A FLOOR, NOT THE WHOLE REACH.** `Interface.md` section 4 says a tap's meaning
    // comes from what is under it, and a tap on a station's hull is under the station however far from
    // its center it lands. Until 2026-09-24 only the 24 pixels around the center counted. That held while
    // the camera opened at 2,400 units and a station was 120 pixels across. At 1,200 it is 240, and a tap
    // on its body became an order to fly into it. So a tap is on an entity when it is within the pick
    // radius of its center **or** on the plane inside its hull, and the tier order still decides between
    // things that overlap.
    const float distance = DistanceFromTap(_request, screenX, screenY);
    if ((distance > Neuron::PICK_RADIUS_AUTHORED_PIXELS) && !(tapOnPlane && UnderHull(record, worldX, worldY, tapWorldX, tapWorldY)))
    {
      continue;
    }

    candidates.push_back(
      PickCandidate{.identity = record.identity, .tier = TierOf(record, _request.player), .screenDistanceAuthoredPixels = distance});
  }

  return candidates;
}

} // namespace Outpost
