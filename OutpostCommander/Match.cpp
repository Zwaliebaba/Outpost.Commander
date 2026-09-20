#include "pch.h"

#include "Match.h"

#include "StartingBase.h"

#include "Placement.h"

#include "FixedPoint.h"
#include "Log.h"

#include <algorithm>
#include <string>

namespace Outpost
{

namespace
{

/// A liveness counter runs every tick it owes: one that ran four at a time would make a timeout
/// late by however long the frame hitched, which is the opposite of what a timeout is for. The
/// ceiling is high for the same reason - the debt is the measure, not something to write off.
inline constexpr std::uint32_t LIVENESS_MAX_PER_PASS = 1000;
inline constexpr std::uint32_t LIVENESS_MAX_DEBT = 1000;

} // namespace

Match::Match(const ContentTree& _content, const MatchSettings& _settings, std::uint64_t _contentHash, std::uint32_t _chunkCells)
  : m_content(&_content),
    m_settings(_settings),
    m_sim(_settings, _content),
    m_network(0),
    m_host(m_sim, m_network, _contentHash),
    m_clientEnd(m_network.Connect()),
    m_client(m_clientEnd, LOCAL_LIVENESS, 0),
    m_replica(m_client),
    m_composer(_content),
    m_builder(_content, m_composer, RenderViewSettings{}),
    m_liveness(TICK_DURATION, LIVENESS_MAX_PER_PASS, LIVENESS_MAX_DEBT),
    m_contentHash(_contentHash),
    m_chunkCells(_chunkCells)
{
  // THE SEED IS ZERO AND THE FAULTS ARE OFF. A loopback with no faults set drops nothing, so the
  // seed never draws; it is named rather than left to a default so that nobody later reads a
  // dropped datagram in single-player as a network problem.
}

Match::~Match()
{
  Stop();
}

bool Match::Start(const LandscapeDefinition& _landscape, std::string_view _commanderName, std::uint8_t _observeSeat, HostPacing _pacing)
{
  if (!m_sim.CreateLandscape(_landscape))
  {
    Neuron::Log::Write(Neuron::LogLevel::Error, "match: the landscape was refused; there is no match to play");
    return false;
  }
  // The builder needs the landscape's size before it can turn a footprint into a chunk, and the
  // landscape is only known now - Sim is what validates and holds it.
  RenderViewSettings settings{};
  settings.cellsPerSide = m_sim.Terrain().CellsPerSide();
  settings.chunkCells = m_chunkCells;
  m_builder.Settings(settings);

  // WHAT IS ALREADY STANDING WHEN THE MATCH BEGINS (GameDesign.md §5): "the base level in the
  // lobby decides", and until M2's lobby that is the fixed pair of seats G1a names. It happens
  // HERE, before the host thread exists, because every seat has to be set up before tick 0 - a
  // base placed on a running simulation would be a structure that appeared out of nothing in a
  // frame some commander was already watching.
  //
  // A SEAT THE LANDSCAPE HAS NO START FOR IS REFUSED rather than dropped. Two commanders and one
  // start is a landscape that cannot hold this lobby, and a match that began anyway would put the
  // second commander nowhere and look like the game losing him.
  if (_landscape.starts.size() < m_sim.Settings().seatCount)
  {
    Neuron::Log::Write(Neuron::LogLevel::Error, "match: the landscape names " + std::to_string(_landscape.starts.size()) +
                                                  " start(s) and the lobby seats " + std::to_string(m_sim.Settings().seatCount));
    return false;
  }
  for (std::uint8_t seat = 0; seat < m_sim.Settings().seatCount; ++seat)
  {
    if (m_sim.Settings().seats[seat].kind == SeatKind::Empty)
    {
      continue; // An empty seat is nobody; it gets no base and costs the landscape no start.
    }
    if (!PlaceStartingBase(m_sim, seat, _landscape.starts[seat], m_sim.Settings().baseLevel))
    {
      return false; // PlaceStartingBase has logged which of its faults it was.
    }
  }

  // THE HOST STARTS BEFORE THE JOIN, and it has to: the join is a datagram, and a datagram is
  // answered by a host that is running. Starting the thread last would leave the first join to sit
  // in a queue until the second pass, which works and is a frame of nothing for no reason.
  //
  // A STEPPED MATCH STARTS NO THREAD AT ALL, and the order still holds: its caller runs the pass
  // itself, so the join sits in the loopback's queue for exactly as long as it takes that caller to
  // call StepHost once. What would be wrong is starting a thread here and stepping it too, which is
  // two clocks on one simulation.
  if (_pacing == HostPacing::Threaded)
  {
    m_host.Start();
  }
  m_client.SendJoin(m_contentHash, 1, _commanderName, 0, _observeSeat);
  return true;
}

void Match::StepHost()
{
  if (m_host.Running())
  {
    return; // Its own thread is already doing this; a second clock would run the match twice.
  }
  m_host.Pass(TICK_DURATION);
}

void Match::Stop() noexcept
{
  m_host.Stop();
}

void Match::Select(std::span<const std::uint32_t> _ids)
{
  m_selected.assign(_ids.begin(), _ids.end());
  // Ascending, so that the render view's flags and picking's answers are compared against one
  // order rather than whichever the caller happened to hand over.
  std::sort(m_selected.begin(), m_selected.end());
  m_selected.erase(std::unique(m_selected.begin(), m_selected.end()), m_selected.end());
}

void Match::Advance(std::chrono::nanoseconds _elapsed)
{
  // The liveness clock. Not the simulation's: nothing here advances the world.
  m_livenessTick += m_liveness.Take(_elapsed).ticks;

  // 2. Drain the network: frames in, orders and acks out. One Poll serves this end; the host thread
  //    polls its own (Net/Client.h says the transport is the caller's to poll).
  m_clientEnd.Poll();

  // 3. Apply what arrived to the replica. Client::Advance reads everything that has come in, hands
  //    each frame to the replica, and sends the orders and the acknowledgement that are due.
  m_client.Advance(m_livenessTick, m_replica);

  // The landscape arrives with the join, and is generated HERE rather than taken from the host:
  // §5.2's boundary, and the reason Match holds a Landscape of its own at all. AFTER Advance and
  // not before: Advance is what reads the JoinAccepted, so asking first would leave the terrain a
  // pass behind the moment it became knowable, and App cannot build a terrain pass without it.
  if (!m_terrain.Created() && m_client.State() == ClientState::Playing)
  {
    if (!m_terrain.Create(m_client.Landscape()))
    {
      Neuron::Log::Write(Neuron::LogLevel::Error, "match: the landscape the host sent could not be generated on this client");
    }
    // The same definition the terrain came from, so the footprint ghost knows a deposit when it
    // sees one; the host's own field is built from the same bytes and the two therefore agree.
    m_deposits.Build(m_client.Landscape());
  }

  // 4 and 5. Interpolate a hundred milliseconds behind the newest frame, and build the render view
  //    from that moment. RenderTimeAtNewestFrame is the replica's own timeline - the host's tick
  //    numbers arriving late - and the interpolation delay is already inside it.
  m_builder.Build(m_replica, m_replica.RenderTimeAtNewestFrame(), m_selected, m_view, m_picks);

  // AND THE GROUND UNDER WHAT HE CAN SEE (m1-vertical-slice/K6). The wire carries no height deltas
  // - they would hand a commander the shape of ground he has never scouted (TechnicalDesign.md
  // §5.2) - so this is where the flatten reaches the client at all. It follows the fog and nothing
  // else, because the render view is built from the replica and the replica holds only what this
  // commander can see or remembers seeing: a base he has never scouted levels nothing, and a ghost
  // levels exactly what the structure did when he last saw it.
  //
  // THE SAME ARITHMETIC THE HOST RAN, so the two agree sample for sample: Sim/Placement.h's
  // FlattenDelta for the rectangle of samples a footprint owns, and the height the wire carried
  // rather than a mean recomputed here. Recomputing would be wrong in the one case that matters -
  // a second structure whose window overlaps the first levels ground the host had ALREADY
  // flattened, so its mean is taken over different samples on the two machines.
  const std::uint32_t levelled = LevelUnderFlattens(m_terrain, m_view.flattens);
  if (levelled != m_view.flattens.size())
  {
    Neuron::Log::Write(Neuron::LogLevel::Warning, "match: " + std::to_string(m_view.flattens.size() - levelled) +
                                                    " flatten(s) were refused by this commander's landscape");
  }
}

} // namespace Outpost
