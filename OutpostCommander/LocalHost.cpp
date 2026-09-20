#include "pch.h"

#include "LocalHost.h"

#include "FixedPoint.h"
#include "Log.h"

#include <string>

namespace Outpost
{

LocalHost::LocalHost(Sim& _sim, Neuron::LoopbackTransport& _network, std::uint64_t _contentHash)
  : m_sim(&_sim),
    m_network(&_network),
    m_host(_sim, _network.Host(), _contentHash, _sim.Tick()),
    m_pacer(TICK_DURATION, MAX_TICKS_PER_PASS, MAX_DEBT_TICKS)
{
  m_tick.store(_sim.Tick(), std::memory_order_relaxed);
}

LocalHost::~LocalHost()
{
  Stop();
}

void LocalHost::Start()
{
  if (m_thread.joinable())
  {
    return;
  }
  m_stop.store(false, std::memory_order_relaxed);
  m_thread = std::thread([this] { Loop(); });
}

void LocalHost::Stop() noexcept
{
  m_stop.store(true, std::memory_order_relaxed);
  if (m_thread.joinable())
  {
    m_thread.join();
  }
}

void LocalHost::Loop()
{
  // WALL TIME IS READ HERE AND NOWHERE ELSE. steady_clock rather than system_clock: a match must not
  // run backwards because somebody's clock synchronised, and a host that ran a thousand ticks in one
  // pass because the wall clock jumped an hour is a host that hangs.
  std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
  while (!m_stop.load(std::memory_order_relaxed))
  {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const std::chrono::nanoseconds elapsed = now - last;
    last = now;
    Pass(elapsed);
    if (m_pacer.Owed() < TICK_DURATION)
    {
      // Nothing owed: give the core back rather than spin. The sleep is shorter than a tick, so the
      // next one is never late by more than a fraction of itself.
      std::this_thread::sleep_for(IDLE_PASS);
    }
  }
}

void LocalHost::Pass(std::chrono::nanoseconds _elapsed)
{
  // 1. Drain the network: orders in, acks in (§3's step 1). The host's Poll both delivers what has
  //    arrived and sends what the last pass queued, so it comes before the ticks rather than after.
  m_network->Host().Poll();

  // 2. While the match clock trails wall time by a tick, run one (§3's step 2). The arithmetic is
  //    Core's TickPacer, which is where the rule it serves can be tested: the match SLOWS when it
  //    falls behind and never skips a tick, because a skipped tick would be a different match.
  // A FINISHED SIM AND A PAUSED ONE ARE THE SAME SHAPE HERE. A finished Sim advances nothing
  // (Sim::Advance returns on m_finished) and a paused one is asked to advance nothing, so in both
  // cases counting the debt would report a host falling further and further behind for as long as
  // the window stayed open - and step 3 below still runs, which is what keeps the client heard
  // from while the world is stopped.
  const bool frozen = m_sim->Finished() || m_paused.load(std::memory_order_relaxed);
  if (frozen)
  {
    m_pacer.Forget();
  }
  const Neuron::TickStep step = frozen ? Neuron::TickStep{} : m_pacer.Take(_elapsed);
  for (std::uint32_t index = 0; index < step.ticks; ++index)
  {
    m_sim->Advance();
    // 2b. WHAT THE TICK DID, TURNED INTO WHAT THE WIRE CARRIES (m1-vertical-slice/C9). GameLogic/Host.h
    //     says of its event list that "the host loop fills this from what the tick did", and until
    //     now no host loop filled it: every one of the seven event kinds was a wire feature with no
    //     producer, and a client could not be told that anything had been fired.
    //
    //     INSIDE THE LOOP AND NOT AFTER IT, because Sim::Shots() is one tick's scratch and the next
    //     Advance empties it. A pass that owes four ticks would otherwise report the fourth only.
    RecordShots();
  }
  if (step.ticks > 0)
  {
    m_tick.store(m_sim->Tick(), std::memory_order_relaxed);
  }
  if (step.givenUp > 0)
  {
    m_slowedTicks.fetch_add(step.givenUp, std::memory_order_relaxed);
    Neuron::Log::Write(Neuron::LogLevel::Warning,
                       "host: the match clock gave up " + std::to_string(step.givenUp) + " tick(s) of wall time; it is running slow");
  }
  m_behindTicks.store(step.behind, std::memory_order_relaxed);

  // 3 and 4. Publish to each client on its own schedule, and the heartbeats and timeouts with it -
  //    Host::Advance is all of that, and it is the half of the loop that knows the protocol.
  m_host.Advance(m_sim->Tick());
}

/// Turns the tick's SimShots into the events the next publish carries.
///
/// THE TRANSLATION IS THE HOST LOOP'S AND NOT THE SIMULATION'S. Sim is below Net (ADR-001), so it
/// records what happened in its own terms and never names an Event; Net decides who may see one and
/// puts it on the wire. This is the one place the two meet, which is why it is four lines rather
/// than a system.
///
/// WHAT A Shot EVENT CARRIES. The source is the shooter, which is what the interest filter tests
/// and what the client composes a muzzle from; the target is what it was aimed at, or nothing when
/// a shell was sent at ground; the point is where it was aimed, quantized to the wire; and the tick
/// is the one the trigger was pulled on, so the client places the shot inside the interval the
/// frame covers rather than at the frame's own tick.
void LocalHost::RecordShots()
{
  std::vector<Event>& events = m_host.Events();
  for (const SimShot& shot : m_sim->Shots())
  {
    Event event{};
    event.kind = EventKind::Shot;
    event.source = shot.shooter.value;
    event.sourceKind = shot.shooter.kind;
    event.target = shot.target.Valid() ? shot.target.value : 0;
    event.targetKind = shot.target.kind;
    event.x = WireFromSubunits(shot.x);
    event.y = WireFromSubunits(shot.y);
    event.z = WireFromSubunits(shot.z);
    event.tick = shot.tick;
    events.push_back(event);
  }
}

} // namespace Outpost
