#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr std::uint64_t SEED = 0xF100Dull;

[[nodiscard]] std::vector<Outpost::FloodDatagram> Produce(Outpost::FloodSchedule& _schedule, std::uint32_t _ticks)
{
  std::vector<Outpost::FloodDatagram> datagrams;
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
  {
    _schedule.Produce(datagrams);
  }
  return datagrams;
}

[[nodiscard]] Neuron::PacketFault HeaderFault(const Outpost::FloodDatagram& _datagram)
{
  Neuron::ByteReader reader{std::span<const std::byte>{_datagram.bytes}};
  Neuron::PacketHeader header{};
  return Neuron::PacketHeader::Read(reader, header);
}

[[nodiscard]] Outpost::CommandFault CommandFaultOf(const Outpost::FloodDatagram& _datagram, Outpost::CommandPacket& _outPacket)
{
  Neuron::ByteReader reader{std::span<const std::byte>{_datagram.bytes}};
  return Outpost::Decode(reader, _outPacket);
}
} // namespace

/// ADR-022's flooder: which refusable datagram goes out on which harness tick.
TEST_CLASS(TheFloodSchedule)
{
public:
  TEST_METHOD(TheSameSeedGivesTheSameDatagramsTwice)
  {
    Outpost::FloodSchedule first{SEED, 4, 5};
    Outpost::FloodSchedule second{SEED, 4, 5};
    first.EnableJoins();
    second.EnableJoins();

    const std::vector<Outpost::FloodDatagram> a = Produce(first, 100);
    const std::vector<Outpost::FloodDatagram> b = Produce(second, 100);
    Assert::AreEqual(static_cast<std::size_t>(500), a.size());
    Assert::AreEqual(a.size(), b.size());
    for (std::size_t index = 0; index < a.size(); ++index)
    {
      Assert::IsTrue(a[index].kind == b[index].kind, L"the same seed chose a different kind");
      Assert::IsTrue(a[index].bytes == b[index].bytes, L"the same seed built different bytes");
    }
  }

  /// **EVERY DATAGRAM IS ONE THE HOST REFUSES, BY NAME.** The three malformed kinds are refused by the decoders,
  /// each with its own fault; the two well-formed kinds decode cleanly and are refused by who sent them -- a
  /// full match for the join, the session table for the command -- which a socket-free suite can state but only
  /// the run against `Server` can watch happen.
  TEST_METHOD(EveryDatagramIsRefusedByName)
  {
    Outpost::FloodSchedule schedule{SEED, 0, Outpost::FloodSchedule::MAX_DATAGRAMS_PER_TICK};
    schedule.EnableJoins();

    for (const Outpost::FloodDatagram& datagram : Produce(schedule, 50))
    {
      Assert::IsFalse(datagram.bytes.empty(), L"an empty datagram, which the transport would not send");
      Outpost::CommandPacket packet;

      switch (datagram.kind)
      {
      case Outpost::FloodKind::TruncatedHeader:
        Assert::IsTrue(HeaderFault(datagram) == Neuron::PacketFault::Truncated);
        Assert::IsTrue(CommandFaultOf(datagram, packet) == Outpost::CommandFault::Truncated);
        break;

      case Outpost::FloodKind::WrongVersion:
        Assert::IsTrue(HeaderFault(datagram) == Neuron::PacketFault::VersionMismatch);
        Assert::IsTrue(CommandFaultOf(datagram, packet) == Outpost::CommandFault::VersionMismatch);
        break;

      case Outpost::FloodKind::ImpossibleCount:
        Assert::IsTrue(HeaderFault(datagram) == Neuron::PacketFault::None, L"the header should be whole");
        Assert::IsTrue(CommandFaultOf(datagram, packet) == Outpost::CommandFault::Malformed);
        break;

      case Outpost::FloodKind::JoinPastFull:
      {
        Neuron::ByteReader reader{std::span<const std::byte>{datagram.bytes}};
        Outpost::Join join{};
        Assert::IsTrue(Outpost::Decode(reader, join) == Outpost::JoinFault::None);
        Assert::IsTrue(join.token == Outpost::NO_SESSION_TOKEN, L"a flooder presented a token and would resume a seat");
        break;
      }

      case Outpost::FloodKind::UnseatedCommand:
        Assert::IsTrue(CommandFaultOf(datagram, packet) == Outpost::CommandFault::None);
        Assert::AreEqual(static_cast<std::size_t>(1), packet.commands.size());
        Assert::IsFalse(packet.commands.front().selection.empty());
        break;
      }
    }
  }

  /// Every kind within one rotation, so a run of any length exercises every refusal path.
  TEST_METHOD(OneRotationCoversEveryKind)
  {
    Outpost::FloodSchedule schedule{SEED, 3, static_cast<std::uint32_t>(Outpost::FLOOD_KIND_COUNT)};
    schedule.EnableJoins();
    std::vector<Outpost::FloodDatagram> datagrams;
    schedule.Produce(datagrams);

    for (std::size_t kind = 0; kind < Outpost::FLOOD_KIND_COUNT; ++kind)
    {
      Assert::AreEqual(static_cast<std::uint64_t>(1), schedule.SentCount(static_cast<Outpost::FloodKind>(kind)));
    }
  }

  /// **A FLOODER IS NEVER SEATED.** No join until the harness says the match is full, and once the host has
  /// seated it anyway, neither of the kinds that need it unseated.
  TEST_METHOD(ItJoinsOnlyPastAFullMatchAndStopsOnceSeated)
  {
    Outpost::FloodSchedule schedule{SEED, 1, 8};
    for (const Outpost::FloodDatagram& datagram : Produce(schedule, 100))
    {
      Assert::IsTrue(datagram.kind != Outpost::FloodKind::JoinPastFull, L"joined before the match was full");
    }

    schedule.EnableJoins();
    schedule.NoteJoinReply(Outpost::JoinResult::MatchFull);
    Assert::IsFalse(schedule.WasSeated());
    Assert::IsTrue(schedule.SentCount(Outpost::FloodKind::JoinPastFull) == 0);
    static_cast<void>(Produce(schedule, 10));
    Assert::IsTrue(schedule.SentCount(Outpost::FloodKind::JoinPastFull) > 0, L"joins enabled and none sent");

    schedule.NoteJoinReply(Outpost::JoinResult::Accepted);
    Assert::IsTrue(schedule.WasSeated());
    for (const Outpost::FloodDatagram& datagram : Produce(schedule, 100))
    {
      Assert::IsTrue(datagram.kind != Outpost::FloodKind::JoinPastFull, L"joined again after being seated");
      Assert::IsTrue(datagram.kind != Outpost::FloodKind::UnseatedCommand, L"sent an 'unseated' command from a seat");
    }
  }

  TEST_METHOD(TheRateIsClampedToWhatTheScheduleOffers)
  {
    Assert::AreEqual(1u, Outpost::FloodSchedule{SEED, 0, 0}.DatagramsPerTick());
    Assert::AreEqual(Outpost::FloodSchedule::MAX_DATAGRAMS_PER_TICK, Outpost::FloodSchedule{SEED, 0, 100000}.DatagramsPerTick());
  }
};

} // namespace GameClientTests
