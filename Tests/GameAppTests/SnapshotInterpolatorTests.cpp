#include "pch.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <tuple>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr float SECONDS_PER_TICK = 1.0f / TICKS_PER_SECOND;
constexpr float FRAME_SECONDS = 1.0f / 60.0f;
constexpr float TOLERANCE = 1e-4f;
constexpr Outpost::EntityId SHIP{1};
constexpr Outpost::EntityId OTHER{2};

// A snapshot of one ship at a place and heading, and optionally a second ship.
Outpost::Snapshot At(std::uint64_t _tick, float _xMeters, float _headingRadians = 0.0f, bool _withOther = false)
{
  Outpost::Snapshot snapshot{.tick = _tick, .player = Outpost::PlayerId{1}};
  snapshot.entities.push_back({.id = SHIP, .position = {.xMeters = _xMeters, .zMeters = 0.0f}, .headingRadians = _headingRadians});
  if (_withOther)
    snapshot.entities.push_back({.id = OTHER, .position = {.xMeters = 500.0f, .zMeters = 0.0f}});
  return snapshot;
}

float ShipX(const Outpost::SnapshotInterpolator& _view)
{
  return _view.Entities().front().position.xMeters;
}

// A link as a state crosses it: half the round trip, its jitter either way, and its packets' loss. A state is six packets,
// and one that loses any arrives a round trip later, unless a newer one has overtaken it.
struct Link
{
  double delaySeconds = 0.0;
  double jitterSeconds = 0.0;
  double lossShare = 0.0;
};

constexpr std::size_t PACKETS_A_STATE = 6;
// The links the network is measured over, each way (owner, 2026-10-26): near, Wi-Fi and a phone's hotspot.
constexpr Link NEAR_LINK{.delaySeconds = 0.010, .jitterSeconds = 0.005, .lossShare = 0.001};
constexpr Link WI_FI_LINK{.delaySeconds = 0.020, .jitterSeconds = 0.015, .lossShare = 0.01};
constexpr Link HOTSPOT_LINK{.delaySeconds = 0.040, .jitterSeconds = 0.030, .lossShare = 0.02};

// When each tick's state from _first to _last arrives over _link, by a seed, in the order they arrive.
std::vector<std::pair<double, std::uint64_t>> Arrivals(const Link& _link, std::uint64_t _first, std::uint64_t _last, std::uint64_t _seed)
{
  Outpost::EffectRandom random(_seed);
  std::vector<std::pair<double, std::uint64_t>> arrivals;
  for (std::uint64_t tick = _first; tick <= _last; ++tick)
  {
    double arrives = (static_cast<double>(tick) * SECONDS_PER_TICK) + _link.delaySeconds +
                     static_cast<double>(random.Signed(static_cast<float>(_link.jitterSeconds)));
    bool lost = false;
    for (std::size_t packet = 0; packet < PACKETS_A_STATE; ++packet)
      lost = lost || random.Unit() < static_cast<float>(_link.lossShare);
    if (lost)
      arrives += 2.0 * _link.delaySeconds;
    arrivals.emplace_back(arrives, tick);
  }
  std::ranges::sort(arrivals);
  return arrivals;
}

// Plays _arrivals into _view a 60 Hz frame at a time, from _fromFrame to _toFrame, each frame taking what has arrived by
// its start; _frame is called after each frame's Advance.
void Play(Outpost::SnapshotInterpolator& _view, const std::vector<std::pair<double, std::uint64_t>>& _arrivals, std::size_t& _next,
          int _fromFrame, int _toFrame, const std::function<void(int)>& _frame)
{
  for (int frame = _fromFrame; frame < _toFrame; ++frame)
  {
    const double now = static_cast<double>(frame) * FRAME_SECONDS;
    while (_next < _arrivals.size() && _arrivals[_next].first <= now)
    {
      const std::uint64_t tick = _arrivals[_next].second;
      _view.Receive(At(tick, static_cast<float>(tick)));
      ++_next;
    }
    _view.Advance(FRAME_SECONDS);
    _frame(frame);
  }
}
} // namespace

TEST_CLASS(SnapshotInterpolatorTests)
{
public:
  TEST_METHOD(ShowsNothingUntilASnapshotArrives)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Advance(FRAME_SECONDS);
    Assert::IsTrue(view.IsEmpty());
    Assert::IsTrue(view.Entities().empty());
  }

  TEST_METHOD(RunsOneTickBehindTheNewestSnapshot)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f));
    view.Advance(0.0f);
    Assert::AreEqual(10.0, view.ViewTick(), 1e-9);
    Assert::AreEqual(0.0f, ShipX(view), TOLERANCE);
  }

  // The interpolation math: halfway between two ticks, halfway between two places.
  TEST_METHOD(PlacesAShipBetweenTwoSnapshots)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f));
    view.Advance(0.0f);
    view.Advance(SECONDS_PER_TICK / 2.0f);
    Assert::AreEqual(10.5, view.ViewTick(), 1e-6);
    Assert::AreEqual(5.0f, ShipX(view), TOLERANCE);
  }

  TEST_METHOD(TurnsTheShortWayRound)
  {
    constexpr float PI = std::numbers::pi_v<float>;
    // From just below +x to just above it through zero, not the long way through pi.
    const float heading = Outpost::InterpolateHeading(-0.1f, 0.1f, 0.5f);
    Assert::AreEqual(0.0f, heading, TOLERANCE);
    // Across the seam at pi: from 170 degrees to -170 degrees is 20 degrees through 180.
    const float across = Outpost::InterpolateHeading(PI - 0.1745f, -PI + 0.1745f, 0.5f);
    Assert::AreEqual(PI, std::abs(std::remainder(across, 2.0f * PI)), 1e-3f);
  }

  // The view moves smoothly at 60 fps from a 20 Hz tick (ADR-013 decision 3). With a snapshot arriving every third frame,
  // the ship's shown position must move forward every frame, by close to the same step, and never jump back.
  TEST_METHOD(MovesSmoothlyAtSixtyFramesFromTwentyTicks)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    constexpr float METERS_PER_TICK = 3.0f;
    std::uint64_t tick = 0;
    view.Receive(At(tick, 0.0f));
    view.Advance(0.0f);

    float previous = ShipX(view);
    // Settle the clock, then measure.
    std::vector<float> steps;
    for (int frame = 1; frame <= 300; ++frame)
    {
      if (frame % 3 == 0)
      {
        ++tick;
        view.Receive(At(tick, static_cast<float>(tick) * METERS_PER_TICK));
      }
      view.Advance(FRAME_SECONDS);
      const float x = ShipX(view);
      if (frame > 60)
        steps.push_back(x - previous);
      previous = x;
    }
    const float expected = METERS_PER_TICK * TICKS_PER_SECOND * FRAME_SECONDS;
    for (const float step : steps)
    {
      Assert::IsTrue(step > 0.0f);
      Assert::AreEqual(expected, step, expected * 0.25f);
    }
  }

  TEST_METHOD(HoldsOnTheNewestWhenSnapshotsStop)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f));
    for (int frame = 0; frame < 60; ++frame)
      view.Advance(FRAME_SECONDS);
    Assert::AreEqual(11.0, view.ViewTick(), 1e-9);
    Assert::AreEqual(10.0f, ShipX(view), TOLERANCE);
  }

  // ADR-029: how a ship moved between the two snapshots around the view, for banking: its speed, and its turn,
  // counterclockwise positive and the short way round.
  TEST_METHOD(ReadsEachShipsMotionBetweenTwoSnapshots)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f, 3.0f));
    view.Receive(At(11, 4.0f, -3.0f, true));
    view.Advance(0.0f);
    view.Advance(SECONDS_PER_TICK / 2.0f);
    const std::vector<Outpost::EntityMotion> motions = view.Motions();
    Assert::AreEqual(size_t{1}, motions.size(), L"the second ship is new, so it has no motion yet");
    Assert::IsTrue(motions.front().id == SHIP);
    Assert::AreEqual(4.0f * TICKS_PER_SECOND, motions.front().speedMetersPerSecond, TOLERANCE);
    const float turned = std::remainder(-3.0f - 3.0f, 2.0f * std::numbers::pi_v<float>);
    Assert::AreEqual(turned * TICKS_PER_SECOND, motions.front().turnRadiansPerSecond, 1e-3f);
    Assert::IsTrue(motions.front().turnRadiansPerSecond > 0.0f, L"from 3 to -3 radians is counterclockwise through pi");
  }

  // When snapshots stop the view holds still, so nothing is moving.
  TEST_METHOD(ReadsNoMotionWhenSnapshotsStop)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f, 1.0f));
    for (int frame = 0; frame < 60; ++frame)
      view.Advance(FRAME_SECONDS);
    Assert::IsTrue(view.Motions().empty());
  }

  TEST_METHOD(IgnoresASnapshotThatIsNotNewer)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(10, 99.0f));
    view.Receive(At(9, 99.0f));
    Assert::AreEqual(std::uint64_t{10}, view.Newest().tick);
    Assert::AreEqual(0.0f, view.Newest().entities.front().position.xMeters);
  }

  // ADR-013 decision 3: states that arrive every tick keep the delay at a tick, and the view a tick behind the newest as
  // each arrives.
  TEST_METHOD(KeepsTheDelayAtATickWhenStatesArriveEveryTick)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    std::vector<std::pair<double, std::uint64_t>> arrivals;
    for (std::uint64_t tick = 1; tick <= 600; ++tick)
      arrivals.emplace_back(static_cast<double>(tick) * SECONDS_PER_TICK, tick);
    std::size_t next = 0;
    std::uint64_t newest = 0;
    Play(view, arrivals, next, 0, 1800,
         [&](int _frame)
         {
           Assert::AreEqual(Outpost::SnapshotInterpolator::LEAST_DELAY_TICKS, view.DelayTicks(), 1e-3);
           if (view.IsEmpty())
             return;
           if (_frame > 120 && view.Newest().tick != newest)
             Assert::AreEqual(static_cast<double>(view.Newest().tick) - 1.0, view.ViewTick(), 0.4, L"a tick behind the newest");
           newest = view.Newest().tick;
         });
  }

  // ADR-013 decision 3: with the jitter and loss of each of the network's links, the view stands still in no more frames
  // than the share its budget allows the newest state to be late: 1% on near and Wi-Fi, 5% on a hotspot.
  TEST_METHOD(RidesOutEachLinksJitter)
  {
    const std::array<std::tuple<Link, double, const wchar_t*>, 3> links{
      {{NEAR_LINK, 0.01, L"near"}, {WI_FI_LINK, 0.01, L"Wi-Fi"}, {HOTSPOT_LINK, 0.05, L"hotspot"}}};
    std::uint64_t seed = 1;
    for (const auto& [link, allowed, name] : links)
    {
      Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
      const std::vector<std::pair<double, std::uint64_t>> arrivals = Arrivals(link, 1, 1300, seed++);
      std::size_t next = 0;
      double previous = 0.0;
      int frames = 0;
      int still = 0;
      // A minute, after two seconds in which the delay finds the link.
      Play(view, arrivals, next, 0, 3720,
           [&](int _frame)
           {
             if (_frame >= 120)
             {
               ++frames;
               still += view.ViewTick() == previous ? 1 : 0;
             }
             previous = view.ViewTick();
           });
      const double share = static_cast<double>(still) / static_cast<double>(frames);
      Assert::IsTrue(
        share <= allowed,
        std::format(L"{}: still in {} of {} frames, at a delay of {:.2f} ticks", name, still, frames, view.DelayTicks()).c_str());
    }
  }

  // ADR-013 decision 3: when states stop and then come all at once, the view plays on to them, never more than a tick in a
  // frame and never back, rather than jumping.
  TEST_METHOD(PlaysOnAfterAStallRatherThanJumping)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    // Five seconds on time, a second of nothing, and then that second's twenty at once, and on time again.
    std::vector<std::pair<double, std::uint64_t>> arrivals;
    for (std::uint64_t tick = 1; tick <= 400; ++tick)
    {
      const bool stalled = tick > 100 && tick <= 120;
      arrivals.emplace_back(stalled ? 121.0 * SECONDS_PER_TICK : static_cast<double>(tick) * SECONDS_PER_TICK, tick);
    }
    std::ranges::sort(arrivals);
    std::size_t next = 0;
    double previous = 0.0;
    Play(view, arrivals, next, 0, 1200,
         [&](int _frame)
         {
           if (_frame > 0)
           {
             Assert::IsTrue(view.ViewTick() >= previous, L"never back");
             Assert::IsTrue(view.ViewTick() - previous <= 1.0 + 1e-6, std::format(L"frame {}: no more than a tick", _frame).c_str());
           }
           previous = view.ViewTick();
         });
    Assert::IsTrue(static_cast<double>(view.Newest().tick) - view.ViewTick() <= view.DelayTicks() + 0.5, L"and it caught up");
  }

  // ADR-013 decision 3: the delay a jittery link set comes back down to about a tick within the window once the jitter ends.
  TEST_METHOD(BringsTheDelayBackDownOnceTheJitterEnds)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    // Twenty seconds of Wi-Fi, and then its half round trip alone.
    std::vector<std::pair<double, std::uint64_t>> arrivals = Arrivals(WI_FI_LINK, 1, 400, 7);
    for (std::uint64_t tick = 401; tick <= 700; ++tick)
      arrivals.emplace_back((static_cast<double>(tick) * SECONDS_PER_TICK) + WI_FI_LINK.delaySeconds, tick);
    std::ranges::sort(arrivals);
    std::size_t next = 0;
    double jittery = 0.0;
    // The jitter ends with tick 400's state, about frame 1,200; ten seconds is 600 frames more.
    Play(view, arrivals, next, 0, 1200, [&](int) { jittery = std::max(jittery, view.DelayTicks()); });
    Assert::IsTrue(jittery > 1.5, std::format(L"the jitter raised the delay, to {:.2f}", jittery).c_str());
    Play(view, arrivals, next, 1200, 1810, [](int) {});
    Assert::AreEqual(Outpost::SnapshotInterpolator::LEAST_DELAY_TICKS, view.DelayTicks(), 0.05, L"back to a tick, within a twentieth");
  }

  TEST_METHOD(ShowsANewEntityWhereItIs)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f, 0.0f, true));
    view.Advance(0.0f);
    view.Advance(SECONDS_PER_TICK / 2.0f);
    const std::vector<Outpost::EntityView> entities = view.Entities();
    Assert::AreEqual(size_t{2}, entities.size());
    Assert::AreEqual(500.0f, entities[1].position.xMeters);
  }
};
} // namespace GameAppTests