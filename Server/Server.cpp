#include "pch.h"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <thread>

// The host, and the whole of it: a console executable that names what it was linked against and
// exits, plus M0.5's temporary probe mode. The match loop arrives with the simulation.

namespace
{
void PrintLibraryName(std::string_view _name)
{
  std::string line{_name};
  line.push_back('\n');
  std::fputs(line.c_str(), stdout);
}

// ---------------------------------------------------------------------------------------------
// M0.5 SCAFFOLDING, AND IT IS MEANT TO BE DELETED.
//
// `Design/Plan/M0-the-wire.md` M0.5 is a gate rather than a feature: it adds no product code, and
// what it does add is a host mode that sends a numbered packet at a fixed rate and a client that
// logs what arrives. Everything between this banner and the next one goes when the gate has its
// four answers and ADR-008 and TechnicalDesign.md section 9 have been written.
//
// It stays thin on purpose (R20, which names `Server` explicitly): there is no arithmetic here a
// suite could pin. The framing is ProbePacket's and NeuronCoreTests owns it; the loss and the
// jitter are computed by `Scripts/ProbeReport.py` from the CLIENT's log, because the client is
// the side that can see what did not arrive.
// ---------------------------------------------------------------------------------------------

[[nodiscard]] std::uint64_t MillisecondsSince(std::chrono::steady_clock::time_point _start) noexcept
{
  const auto elapsed = std::chrono::steady_clock::now() - _start;
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

void PrintEndpoint(std::string_view _label, const Neuron::Endpoint& _endpoint)
{
  // Host byte order, so the octets come out most significant first exactly as they are written.
  std::printf("%.*s %u.%u.%u.%u:%u\n", static_cast<int>(_label.size()), _label.data(), (_endpoint.addressV4 >> 24) & 0xFFu,
              (_endpoint.addressV4 >> 16) & 0xFFu, (_endpoint.addressV4 >> 8) & 0xFFu, _endpoint.addressV4 & 0xFFu, _endpoint.port);
}

/// Sends a numbered Heartbeat to whoever spoke last, at ProbePacket::RATE_HZ, until the duration
/// runs out or the console is interrupted. Returns the process exit code.
[[nodiscard]] int RunProbe(std::uint32_t _durationSeconds)
{
  Neuron::WinsockTransport transport;
  if (!transport.Open(Neuron::ProbePacket::PORT))
  {
    std::printf("probe: could not open port %u, WSA fault %d\n", Neuron::ProbePacket::PORT, transport.LastFault());
    return 1;
  }

  PrintEndpoint("probe: listening on", transport.BoundEndpoint());
  std::printf("probe: sending Heartbeat at %u Hz once a client says hello; Ctrl+C to stop\n", Neuron::ProbePacket::RATE_HZ);
  std::fflush(stdout);

  const auto start = std::chrono::steady_clock::now();
  const auto interval = std::chrono::milliseconds{1000 / Neuron::ProbePacket::RATE_HZ};
  auto nextSend = start;

  Neuron::Endpoint client{};
  bool haveClient = false;
  std::uint16_t sequence = 0;
  std::uint64_t sentCount = 0;
  std::uint64_t receivedCount = 0;

  std::array<std::byte, 2048> scratch{};
  while (_durationSeconds == 0 || MillisecondsSince(start) < std::uint64_t{_durationSeconds} * 1000u)
  {
    // Drain first, so a client that has just appeared is sent to on this pass rather than the next.
    for (;;)
    {
      std::size_t byteCount = 0;
      Neuron::Endpoint sender{};
      const Neuron::ReceiveOutcome outcome = transport.Receive(scratch, byteCount, sender);
      if (outcome == Neuron::ReceiveOutcome::Empty)
      {
        break;
      }
      if (outcome == Neuron::ReceiveOutcome::Failed)
      {
        std::printf("probe: receive failed, WSA fault %d\n", transport.LastFault());
        break;
      }
      if (outcome == Neuron::ReceiveOutcome::Oversized)
      {
        std::printf("probe: a datagram larger than %zu bytes was discarded\n", scratch.size());
        continue;
      }

      ++receivedCount;
      if (!haveClient || !(sender == client))
      {
        client = sender;
        haveClient = true;
        PrintEndpoint("probe: client is", client);
        std::fflush(stdout);
      }
    }

    const auto now = std::chrono::steady_clock::now();
    if (haveClient && now >= nextSend)
    {
      std::array<std::byte, Neuron::ProbePacket::SIZE_BYTES> datagram{};
      Neuron::ByteWriter writer{datagram};
      const Neuron::ProbePacket packet{.sequence = sequence, .sentAtMs = MillisecondsSince(start)};
      if (packet.Write(writer) && transport.Send(client, std::span<const std::byte>{datagram.data(), writer.WrittenBytes()}))
      {
        ++sentCount;
      }

      ++sequence;
      nextSend += interval;

      // A machine that stalled for a second must not then send twenty packets back to back: that
      // is a burst the network never saw, and it would read as jitter the link did not cause.
      if (nextSend < now)
      {
        nextSend = now;
      }
    }

    // NOT sleep_for. Windows rounds a short sleep up to the system timer granularity, which is
    // 15.6 ms by default, and a probe that polls that coarsely quantizes every arrival into a
    // 15 ms bucket -- reporting as jitter what is actually its own scheduler. A measurement whose
    // whole purpose is to find the real figure cannot have a noise floor three times the tick, so
    // this spins instead and spends a core for the length of a run. Deliberate, and temporary.
    std::this_thread::yield();
  }

  std::printf("probe: sent %llu, received %llu\n", static_cast<unsigned long long>(sentCount),
              static_cast<unsigned long long>(receivedCount));
  return 0;
}

// --------------------------------------------------------------------------------------------
// End of M0.5 scaffolding.
// --------------------------------------------------------------------------------------------
} // namespace

int main(int _argc, char** _argv)
{
  const std::span<char*> arguments{_argv, static_cast<std::size_t>(_argc)};
  if (_argc > 1 && std::string_view{arguments[1]} == "--probe")
  {
    std::uint32_t durationSeconds = 0;
    if (_argc > 2)
    {
      const std::string_view given{arguments[2]};
      if (std::from_chars(given.data(), given.data() + given.size(), durationSeconds).ec != std::errc{})
      {
        std::fputs("usage: Server --probe [durationSeconds]\n", stderr);
        return 2;
      }
    }
    return RunProbe(durationSeconds);
  }

  PrintLibraryName(Neuron::CoreLibraryName());
  PrintLibraryName(Neuron::ServerLibraryName());
  PrintLibraryName(Outpost::CoreLibraryName());
  PrintLibraryName(Outpost::LogicLibraryName());
  return 0;
}
