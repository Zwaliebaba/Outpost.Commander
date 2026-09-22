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

Snapshot BuildSnapshot(const World& _world, const CommandIntake& _intake, const BuildSystem& _build, std::uint32_t _tick,
                       std::uint16_t _sequence, std::size_t _playerCount)
{
  Snapshot snapshot;
  snapshot.sequence = _sequence;
  snapshot.tick = _tick;

  for (std::size_t player = 0; player < _playerCount; ++player)
  {
    const PlayerId identity = static_cast<PlayerId>(player + 1);
    // M1.6: all four fields carry meaning. The two build bytes are Q21's whole answer to the queue
    // that had no wire record -- the item and its progress, and nothing else.
    snapshot.players.push_back(PlayerBlock{.credits = _build.Credits(identity),
                                           .lastCommandSequenceApplied = _intake.LastAppliedSequence(identity),
                                           .buildingDesign = _build.WireBuildingDesign(identity),
                                           .buildProgressPercent = _build.WireProgressPercent(identity)});
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

Host::Host()
{
  BeginMatch(DEFAULT_MATCH_SEED);
}

void Host::BeginMatch(std::uint64_t _matchSeed)
{
  m_world = World{};
  m_intake = CommandIntake{};
  m_build.Begin(PLAYER_COUNT);
  m_sessions.Begin(PLAYER_COUNT, _matchSeed);

  // M1.5: the stations, from `GameCore`'s generator -- the same function the client runs to draw the
  // same field (R23). A station needs no code of its own here because it is a row in the design
  // table (ADR-006), so this is `World::Create` like anything else.
  for (const Placement& placed : GenerateLayout(_matchSeed, PLAYER_COUNT))
  {
    static_cast<void>(m_world.Create(placed.position, placed.heading, placed.design, placed.owner));
  }
}

bool Host::Open(std::uint16_t _port) noexcept
{
  return m_transport.Open(_port);
}

void Host::Close() noexcept
{
  m_transport.Close();

  // THE SEATS GO WITH THE SOCKET AND NOT WITH A DISCONNECT. `GameDesign.md` section 2 holds a
  // slot indefinitely while the match runs; closing the host ends the match (ADR-013).
  m_sessions.Clear();
}

void Host::AnswerJoin(std::span<const std::byte> _datagram, const Neuron::Endpoint& _sender) noexcept
{
  Neuron::ByteReader reader{_datagram};
  Join join{};
  if (Decode(reader, join) != JoinFault::None)
  {
    ++m_rejectedDatagrams;
    return;
  }

  ++m_joins;
  const JoinReply reply = m_sessions.Admit(join, _sender);

  std::array<std::byte, JoinReply::SIZE_BYTES + Neuron::PacketHeader::SIZE_BYTES> outgoing{};
  Neuron::ByteWriter writer{outgoing};
  if (!Encode(reply, writer))
  {
    return;
  }

  // NOT RETRANSMITTED AND NOT ACKNOWLEDGED (ADR-013). A lost reply costs the client one retry,
  // and the retry is answered with the same seat because `Admit` matches on the endpoint.
  static_cast<void>(m_transport.Send(_sender, std::span<const std::byte>{outgoing.data(), writer.WrittenBytes()}));
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

    const std::span<const std::byte> datagram{scratch.data(), byteCount};

    // THE TYPE IS READ BEFORE THE RECORD IS, because two records now arrive on one socket and a
    // decoder that is handed the wrong one reports a fault rather than a miss. The header is read
    // twice -- once here and once inside whichever decoder takes it -- which is six bytes.
    Neuron::ByteReader probe{datagram};
    Neuron::PacketHeader header{};
    if (Neuron::PacketHeader::Read(probe, header) != Neuron::PacketFault::None)
    {
      ++m_rejectedDatagrams;
      continue;
    }

    if (header.type == Neuron::PacketType::Join)
    {
      AnswerJoin(datagram, sender);
      continue;
    }
    if (header.type != Neuron::PacketType::Command)
    {
      // A heartbeat, or a reply this side never receives. Neither is an error and neither has
      // anywhere to go yet; both are counted rather than acted on.
      ++m_rejectedDatagrams;
      continue;
    }

    Neuron::ByteReader reader{datagram};
    CommandPacket packet;
    if (Decode(reader, packet) != CommandFault::None)
    {
      ++m_rejectedDatagrams;
      continue;
    }

    // **THE HOST RESOLVES THE SENDER FROM ITS SESSION, NOT FROM THE BYTE THE PACKET CARRIES**
    // (ADR-013). An endpoint that never joined is refused outright -- which is the teeth of the
    // join, because until it existed any endpoint that sent a command was believed.
    //
    // R19 still makes the host authoritative over what an order DOES, and section 5 still
    // declines authentication: this is not a credential check, it is the host knowing who it
    // seated. What it does not take on faith is what the order touches -- Q24, and CommandIntake
    // has already refused anything this player does not own.
    const PlayerId seated = m_sessions.PlayerAt(sender);
    if (seated == NO_PLAYER)
    {
      ++m_unjoinedCommands;
      continue;
    }
    if (packet.player != seated)
    {
      // A stale packet from a previous match, or a client that has not read its reply yet. The
      // session wins and the disagreement becomes a number rather than a silence.
      ++m_misaddressedCommands;
      packet.player = seated;
    }

    static_cast<void>(m_intake.ApplyPacket(m_world, m_build, packet));
  }
}

void Host::SendSnapshots()
{
  if (m_sessions.Count() == 0)
  {
    // Still advance the sequence? No. A client watches the sequence advance by exactly one per
    // snapshot it receives, so a sequence that moved while nobody was listening would make the
    // first snapshot after a join look like a gap.
    return;
  }

  const Snapshot snapshot = BuildSnapshot(m_world, m_intake, m_build, m_tick, m_snapshotSequence, PLAYER_COUNT);

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

  // **TO SESSIONS, NOT TO ENDPOINTS THAT HAVE SPOKEN** (ADR-013). A client that has not joined
  // sees nothing at all, which is a change from M0 and is the correct one.
  const std::span<const std::byte> datagram{scratch.data(), writer.WrittenBytes()};
  for (const Sessions::Session& session : m_sessions.All())
  {
    static_cast<void>(m_transport.Send(session.endpoint, datagram));
  }

  ++m_snapshotSequence;
}

void Host::RunOneTick()
{
  DrainAndApply();
  Tick(m_world);

  // AFTER THE MOVEMENT, so a ship that appears this tick does not also move on it -- which would
  // put it somewhere no snapshot ever said it started from.
  m_build.Advance(m_world);

  ++m_tick;
  SendSnapshots();
}

} // namespace Outpost
