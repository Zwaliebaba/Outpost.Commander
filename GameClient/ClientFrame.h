#pragma once

#include "Camera.h"
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

  /// Which player this client is. **There is no join yet** -- `README.md` F2 records that the
  /// protocol has no way to tell a client which player it is, and M1.4 is where that arrives. Until
  /// then it is configuration, defaulted here, and the marker clearing below reads the block it
  /// names.
  [[nodiscard]] PlayerId& Player() noexcept
  {
    return m_player;
  }

private:
  ReplicaStore m_replicas;
  OrderMarkerSet m_markers;

  /// **M0'S STARTING VIEW, AND IT IS A CONVENIENCE RATHER THAN A DESIGN DECISION.** The pose a
  /// client opens on is `Interface.md` section 5's to settle and M1.8 pins the zoom range; this is
  /// the one that makes M0 checkable by eye.
  ///
  /// A default-constructed pose is the far end of the zoom -- 22,500 units, where the whole
  /// 16,384-unit square fits and a 30-unit ship is under two authored pixels. That is arithmetically
  /// right and useless for looking at: **the arrow exists so that a wrong heading is visible**, and
  /// nothing about a heading survives at two pixels. At 4,000 units the frame spans about 4,370
  /// units across, so the same ship is roughly ten pixels and points somewhere.
  ///
  /// The focus sits at the middle of the path M0's host walks its one entity along -- origin to
  /// (4096, 2048) -- so the thing there is to see is on screen without anybody panning first.
  CameraPose m_camera{.focusX = 2048.0f, .focusY = 1024.0f, .headingRadians = 0.0f, .distance = 4000.0f};

  /// **PLAYER ONE, AND ZERO WOULD BE `NO_PLAYER`** (`GameCore/Entity.h`). `CommandIntake` refuses a
  /// packet from `NO_PLAYER` outright, before it looks at what the order touches, so a client that
  /// defaults to zero has every order it ever sends discarded in silence -- which is exactly what
  /// happened on the device the first time the tap was wired.
  ///
  /// **IT IS CONFIGURATION BECAUSE `README.md` F2 SAYS IT HAS TO BE**: the protocol has no join, so
  /// a client cannot learn which player it is, and M1.4 is where that arrives. Until then the host
  /// seeds player one and the client is told so here.
  PlayerId m_player = 1;
  std::uint16_t m_nextCommandSequence = 1;

  /// Sized by what one datagram can be. ADR-003's MVP snapshot is 1,137 bytes and the MTU is what
  /// bounds the rest, so this is the buffer a drain hands the queue.
  static constexpr std::size_t DATAGRAM_BUFFER_BYTES = 1500;
};

} // namespace Outpost
