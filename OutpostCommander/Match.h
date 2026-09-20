#pragma once

#include "LocalHost.h"

#include "Client.h"
#include "Deposit.h"
#include "ContentTree.h"
#include "ModelComposer.h"
#include "Picking.h"
#include "Replica.h"
#include "RenderViewBuilder.h"

#include "Liveness.h"
#include "RenderView.h"
#include "TickPacer.h"

#include <chrono>
#include <cstdint>
#include <span>
#include <string_view>

// One match as the game holds it (TechnicalDesign.md §3; m1-vertical-slice/G1a): the host thread on
// one side, this commander's client and replica on the other, and the loopback between them.
//
// IT IS BOTH LOOPS' MEETING POINT AND NEITHER LOOP'S. The host loop is LocalHost's, on its own
// thread. Of the client loop's seven steps this object runs 2 to 5 - drain the network, apply
// frames, interpolate, build the render view - and App runs 1, 6 and 7, the window's messages, the
// input routing and the drawing, because those need a window and a device and these do not.
//
// WHICH MEANS IT BUILDS AND RUNS OUTSIDE CI. Sim, Net, Replica, Content and Core, and not one
// Windows or Direct3D header. G1a's notes say the executable is CI's alone and that is true of App;
// it is not true of the two files that hold the match itself, and keeping it that way is worth more
// than the convenience of putting a device in here.
//
// THE CLIENT HAS NO SIMULATION CLOCK, AND STILL NEEDS A CLOCK. §3 is explicit that the client "has
// no simulation clock; it has the replica's timeline, which is the host's tick numbers arriving
// late" - and Net's Client takes a tick, for heartbeats and timeouts (NeuronCore/Liveness.h). That is a
// LIVENESS clock and not a simulation one, and it has to come from wall time rather than from the
// replica: a host that stopped sending stops advancing the replica's tick, which is exactly the
// moment a timeout has to fire. Nothing here advances the world; the world is the host's.

namespace Outpost
{

/// The landscape a match of M1 is played on, until M2's lobby chooses one.
inline constexpr const char* SLICE_LANDSCAPE = "Slice";

/// Who runs the host's loop. THREADED IS THE GAME: the host keeps wall time on a thread of its own
/// and the client draws whatever has arrived, which is the arrangement TechnicalDesign.md §3 asks
/// for and the reason single-player exercises the network path. STEPPED IS THE CAPTURE'S: no thread
/// at all, and the caller runs exactly the ticks it asks for as fast as the machine will run them,
/// so that a capture is a fixed number of TICKS rather than however many a WARP frame took
/// (m1-vertical-slice/G2). A capture paced by wall time is a different match on every run, which is
/// no use at all for frames somebody is meant to compare against last week's.
enum class HostPacing : std::uint8_t
{
  Threaded,
  Stepped
};

/// Heartbeats and timeouts for a loopback match. Generous, because the "network" here cannot lose a
/// datagram and a client that timed out against a host in its own process would be reporting that
/// the machine had stalled - which is what LocalHost's SlowedTicks() is for saying properly.
inline constexpr Neuron::LivenessSettings LOCAL_LIVENESS{20, 200, 400};

class Match
{
public:
  /// _content outlives the match; Sim holds a reference to it. _settings is the lobby's, which in
  /// M1 is the fixed pair of seats G1a's acceptance names. _chunkCells is NeuronClient/TerrainChunk.h's
  /// CHUNK_CELLS, PASSED IN rather than included: it is a rendering decision, and reaching for it
  /// here would drag Direct3D into the one half of this executable that does not need it.
  Match(const ContentTree& _content, const MatchSettings& _settings, std::uint64_t _contentHash, std::uint32_t _chunkCells);
  ~Match();
  Match(const Match&) = delete;
  Match& operator=(const Match&) = delete;

  /// Creates the landscape, starts the host thread and sends the join. False, with the reason
  /// logged, when the landscape is refused - which is a content fault and belongs on the way up
  /// rather than on a thread nobody is watching.
  ///
  /// _observeSeat asks to WATCH a seat rather than to play one (GameShared/Messages.h's Join; the owner's
  /// ruling of 2026-09-19 at m1-vertical-slice/G2). That is how a match of two scripted commanders
  /// comes to have a client at all: FreeSeat hands a joining client only a seat whose kind is
  /// Human, so an all-AI lobby refuses an ordinary join with NoSeat and leaves nothing to draw.
  /// _pacing decides whether the host gets a thread; a Stepped match is advanced by StepHost.
  [[nodiscard]] bool Start(const LandscapeDefinition& _landscape, std::string_view _commanderName,
                           std::uint8_t _observeSeat = NO_OBSERVED_SEAT, HostPacing _pacing = HostPacing::Threaded);

  /// One tick of the host, for a Stepped match: the pass its thread would have run, handed exactly
  /// a tick's worth of time so that it runs exactly one (NeuronServer/TickPacer.h says so in as many
  /// words). Does nothing on a Threaded match, whose host is already running its own loop.
  void StepHost();

  /// Stops the host thread and joins it. Called by the destructor.
  void Stop() noexcept;

  /// Pauses the world and not the connection (LocalHost::Pause says why the two are not the same).
  /// M1 may do this at all because the host is a thread in this process; Design/Interface.md §10
  /// records it as the thing that has to change before a match has a second human in it.
  void Pause(bool _paused) noexcept
  {
    m_host.Pause(_paused);
  }

  [[nodiscard]] bool Paused() const noexcept
  {
    return m_host.Paused();
  }

  /// Steps 2 to 5 of the client loop, once. _elapsed is the wall time since the last call, which
  /// drives the liveness clock and nothing else.
  void Advance(std::chrono::nanoseconds _elapsed);

  [[nodiscard]] bool Playing() const noexcept
  {
    return m_client.State() == ClientState::Playing;
  }

  /// Queues one of this commander's orders. False when the unacknowledged window is full.
  [[nodiscard]] bool Submit(const Order& _order)
  {
    return m_client.Submit(_order);
  }

  /// What to draw, rebuilt by every Advance.
  [[nodiscard]] const Neuron::RenderView& View() const noexcept
  {
    return m_view;
  }

  /// What a click can land on, rebuilt by every Advance.
  [[nodiscard]] const PickSet& Picks() const noexcept
  {
    return m_picks;
  }

  [[nodiscard]] const Replica& Commander() const noexcept
  {
    return m_replica;
  }

  /// THE LOBBY THIS PROCESS CHOSE, and not something the wire carried. Design/Interface.md §7.5
  /// asks the Match tab to show what the lobby fixed "because M1 has no lobby and the defaults are
  /// otherwise invisible", and nothing in Net replicates MatchSettings: a joining client is sent
  /// its seat and the landscape's definition and no lobby at all (GameShared/Messages.h's JoinAccepted).
  ///
  /// THAT IS HONEST ONLY WHILE THE MATCH IS LOCAL. Here the same process both chose these values
  /// and is reading them back, so the panel is showing the commander his own lobby rather than
  /// telling him something the host knows and he does not. The moment a client joins a host over
  /// UDP this accessor answers whatever THAT process was constructed with, which is nothing at all
  /// - so the tab is a local-match tab until the settings are on the wire, which is what
  /// Design/Interface.md §11 row 17 owns.
  [[nodiscard]] const MatchSettings& Settings() const noexcept
  {
    return m_settings;
  }

  /// THE LANDSCAPE THIS COMMANDER GENERATES, and not the host's. It is built from the definition
  /// the host sent with the join, so it carries no flatten deltas at all: the host's own landscape
  /// is flattened under every structure in the match, including the bases this commander has never
  /// scouted, and TechnicalDesign.md §5.2 is explicit that "the terrain under an unscouted base is
  /// not public". A local match is where that boundary is easiest to lose - the host's Landscape is
  /// one member away - and losing it here would be a wallhack that shipped.
  ///
  /// Empty until the join is accepted, because the definition arrives with it. TerrainReady() says.
  [[nodiscard]] const Landscape& Terrain() const noexcept
  {
    return m_terrain;
  }

  [[nodiscard]] bool TerrainReady() const noexcept
  {
    return m_terrain.Created();
  }

  /// THE DEPOSITS THIS COMMANDER KNOWS OF, built from the definition his join carried and not from
  /// the host's economy. GameClient/PlacementPreview.h wants it for the one rule that is about WHAT is
  /// being built - an extractor stands on a deposit and nothing else does - and a client that had
  /// none would draw every extractor ghost legal and be refused by the host every time.
  [[nodiscard]] const DepositField& Deposits() const noexcept
  {
    return m_deposits;
  }

  [[nodiscard]] const LocalHost& HostSide() const noexcept
  {
    return m_host;
  }

  /// The ids the commander has selected. Held here because the render view's selection flag and
  /// picking's answer have to agree, and one owner is how they do.
  void Select(std::span<const std::uint32_t> _ids);
  [[nodiscard]] std::span<const std::uint32_t> Selected() const noexcept
  {
    return m_selected;
  }

  /// The liveness tick this client is on. Not the simulation's, and not the replica's.
  [[nodiscard]] std::uint32_t LivenessTick() const noexcept
  {
    return m_livenessTick;
  }

private:
  const ContentTree* m_content;
  MatchSettings m_settings;
  Sim m_sim;
  Neuron::LoopbackTransport m_network;
  LocalHost m_host;
  Neuron::Transport& m_clientEnd;
  Client m_client;
  Replica m_replica;
  ModelComposer m_composer;
  RenderViewBuilder m_builder;
  /// The client's own, generated from the definition the join carried. Never the host's.
  Landscape m_terrain;
  DepositField m_deposits;
  Neuron::RenderView m_view;
  PickSet m_picks;
  std::vector<std::uint32_t> m_selected;
  /// The liveness clock: wall time into ticks, for heartbeats and timeouts. Core's pacer, so that
  /// the one place wall time becomes a tick count is one tested piece of arithmetic wherever it is
  /// used - here it is allowed to run every tick it owes, because a liveness counter that ran four
  /// at a time would make a timeout late by however long the frame hitched.
  Neuron::TickPacer m_liveness;
  std::uint32_t m_livenessTick = 0;
  std::uint64_t m_contentHash;
  std::uint32_t m_chunkCells;
};

} // namespace Outpost
