#pragma once

#include "Camera.h"
#include "HitTest.h"
#include "Interpolation.h"

#include "GameCore.h"

#include "NeuronClient.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// **HOW LONG A TRACER IS DRAWN: A QUARTER OF A SECOND** (`OpenQuestions.md` Q81). A ship firing without pause
/// is sent one fire event per weapon every ten ticks, half a second (M3.2), so a quarter second on and a quarter
/// off reads as a weapon cycling rather than as a beam that never goes out. **Not tuned.**
inline constexpr std::uint64_t TRACER_LIFETIME_MILLISECONDS = 250;

/// **A FIRE EVENT SEEN AGAIN INSIDE THIS WINDOW IS THE SAME SHOT.** The host repeats every event in
/// `FIRE_REPEAT_TICKS` consecutive updates (ADR-004) and does not say which tick it was fired on, so the client
/// folds repeats by shooter, target and weapon. The window covers the three repeats with a margin for jitter,
/// and stays under the half second between two real shots from one ship.
inline constexpr std::uint64_t TRACER_REPEAT_WINDOW_MILLISECONDS = 300;
static_assert(TRACER_REPEAT_WINDOW_MILLISECONDS > (std::uint64_t{FIRE_REPEAT_TICKS} * SNAPSHOT_INTERVAL_MILLISECONDS),
              "a repeat of one shot must fall inside the window");
static_assert(TRACER_REPEAT_WINDOW_MILLISECONDS < 500, "a second shot half a second later must not");

/// The most tracers held at once. A match's worth of ships firing together is well under this, and past it a
/// new shot is dropped rather than allocated for inside the frame's drain.
inline constexpr std::size_t MAX_TRACERS = 256;

/// One shot being drawn. R8: a public aggregate.
struct Tracer
{
  FireEvent event;

  /// When the first copy of the event arrived.
  std::uint64_t arrivedMilliseconds = 0;
};

/// **THE SHOTS BEING DRAWN**, fed from each update's fire events as it is drained (ADR-004). Presentation only:
/// a tracer that is lost costs a flash of light and nothing the host will ask about.
class TracerSet
{
public:
  TracerSet();

  /// Folds in one update's fire events, arrived at _nowMilliseconds. Repeats inside the window are dropped, and
  /// so is everything past `MAX_TRACERS` -- so this never allocates.
  void Note(std::span<const FireEvent> _events, std::uint64_t _nowMilliseconds) noexcept;

  /// Drops every tracer that has finished drawing by _nowMilliseconds.
  void Expire(std::uint64_t _nowMilliseconds) noexcept;

  void Clear() noexcept;

  [[nodiscard]] std::span<const Tracer> Tracers() const noexcept
  {
    return m_tracers;
  }

private:
  std::vector<Tracer> m_tracers;
};

/// How many world units one scene-target pixel covers at the camera's focus: what turns a beam's width in pixels
/// into the width in world units the pass draws it at.
[[nodiscard]] float UnitsPerPixelAtFocus(const CameraPose& _camera, std::uint32_t _heightPixels) noexcept;

/// **EVERY BEAM THIS FRAME** (Q81), into _outBeams, which is cleared first:
///
/// - **A tracer for each shot**, from the shooter's drawn position to the target's, colored by weapon and fading
///   out over its lifetime. **It starts `INTERPOLATION_DELAY_MILLISECONDS` after the event arrived**, because the
///   hulls are drawn that far in the past and a shot drawn at once would fly from where the shooter will be.
///   A shot whose shooter or target is not drawn is skipped.
/// - **A mining beam for each extracting ship**, to the nearest rock within its mining reach.
/// - **An unloading beam for each unloading ship**, to its owner's nearest ore acceptor.
///
/// Positions are the interpolated records the meshes are drawn from, so a beam stays attached to its hulls.
void BuildBeams(const TracerSet& _tracers, std::span<const EntityRecord> _drawn, std::span<const RockPickPoint> _rocks,
                std::uint64_t _nowMilliseconds, float _unitsPerPixel, std::vector<Neuron::BeamInstance>& _outBeams);

} // namespace Outpost
