#include "pch.h"

#include "Host.h"

#include "Tick.h"

#include <array>

namespace Outpost
{

namespace
{
/// ADR-003's pinned payload. A snapshot larger than this would fragment, which is the property the
/// whole replication design exists to keep.
inline constexpr std::size_t PAYLOAD_BYTES = 1232;

/// Comfortably larger than any datagram this build sends or accepts, so a receive is never
/// measuring the buffer instead of the packet.
inline constexpr std::size_t SCRATCH_BYTES = 2048;

/// Q27 ships two through M3. It is a runtime value everywhere it matters -- the snapshot sizes its
/// per-player blocks by a count in the header -- so raising it is configuration and not a format
/// change.
inline constexpr std::size_t PLAYER_COUNT = 2;
} // namespace

Snapshot BuildSnapshot(const World& _world, const CommandIntake& _intake, std::uint32_t _tick, std::uint16_t _sequence,
                       std::size_t _playerCount)
{
  Snapshot snapshot;
  snapshot.sequence = _sequence;
  snapshot.tick = _tick;

  for (std::size_t player = 0; player < _playerCount; ++player)
  {
    const PlayerId identity = static_cast<PlayerId>(player + 1);
    snapshot.players.push_back(PlayerBlock{
      .credits = 0, .lastCommandSequenceApplied = _intake.LastAppliedSequence(identity), .buildingDesign = 0, .buildProgressPercent = 0});
  }

  // Index order, which is the order everything else walks the store in.
  const std::size_t slotCount = _world.SlotCount();
  snapshot.entities.reserve(_world.AliveCount());
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }

    const Entity& entity = _world.EntityInSlot(slot);

    // An index the wire cannot name is dropped rather than truncated: a truncated identity does
    // not fail, it names a different entity. The store allows 65,536 slots and the wire ten bits
    // of index, so this is unreachable at the design's counts and is written for the day it is
    // not (see EntityRecord.h).
    if (!FitsWireIdentity(entity.id.index))
    {
      continue;
    }

    snapshot.entities.push_back(
      EntityRecord{.identity = PackIdentity(entity.id.index, entity.id.generation),
                   .positionX = QuantizePosition(entity.position.x),
                   .positionY = QuantizePosition(entity.position.y),
                   .heading = QuantizeWireHeading(entity.heading),
                   // M1.3: the fields M0.9 encoded as zeroes now carry meaning.
                   .hullPercentRemaining = QuantizeHullPercent(entity.hullRemaining, Derive(entity.design).hullPoints),
                   .designIdentity = static_cast<std::uint8_t>(entity.design),
                   .flags = static_cast<std::uint8_t>((entity.owner & FLAGS_TEAM_MASK) << FLAGS_TEAM_SHIFT)});
  }

  // M0 removes nothing and fires nothing. The lists are still encoded, which is what M0.9 proved.
  return snapshot;
}

bool Host::Open(std::uint16_t _port) noexcept
{
  return m_transport.Open(_port);
}

void Host::Close() noexcept
{
  m_transport.Close();
  m_clients.clear();
}

void Host::DrainAndApply()
{
  std::array<std::byte, SCRATCH_BYTES> scratch{};
  for (;;)
  {
    std::size_t byteCount = 0;
    Neuron::Endpoint sender{};
    const Neuron::ReceiveOutcome outcome = m_transport.Receive(scratch, byteCount, sender);
    if (outcome == Neuron::ReceiveOutcome::Empty)
    {
      // The ordinary answer, twenty times a second, for as long as nobody is talking.
      break;
    }
    if (outcome == Neuron::ReceiveOutcome::Oversized)
    {
      // The datagram is gone and the NEXT receive gets the next one, so this is the one outcome
      // that keeps draining.
      ++m_rejectedDatagrams;
      continue;
    }
    if (outcome == Neuron::ReceiveOutcome::Failed)
    {
      // BREAK, NOT CONTINUE. A failed receive does not consume anything, so continuing here spins
      // forever on a socket that is closed or in trouble -- which is exactly what it did the
      // first time this loop ran against a Host whose transport was never opened.
      ++m_rejectedDatagrams;
      break;
    }

    Neuron::ByteReader reader{std::span<const std::byte>{scratch.data(), byteCount}};
    CommandPacket packet;
    if (Decode(reader, packet) != CommandFault::None)
    {
      ++m_rejectedDatagrams;
      continue;
    }

    // A packet says who sent it, and the host takes that at face value: R19 makes the host
    // authoritative over what an order DOES, and section 5 declines authentication outright.
    // What it does not take on faith is what the order touches -- that is Q24, and CommandIntake
    // has already refused anything this player does not own.
    bool known = false;
    for (Client& client : m_clients)
    {
      if (client.endpoint == sender)
      {
        client.player = packet.player;
        known = true;
        break;
      }
    }
    if (!known && (m_clients.size() < CommandIntake::MAX_PLAYERS))
    {
      m_clients.push_back(Client{.endpoint = sender, .player = packet.player});
    }

    static_cast<void>(m_intake.ApplyPacket(m_world, packet));
  }
}

void Host::SendSnapshots()
{
  if (m_clients.empty())
  {
    // Still advance the sequence? No. A client watches the sequence advance by exactly one per
    // snapshot it receives, so a sequence that moved while nobody was listening would make the
    // first snapshot after a join look like a gap.
    return;
  }

  const Snapshot snapshot = BuildSnapshot(m_world, m_intake, m_tick, m_snapshotSequence, PLAYER_COUNT);

  std::array<std::byte, SCRATCH_BYTES> scratch{};
  Neuron::ByteWriter writer{scratch};
  if (!Encode(snapshot, writer))
  {
    return;
  }
  if (writer.WrittenBytes() > PAYLOAD_BYTES)
  {
    // ADR-003's property, checked at run time rather than only in a test. Sending this would
    // fragment, and fragmentation is what the whole replication design declines.
    return;
  }

  const std::span<const std::byte> datagram{scratch.data(), writer.WrittenBytes()};
  for (const Client& client : m_clients)
  {
    static_cast<void>(m_transport.Send(client.endpoint, datagram));
  }

  ++m_snapshotSequence;
}

void Host::RunOneTick()
{
  DrainAndApply();
  Tick(m_world);
  ++m_tick;
  SendSnapshots();
}

} // namespace Outpost
