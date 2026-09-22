#pragma once

#include "Camera.h"
#include "JoinState.h"
#include "OrderMarker.h"
#include "ReplicaStore.h"
#include "TapOrder.h"

#include "NeuronClient.h"

#include <cstdint>

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
/// **NOTHING HERE SIMULATES** (R19). It folds arriving snapshots into the replica store, moves the
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
    /// Snapshots folded into the store.
    std::uint32_t accepted = 0;
    /// Well-formed snapshots the store refused -- out of order, or already held.
    std::uint32_t refused = 0;
    /// Datagrams that did not decode. A counter and not a log: one is ordinary on a wireless link
    /// and a rising rate is the thing worth seeing, which is the same argument `PacketQueue` makes
    /// about its own drops.
    std::uint32_t faulted = 0;
    /// Markers cleared because a snapshot acknowledged the command that made them.
    std::uint32_t markersCleared = 0;

    /// Join replies folded in (ADR-013). **More than one per drain is ordinary**: the host answers
    /// every retry, so a client that asked three times before the first answer arrived gets three.
    std::uint32_t joinReplies = 0;

    /// True when a reply moved the session token and it is worth writing to `LocalState`. The
    /// caller does the writing; nothing below this class touches a file.
    bool tokenChanged = false;
  };

  /// Takes everything waiting on the queue and folds it in, stamping each with the arrival time.
  ///
  /// ONE DRAIN PER FRAME AND IT TAKES EVERYTHING. A queue left with a datagram on it is a frame of
  /// latency added for nothing, and ADR-003 makes the older of two snapshots worthless the moment
  /// the newer one is in hand -- so there is no reason to pace this.
  ///
  /// _nowMilliseconds is the frame's own clock, not a per-datagram one. Everything drained in one
  /// frame arrived before that frame, and splitting hairs finer than a frame would be inventing
  /// precision the queue does not carry.
  DrainResult DrainPackets(Neuron::PacketQueue& _queue, std::uint64_t _nowMilliseconds) noexcept;

  /// Where the frame is drawn from: the pair straddling the render clock, and the fraction between
  /// them (`TechnicalDesign.md` section 6, ADR-003).
  [[nodiscard]] ReplicaStore::Frame Advance(std::uint64_t _nowMilliseconds) const noexcept;

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

  /// **ONE PAST WHAT THE HOST HAS ALREADY APPLIED, ADOPTED FROM THE FIRST SNAPSHOT THAT CARRIES
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

  /// Adopted once. After that the client owns its own counter, and a snapshot that has not yet
  /// caught up with the orders in flight must not wind it backwards.
  bool m_adoptedSequence = false;

  /// Sized by what one datagram can be. ADR-003's MVP snapshot is 1,137 bytes and the MTU is what
  /// bounds the rest, so this is the buffer a drain hands the queue.
  static constexpr std::size_t DATAGRAM_BUFFER_BYTES = 1500;
};

} // namespace Outpost
