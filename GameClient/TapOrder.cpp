#include "pch.h"

#include "TapOrder.h"

namespace Outpost
{

bool ResolvePick(std::span<const PickCandidate> _candidates, PickCandidate& _outHit) noexcept
{
  const PickCandidate* best = nullptr;

  for (const PickCandidate& candidate : _candidates)
  {
    // Outside the radius is not a candidate at all. EXCEEDING rejects, so a ship exactly 24
    // authored pixels away is still hittable -- the same edge rule the seam's palm rejection uses
    // (`Neuron::IsFingertipContact`), because two thresholds a finger's width apart that round
    // opposite ways is the kind of difference nobody can hold in their head.
    if (candidate.screenDistanceAuthoredPixels > Neuron::PICK_RADIUS_AUTHORED_PIXELS)
    {
      continue;
    }

    // EmptySpace is an outcome rather than a candidate; a caller that puts one in the list is
    // describing nothing, and it must not win over a real entity by being nearer.
    if (candidate.tier == PickTier::EmptySpace)
    {
      continue;
    }

    if (best == nullptr)
    {
      best = &candidate;
      continue;
    }

    // THE TIER WINS ACROSS, THE DISTANCE WINS INSIDE (`Interface.md` section 1). Strictly less on
    // the distance, so that equal candidates in one tier keep the order the caller gave them rather
    // than the order the comparison happens to visit them in.
    if (candidate.tier < best->tier)
    {
      best = &candidate;
    }
    else if ((candidate.tier == best->tier) && (candidate.screenDistanceAuthoredPixels < best->screenDistanceAuthoredPixels))
    {
      best = &candidate;
    }
  }

  if (best == nullptr)
  {
    return false;
  }

  _outHit = *best;
  return true;
}

TapOutcome ResolveTap(const CameraPose& _pose, float _aspectRatio, float _authoredX, float _authoredY, float _authoredWidth,
                      float _authoredHeight, std::span<const PickCandidate> _candidates, bool _hasSelection) noexcept
{
  TapOutcome outcome;

  // What is under the tap decides the verb before the plane does (`Interface.md` section 4): a tap
  // on a ship is about that ship wherever the ground beneath it happens to be.
  if (ResolvePick(_candidates, outcome.hit))
  {
    outcome.action = TapAction::Occupied;
    return outcome;
  }

  // Empty space, and with nothing selected that is deliberately nothing at all.
  if (!_hasSelection)
  {
    return outcome;
  }

  float screenX = 0.0f;
  float screenY = 0.0f;
  AuthoredToNormalized(_authoredX, _authoredY, _authoredWidth, _authoredHeight, screenX, screenY);

  if (!ScreenToPlane(_pose, _aspectRatio, screenX, screenY, outcome.worldX, outcome.worldY))
  {
    // Above the horizon. The pitch floor should have made this unreachable.
    return outcome;
  }

  outcome.action = TapAction::MoveTo;
  return outcome;
}

Command BuildMoveCommand(std::uint16_t _sequence, float _worldX, float _worldY, std::span<const std::uint16_t> _selection) noexcept
{
  Command command;
  command.sequence = _sequence;
  command.type = CommandType::MoveTo;

  // Through `Fixed` and then through `GameCore`'s quantizer, so that the rounding and the
  // saturation at the play area's corner are the wire's rules rather than this file's.
  const Neuron::Fixed fixedX = static_cast<Neuron::Fixed>(_worldX * static_cast<float>(Neuron::FIXED_ONE));
  const Neuron::Fixed fixedY = static_cast<Neuron::Fixed>(_worldY * static_cast<float>(Neuron::FIXED_ONE));
  command.targetX = QuantizePosition(fixedX);
  command.targetY = QuantizePosition(fixedY);

  command.selection.assign(_selection.begin(), _selection.end());
  return command;
}

} // namespace Outpost
