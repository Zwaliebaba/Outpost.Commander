#include "pch.h"

#include "Interpolation.h"

namespace Outpost
{

Playout ComputePlayout(std::uint64_t _olderMilliseconds, std::uint64_t _newerMilliseconds, std::uint64_t _renderMilliseconds) noexcept
{
  // Two snapshots that do not straddle any time at all. A pair with the same timestamp is not a
  // pair, and dividing by the span would be the bug rather than reporting it.
  if (_newerMilliseconds <= _olderMilliseconds)
  {
    return Playout{0, PlayoutState::Starved};
  }

  const std::uint64_t spanMilliseconds = _newerMilliseconds - _olderMilliseconds;

  // BEFORE THE OLDER SNAPSHOT IS THE STORE'S BUG, NOT A FRAME TO REFUSE. It means the pair handed
  // over does not contain the render time, and the honest response is the oldest thing that is
  // true -- the older snapshot, exactly -- rather than a negative fraction that would run every
  // ship backwards along its last known line.
  if (_renderMilliseconds <= _olderMilliseconds)
  {
    return Playout{0, PlayoutState::Interpolating};
  }

  const std::uint64_t elapsedMilliseconds = _renderMilliseconds - _olderMilliseconds;

  // The fraction, in Fixed. The numerator is shifted before the divide so the fraction survives
  // it, and 64 bits carries that: a span of milliseconds shifted eight places is nowhere near the
  // width, and the clock is unsigned throughout, so there is no truncation-toward-zero asymmetry
  // to think about here (`NeuronCore/FixedPoint.h` documents the one that exists for signed).
  const std::uint64_t rawFraction = (elapsedMilliseconds << Neuron::FIXED_FRACTION_BITS) / spanMilliseconds;

  if (_renderMilliseconds <= _newerMilliseconds)
  {
    return Playout{static_cast<Neuron::Fixed>(rawFraction), PlayoutState::Interpolating};
  }

  // Past the newest snapshot: the caller is extending the last known motion, and the only
  // question left is for how long.
  const std::uint64_t beyondMilliseconds = _renderMilliseconds - _newerMilliseconds;
  if (beyondMilliseconds <= EXTRAPOLATION_BOUND_MILLISECONDS)
  {
    return Playout{static_cast<Neuron::Fixed>(rawFraction), PlayoutState::Extrapolating};
  }

  // HOLDING. The fraction freezes at the bound rather than at the newest snapshot, so a frame
  // that crosses the bound does not jump backwards to where the ship was: it stops where the
  // guess had reached and waits there for the truth.
  const std::uint64_t heldElapsed = (_newerMilliseconds + EXTRAPOLATION_BOUND_MILLISECONDS) - _olderMilliseconds;
  const std::uint64_t heldFraction = (heldElapsed << Neuron::FIXED_FRACTION_BITS) / spanMilliseconds;
  return Playout{static_cast<Neuron::Fixed>(heldFraction), PlayoutState::Holding};
}

Neuron::Fixed InterpolatePosition(Neuron::Fixed _from, Neuron::Fixed _to, Neuron::Fixed _fraction) noexcept
{
  // from + (to - from) * fraction, with the difference taken first so that two positions near the
  // play area's edge never form a sum that leaves the type. The difference of two coordinates in
  // a 16,384-unit square is bounded by the square; the sum of them is not bounded by anything
  // this function controls.
  const Neuron::Fixed difference = _to - _from;
  return _from + Neuron::Multiply(difference, _fraction);
}

Neuron::Angle InterpolateHeading(Neuron::Angle _from, Neuron::Angle _to, Neuron::Fixed _fraction) noexcept
{
  // THE SUBTRACTION IS THE ALGORITHM. AngleDifference wraps by construction and lands in
  // [-32768, +32767], which is the short way round already -- so scaling it and adding it back is
  // the whole of "interpolate a heading", with no branch on which way is shorter and no modulus.
  const std::int16_t difference = Neuron::AngleDifference(_from, _to);
  const Neuron::Fixed scaled = Neuron::Multiply(static_cast<Neuron::Fixed>(difference), _fraction);
  return static_cast<Neuron::Angle>(static_cast<Neuron::Angle>(_from) + static_cast<Neuron::Angle>(static_cast<std::uint32_t>(scaled)));
}

bool InterpolateRecord(const EntityRecord& _older, const EntityRecord& _newer, Neuron::Fixed _fraction, EntityRecord& _outRecord) noexcept
{
  // The whole identity, index and generation together. See the header for why the generation is
  // not optional here.
  if (_older.identity != _newer.identity)
  {
    return false;
  }

  const Neuron::Fixed positionX =
    InterpolatePosition(DequantizePosition(_older.positionX), DequantizePosition(_newer.positionX), _fraction);
  const Neuron::Fixed positionY =
    InterpolatePosition(DequantizePosition(_older.positionY), DequantizePosition(_newer.positionY), _fraction);
  const Neuron::Angle heading = InterpolateHeading(DequantizeWireHeading(_older.heading), DequantizeWireHeading(_newer.heading), _fraction);

  // Back onto the wire's own grid, through the quantizer that owns the rounding rule, so that an
  // interpolated record is the same kind of value as the two it came from rather than a third
  // representation the renderer has to know about.
  _outRecord = _newer;
  _outRecord.positionX = QuantizePosition(positionX);
  _outRecord.positionY = QuantizePosition(positionY);
  _outRecord.heading = static_cast<std::uint8_t>(heading >> 8);
  return true;
}

} // namespace Outpost
