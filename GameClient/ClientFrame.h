#pragma once

#include "Camera.h"
#include "FieldView.h"
#include "JoinState.h"
#include "OrderMarker.h"
#include "ReplicaStore.h"
#include "TapOrder.h"

#include "NeuronClient.h"

#include <cstdint>
#include <vector>

namespace Outpost
{

/// The frame, in the order `TechnicalDesign.md` section 6 states it, minus the two ends that belong
/// to the package.
///
/// **R20 IS WHY THIS CLASS EXISTS AT ALL.** `OutpostCommander` holds Windows Runtime glue and
/// nothing else -- so the dispatcher drain stays up there, where a `CoreWindow` is, and everything
/// after it comes down here where a suite can reach it. What the package's `Run` does is: drain the
/// dispatcher, call `DrainPackets`, call `Advance`, render, present. The middle two are this class
/// and they are the two a test can drive.
///
/// **NOTHING HERE SIMULATES** (R19). It folds arriving updates into the replica store, moves the
/// interpolation clock, and clears the markers the host has acknowledged. No entity's position is
/// written by anything in this file.
class ClientFrame
{
public:
  /// What one drain did, for the caller to count rather than recompute.
  ///
  /// R8: a public aggregate.
  struct DrainResult
  {
    /// Datagrams taken off the queue, whatever became of them.
    std::uint32_t datagrams = 0;
    /// Updates folded into the store (ADR-024). Up to two a tick.
    std::uint32_t accepted = 0;
    /// Records in those updates that the store refused because they were no newer than what the entity
    /// already held -- reordered, or repeated.
    std::uint32_t refused = 0;
    /// The refresh interval per entity across those updates, as `ReplicaStore::AcceptResult` counts it:
    /// what the stress harness reports (ADR-022) and nothing in the frame reads.
    std::uint32_t refreshed = 0;
    std::uint64_t refreshTicksTotal = 0;
    std::uint32_t refreshTicksMax = 0;
    /// Datagrams that did not decode. A counter and not a log: one is ordinary on a wireless link
    /// and a rising rate is the thing worth seeing, which is the same argument `PacketQueue` makes
    /// about its own drops.
    std::uint32_t faulted = 0;
    /// Markers cleared because an update acknowledged the command that made them.
    std::uint32_t markersCleared = 0;

    /// Outstanding commands the host acknowledged in this drain, and ones given up on because they went
    /// unacknowledged for `COMMAND_RESEND_WINDOW_MILLISECONDS` -- the second a count worth logging, since
    /// the host acknowledges even a refusal and one that never was means a link that is losing everything.
    std::uint32_t commandsRetired = 0;
    std::uint32_t commandsExpired = 0;

    /// Join replies folded in (ADR-013). **More than one per drain is ordinary**: the host answers
    /// every retry, so a client that asked three times before the first answer arrived gets three.
    std::uint32_t joinReplies = 0;

    /// True when a reply moved the session token and it is worth writing to `LocalState`. The
    /// caller does the writing; nothing below this class touches a file.
    bool tokenChanged = false;

    /// This drain found the link silent and started a rejoin. Once per loss, for the log.
    bool linkLost = false;

    /// This drain landed the first update after a rejoin, which is what takes the overlay down.
    bool linkRestored = false;
  };

  /// **HOW LONG A SEATED CLIENT HEARS NOTHING BEFORE IT CALLS THE LINK LOST: ONE SECOND.**
  ///
  /// Twenty ticks at 20 Hz, against ADR-003's 75-millisecond buffer that already covers a lost one or
  /// two without the player seeing anything. A burst that long is not jitter, and it is far shorter than
  /// any suspension -- a packaged client loses the foreground for seconds at least, and `Interface.md`
  /// section 7's resume is detected by exactly this: the frame clock jumps and the last arrival is old.
  /// **Not tuned.** Too short and a bad wireless second throws an overlay over a match that is still
  /// arriving; too long and a player taps into a dead link. M1.16 is where it gets looked at, with the
  /// other constants.
  static constexpr std::uint64_t LINK_SILENCE_MILLISECONDS = 1000;
  static_assert(LINK_SILENCE_MILLISECONDS == std::uint64_t{ReplicaStore::FORGET_FLOOR_TICKS} * SNAPSHOT_INTERVAL_MILLISECONDS,
                "the replica store must not forget an entity inside the silence the link survives (the 2026-09-23 review, m2)");

  /// Takes everything waiting on the queue and folds it in, stamping each with the arrival time.
  ///
  /// ONE DRAIN PER FRAME AND IT TAKES EVERYTHING. A queue left with a datagram on it is a frame of
  /// latency added for nothing, and every update is self-contained -- so there is no reason to pace
  /// this.
  ///
  /// _nowMilliseconds is the frame's own clock, not a per-datagram one. Everything drained in one
  /// frame arrived before that frame, and splitting hairs finer than a frame would be inventing
  /// precision the queue does not carry.
  DrainResult DrainPackets(Neuron::PacketQueue& _queue, std::uint64_t _nowMilliseconds) noexcept;

  /// Every held entity at the render clock, each interpolated between its own two samples or held at its
  /// newest (`TechnicalDesign.md` section 6, ADR-024). _outRecords is cleared and refilled.
  DrawnSummary Advance(std::uint64_t _nowMilliseconds, std::vector<EntityRecord>& _outRecords) const;

  /// **HOW OFTEN A SEATED CLIENT TELLS THE HOST WHAT IT IS LOOKING AT, WHEN IT HAS NOTHING TO ORDER.**
  ///
  /// ADR-024's accumulator scores relevance against the view, and the view rides the command packet's
  /// header -- but commands go out only when the player taps, so without this a host would score a client
  /// that pans without ordering against where it looked when it last gave an order. An empty command
  /// packet is the report. **Four a second**, which is not tuned: it is a quarter of a second of lag
  /// between a pan and the accumulator favoring what the pan revealed, at twelve bytes a report, and the sweep
  /// covers the gap in any case.
  static constexpr std::uint64_t VIEW_REPORT_INTERVAL_MILLISECONDS = 250;

  /// True when a view report should go out now, and it takes the send as having happened -- **so a caller
  /// that ignores the answer has stopped reporting.** Never before the join is answered, since the host
  /// refuses a packet from an endpoint it has not seated.
  [[nodiscard]] bool ShouldReportView(std::uint64_t _nowMilliseconds) noexcept;

  /// Fills a command packet's view from the camera: the focus on the wire's grid, and the camera distance
  /// as the radius in whole world units. **The distance is a deliberately generous radius** -- the frame
  /// spans about 1.1 times the distance across, and pitch shows more beyond the focus than before it -- so
  /// what is on screen is in view, and a little of what is about to be.
  void StampView(CommandPacket& _packet) const noexcept;

  /// **HOW LONG AN UNACKNOWLEDGED COMMAND IS RESENT: TWO SECONDS, FORTY TICKS** (the 2026-09-23 review, M5).
  /// The host acknowledges every command it has decided on, refusals included (Q24 as amended), so one still
  /// unacknowledged after two seconds is on a link that is losing everything -- and resending it forever would
  /// carry it into a later rejoin and keep its marker on screen. Twice the link-loss second, so a rejoin that
  /// succeeds still delivers what was tapped during it. **Not tuned.**
  static constexpr std::uint64_t COMMAND_RESEND_WINDOW_MILLISECONDS = 2000;

  /// **ADR-003'S RELIABILITY, FOR A PERSON** (the 2026-09-23 review, M5). `TechnicalDesign.md` section 4 has
  /// every command repeated in every outgoing packet until the host acknowledges it; until this, only the
  /// Bot did, and the packaged client sent each command once -- so a lost datagram was a lost order and its
  /// marker stayed until some later order was acknowledged. A command issued here is held until an update's
  /// own block acknowledges its sequence, or until the resend window passes.
  void IssueCommand(Command _command, std::uint64_t _nowMilliseconds);

  /// Every outstanding command, oldest first, packed into _packet by `FillOldestFirst` -- **into every
  /// packet this client sends**, the view report included, so a lost command goes again within a quarter of a
  /// second whether or not the player taps. Replaces whatever commands _packet held; returns how many it packed.
  std::size_t FillOutstanding(CommandPacket& _packet) const;

  /// Oldest first.
  [[nodiscard]] const std::vector<Command>& OutstandingCommands() const noexcept
  {
    return m_outstanding;
  }

  /// The sequence the next command should carry. Post-incremented and allowed to wrap, which is
  /// what the host's serial-number comparison at intake expects (`GameCore/Command.h`, Q24).
  [[nodiscard]] std::uint16_t TakeCommandSequence() noexcept
  {
    return m_nextCommandSequence++;
  }

  [[nodiscard]] CameraPose& Camera() noexcept
  {
    return m_camera;
  }

  [[nodiscard]] const CameraPose& Camera() const noexcept
  {
    return m_camera;
  }

  [[nodiscard]] OrderMarkerSet& Markers() noexcept
  {
    return m_markers;
  }

  [[nodiscard]] const OrderMarkerSet& Markers() const noexcept
  {
    return m_markers;
  }

  [[nodiscard]] const ReplicaStore& Replicas() const noexcept
  {
    return m_replicas;
  }

  /// Which player this client is, **as the host said** (ADR-013). `NO_PLAYER` until the join is
  /// answered, which is what the marker clearing and the command path both check.
  [[nodiscard]] PlayerId Player() const noexcept
  {
    return m_join.Player();
  }

  /// **`MutableJoin` AND NOT `Join`**, matching `Host::MutableWorld`. A member function named
  /// `Join` would hide the type `Outpost::Join` inside this class, which is the sort of thing that
  /// compiles until the day somebody declares one here.
  [[nodiscard]] JoinState& MutableJoin() noexcept
  {
    return m_join;
  }

  [[nodiscard]] const JoinState& CurrentJoin() const noexcept
  {
    return m_join;
  }

  /// The asteroid field (M2.3), derived from the join's seed and count whenever a reply moves either --
  /// so it exists from the drain that seats this client and is cleared by one that refuses it. Nothing
  /// from an update touches it.
  [[nodiscard]] const FieldView& Field() const noexcept
  {
    return m_field;
  }

  /// What the system panel shows. **The one place the link state is decided** (R20) -- the package used
  /// to map the join phase itself, and it could not see a loss because the loss is a fact about arrivals.
  [[nodiscard]] LinkState Link() const noexcept;

private:
  ReplicaStore m_replicas;
  OrderMarkerSet m_markers;

  /// **THE OPENING VIEW: YOUR OWN BASE, CLOSE ENOUGH TO READ.**
  ///
  /// The focus is the origin and is replaced the moment the join answers -- `App.cpp` recenters on
  /// this player's station, which it could not do before ADR-013 because nothing knew which player
  /// this was. What this constant actually decides is the DISTANCE, because a recenter moves the
  /// focus and leaves the zoom alone (ADR-018).
  ///
  /// **2,400 UNITS, AND IT IS THE STATION THAT SETS IT.** At a 40-degree field of view the frame
  /// spans `1.092 x distance` across, so 2,400 shows about 2,620 units and a 220-unit station is
  /// roughly **120 authored pixels** -- readable, with its 400-unit module radius on screen around
  /// it. M0 opened at 4,000, where the same station is 72 pixels and reads as a smudge; the far end
  /// of the range, 22,500, puts it at 13.
  ///
  /// **IT IS DELIBERATELY NOT THE NEAR END.** 1,400 would be larger still and pinned against the
  /// zoom limit, with nowhere to go but out -- and because pitch is coupled to zoom
  /// (`Interface.md` section 5), it would also be the most raking view the camera has.
  CameraPose m_camera{.focusX = 0.0f, .focusY = 0.0f, .headingRadians = 0.0f, .distance = 2400.0f};

  /// **M1.4 REPLACED A COMPILED-IN PLAYER ONE WITH THIS.** Until ADR-013 the protocol had no way
  /// to tell a client which player it was, so the client assumed -- and `CommandIntake` refuses a
  /// packet from `NO_PLAYER` outright, so the assumption could not be `NO_PLAYER` and could not be
  /// checked either. Now it is answered, and until it is answered this client sends nothing.
  JoinState m_join;

  /// Follows `m_join`: rederived after a reply is folded in, and a no-op when the reply repeats the pair.
  FieldView m_field;

  /// **ONE PAST WHAT THE HOST HAS ALREADY APPLIED, ADOPTED FROM THE FIRST UPDATE THAT CARRIES
  /// THIS PLAYER'S BLOCK.** It starts at one and is corrected the moment the host says otherwise.
  ///
  /// A COUNTER THAT ALWAYS STARTS AT ONE MAKES A RECONNECTING CLIENT MUTE. `CommandIntake` refuses
  /// a command whose sequence is not NEWER than the last it applied for that player (Q24), and it
  /// remembers that for the life of the match -- so a client that relaunches and starts again at
  /// one has every order discarded until it counts back past where the last session finished. On
  /// the device that was thirty-eight seconds of tapping a ship that would not move.
  ///
  /// **ADR-003 SAYS RESUME COSTS NOTHING AND THAT IS TRUE OF STATE, NOT OF COMMANDS.** A
  /// self-contained snapshot restores everything the client DRAWS; nothing restored what it is
  /// allowed to SAY. `lastCommandSequenceApplied` has been in every snapshot since M0.9 for exactly
  /// this, and until now nothing read it.
  std::uint16_t m_nextCommandSequence = 1;

  /// Adopted once. After that the client owns its own counter, and an update that has not yet
  /// caught up with the orders in flight must not wind it backwards.
  bool m_adoptedSequence = false;

  /// **WHEN THE LAST UPDATE WAS FOLDED IN, OR WHEN THE LAST REJOIN BEGAN**, whichever is later. The
  /// second is what stops a rejoin that is answered but never followed by an update from starting
  /// another one every frame: the silence is measured again from the rejoin, so it asks once a second.
  std::uint64_t m_lastHeardMilliseconds = 0;

  /// A client that has never had an update has no link to lose. Without this the first second of a
  /// join to a host that seats but has not yet sent would read as a loss.
  bool m_heardUpdate = false;

  /// When the last view report went out, and whether one has. Zero is a legal clock reading, so the flag
  /// is what says whether the time means anything -- `JoinState` makes the same choice.
  std::uint64_t m_lastViewReportMilliseconds = 0;
  bool m_reportedView = false;

  /// Set when a silence starts a rejoin, cleared by the first update after the host seats this client
  /// again.
  bool m_reconnecting = false;

  /// Commands the host has not yet acknowledged, oldest first, and when each was issued (`IssueCommand`).
  std::vector<Command> m_outstanding;
  std::vector<std::uint64_t> m_issuedMilliseconds;

  /// Sized by what one datagram can be. An update is at most 1,232 bytes (ADR-024) and the MTU is what
  /// bounds the rest, so this is the buffer a drain hands the queue.
  static constexpr std::size_t DATAGRAM_BUFFER_BYTES = 1500;
};

} // namespace Outpost
