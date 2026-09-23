#include "pch.h"

#include "FloodSchedule.h"
#include "StressReport.h"

#include <algorithm>
#include <utility>

namespace Outpost
{

FloodSchedule::FloodSchedule(std::uint64_t _runSeed, std::uint32_t _botIndex, std::uint32_t _datagramsPerTick) noexcept
  : m_random{_runSeed, BotStream(BotRole::Flooder, _botIndex)},
    m_datagramsPerTick{std::clamp(_datagramsPerTick, std::uint32_t{1}, MAX_DATAGRAMS_PER_TICK)}
{
  m_nextKind = m_random.NextBelow(static_cast<std::uint32_t>(FLOOD_KIND_COUNT));
}

bool FloodSchedule::Allowed(FloodKind _kind) const noexcept
{
  switch (_kind)
  {
  case FloodKind::JoinPastFull:
    return m_joinsEnabled && !m_wasSeated;
  case FloodKind::UnseatedCommand:
    return !m_wasSeated;
  case FloodKind::TruncatedHeader:
  case FloodKind::WrongVersion:
  case FloodKind::ImpossibleCount:
    return true;
  }
  return false;
}

void FloodSchedule::Produce(std::vector<FloodDatagram>& _outDatagrams)
{
  for (std::uint32_t produced = 0; produced < m_datagramsPerTick; ++produced)
  {
    // Three kinds are always allowed, so this finds one within the rotation.
    FloodKind kind = static_cast<FloodKind>(m_nextKind);
    for (std::size_t step = 0; (step < FLOOD_KIND_COUNT) && !Allowed(kind); ++step)
    {
      m_nextKind = static_cast<std::uint32_t>((m_nextKind + 1) % FLOOD_KIND_COUNT);
      kind = static_cast<FloodKind>(m_nextKind);
    }
    m_nextKind = static_cast<std::uint32_t>((m_nextKind + 1) % FLOOD_KIND_COUNT);

    _outDatagrams.push_back(Build(kind));
    ++m_sent[static_cast<std::size_t>(kind)];
  }
}

void FloodSchedule::NoteJoinReply(JoinResult _result) noexcept
{
  if ((_result == JoinResult::Accepted) || (_result == JoinResult::Rejoined))
  {
    m_wasSeated = true;
  }
}

FloodDatagram FloodSchedule::Build(FloodKind _kind)
{
  FloodDatagram datagram{.kind = _kind, .bytes = {}};

  switch (_kind)
  {
  case FloodKind::JoinPastFull:
  {
    datagram.bytes.resize(Neuron::PacketHeader::SIZE_BYTES + Join::SIZE_BYTES);
    Neuron::ByteWriter writer{datagram.bytes};
    static_cast<void>(Encode(Join{.token = NO_SESSION_TOKEN}, writer));
    datagram.bytes.resize(writer.WrittenBytes());
    break;
  }

  case FloodKind::UnseatedCommand:
  {
    // **A PLAUSIBLE ORDER, NAMING A SEAT THAT IS NOT ITS OWN.** The host must go by the endpoint and not the
    // player byte, and this is the datagram that finds out if it does.
    CommandPacket packet;
    packet.sequence = m_sequence++;
    packet.player = static_cast<PlayerId>(1 + m_random.NextBelow(static_cast<std::uint32_t>(MATCH_PLAYERS)));
    packet.viewRadiusUnits = static_cast<std::uint16_t>(m_random.NextBelow(4096));

    Command order{
      .sequence = static_cast<std::uint16_t>(m_random.Next()), .type = CommandType::MoveTo, .targetX = 0, .targetY = 0, .selection = {}};
    order.targetX = static_cast<std::int16_t>(m_random.NextInRange(-32768, 32767));
    order.targetY = static_cast<std::int16_t>(m_random.NextInRange(-32768, 32767));
    const std::uint32_t selected = 1 + m_random.NextBelow(8);
    for (std::uint32_t index = 0; index < selected; ++index)
    {
      const auto slot = static_cast<std::uint16_t>(m_random.NextBelow(65536));
      const auto generation = static_cast<std::uint16_t>(1 + m_random.NextBelow(255));
      order.selection.push_back(PackIdentity(slot, generation));
    }
    packet.commands.push_back(std::move(order));

    datagram.bytes.resize(EncodedSize(packet));
    Neuron::ByteWriter writer{datagram.bytes};
    static_cast<void>(Encode(packet, writer));
    datagram.bytes.resize(writer.WrittenBytes());
    break;
  }

  case FloodKind::TruncatedHeader:
  {
    // One to three bytes: never empty, which the transport would refuse to send, and never a whole header.
    const std::uint32_t length = 1 + m_random.NextBelow(static_cast<std::uint32_t>(Neuron::PacketHeader::SIZE_BYTES - 1));
    for (std::uint32_t index = 0; index < length; ++index)
    {
      datagram.bytes.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(m_random.Next())));
    }
    break;
  }

  case FloodKind::WrongVersion:
  {
    // **ANY VERSION BUT THIS BUILD'S**: an offset of one to 255 from it, modulo a byte, is never zero.
    const auto version = static_cast<std::uint8_t>(Neuron::PROTOCOL_VERSION + 1 + m_random.NextBelow(255));
    datagram.bytes.resize(Neuron::PacketHeader::SIZE_BYTES + 8);
    Neuron::ByteWriter writer{datagram.bytes};
    writer.WriteUInt8(version);
    writer.WriteUInt8(static_cast<std::uint8_t>(Neuron::PacketType::Command));
    writer.WriteUInt16(m_sequence++);
    for (std::size_t index = 0; index < 8; ++index)
    {
      writer.WriteUInt8(static_cast<std::uint8_t>(m_random.Next()));
    }
    break;
  }

  case FloodKind::ImpossibleCount:
  {
    // **A COUNT OF ONE OR MORE, AND FEWER THAN ONE COMMAND'S BYTES BEHIND IT.** A command is at least
    // `Command::FIXED_BYTES`, so any count at all overruns what follows. A decoder that reserved from the
    // count before checking it would allocate for up to 255 commands per datagram; `Decode` refuses first.
    const std::uint32_t trailing = m_random.NextBelow(static_cast<std::uint32_t>(Command::FIXED_BYTES));
    datagram.bytes.resize(CommandPacket::HEADER_BYTES + trailing);
    Neuron::ByteWriter writer{datagram.bytes};
    writer.WriteUInt8(Neuron::PROTOCOL_VERSION);
    writer.WriteUInt8(static_cast<std::uint8_t>(Neuron::PacketType::Command));
    writer.WriteUInt16(m_sequence++);
    writer.WriteUInt8(static_cast<std::uint8_t>(1 + m_random.NextBelow(static_cast<std::uint32_t>(MATCH_PLAYERS))));
    writer.WriteUInt8(static_cast<std::uint8_t>(1 + m_random.NextBelow(255)));
    writer.WriteInt16(0);
    writer.WriteInt16(0);
    writer.WriteUInt16(0);
    for (std::uint32_t index = 0; index < trailing; ++index)
    {
      writer.WriteUInt8(static_cast<std::uint8_t>(m_random.Next()));
    }
    break;
  }
  }

  return datagram;
}

} // namespace Outpost
