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

// The host's shell, and nothing else. R20 names `Server` explicitly: no game logic, no arithmetic,
// no decision a test could pin lives here. The loop's body is `Outpost::Host` in `GameLogic`,
// where a suite can reach it; this file opens a socket, drives the tick schedule, and waits.
//
// Check a diff against that rule specifically. It is the one an executable breaks quietly, because
// putting "just one" branch here is always easier than putting it where it can be tested.

namespace
{
// ---------------------------------------------------------------------------------------------
// M0.5 SCAFFOLDING, AND IT IS MEANT TO BE DELETED -- see the banner at the end of this block.
// ---------------------------------------------------------------------------------------------

[[nodiscard]] std::uint64_t MillisecondsSince(std::chrono::steady_clock::time_point _start) noexcept
{
  const auto elapsed = std::chrono::steady_clock::now() - _start;
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

void PrintEndpoint(std::string_view _label, const Neuron::Endpoint& _endpoint)
{
  std::printf("%.*s %u.%u.%u.%u:%u\n", static_cast<int>(_label.size()), _label.data(), (_endpoint.addressV4 >> 24) & 0xFFu,
              (_endpoint.addressV4 >> 16) & 0xFFu, (_endpoint.addressV4 >> 8) & 0xFFu, _endpoint.addressV4 & 0xFFu, _endpoint.port);
}

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
      if (nextSend < now)
      {
        nextSend = now;
      }
    }

    std::this_thread::yield();
  }

  std::printf("probe: sent %llu, received %llu\n", static_cast<unsigned long long>(sentCount),
              static_cast<unsigned long long>(receivedCount));
  return 0;
}

// ---------------------------------------------------------------------------------------------
// End of M0.5 scaffolding.
// ---------------------------------------------------------------------------------------------

[[nodiscard]] int RunHost(std::uint16_t _port, std::uint32_t _durationSeconds, std::uint64_t _matchSeed, std::size_t _playerCount,
                          bool _stress, std::size_t _aiSeats)
{
  Outpost::Host host;
  // **THE TOKEN SALT IS WALL TIME, AND THIS IS THE ONE PLACE IT MAY BE** (R16: the shell is the seam). Two
  // runs of the host on one seed must not issue one set of session tokens, or two returning clients that
  // join in the other order take each other's seat (the 2026-09-23 review, B4). Both clocks are mixed so
  // that neither one alone repeating is enough to repeat the salt.
  const std::uint64_t tokenSalt = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()) ^
                                  (static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) << 17);
  host.SaltTokens(tokenSalt);
  host.BeginMatch(_matchSeed, _playerCount);
  host.SetAiSeats(_aiSeats);
  if (!host.Open(_port))
  {
    std::printf("host: could not open port %u, WSA fault %d\n", _port, host.LastFault());
    return 1;
  }

  PrintEndpoint("host: listening on", host.BoundEndpoint());
  // The seed is printed because ADR-013 hands it to every client and R23 has both sides drawing the
  // same field from it -- so the one number that has to match across two machines is the one number
  // worth seeing in the log.
  std::printf("host: match seed %llu\n", static_cast<unsigned long long>(host.MatchSeed()));
  // **A STRESS RUN SAYS SO**, first thing, because its layout is not fair and its numbers are not a match's.
  std::printf("host: %zu players%s\n", host.PlayerCount(), _stress ? " -- STRESS CONFIGURATION, not a match" : "");
  std::printf("host: %zu of them the stub AI's\n", host.Ai().Count());
  std::printf("host: %lld ms a tick; Ctrl+C to stop\n", static_cast<long long>(Outpost::TICK_PERIOD_MILLISECONDS));
  std::fflush(stdout);

  // THE SEAM, AND IT LIVES IN THE SHELL. R16 keeps wall time out of the simulation's library, so
  // the schedule is the engine's and the host below it counts only ticks. The three lines that
  // join them are this loop, which holds no arithmetic a suite could want (R20).
  const auto start = std::chrono::steady_clock::now();
  Neuron::TickSchedule schedule{start, Outpost::TICK_PERIOD_MILLISECONDS};
  std::uint64_t reportedAtSecond = 0;
  std::uint16_t reportedMatches = 0;

  for (;;)
  {
    const auto now = std::chrono::steady_clock::now();
    if ((_durationSeconds != 0) && (MillisecondsSince(start) >= (std::uint64_t{_durationSeconds} * 1000u)))
    {
      break;
    }

    const std::uint32_t due = schedule.TicksDue(now);
    for (std::uint32_t tick = 0; tick < due; ++tick)
    {
      host.RunOneTick();

      // M3.8: A MATCH THAT ENDED IS SAID, with the seed the next one plays, since that is the number a log is read for.
      if (host.LastEnded().matchNumber != reportedMatches)
      {
        reportedMatches = host.LastEnded().matchNumber;
        const Outpost::MatchEnded& ended = host.LastEnded();
        if (ended.winner == Outpost::NO_PLAYER)
        {
          std::printf("host: match %u ended in a draw%s; next seed %llu\n", static_cast<unsigned>(ended.matchNumber),
                      ended.onClock ? " on the clock" : "", static_cast<unsigned long long>(host.MatchSeed()));
        }
        else
        {
          std::printf("host: match %u won by player %u%s; next seed %llu\n", static_cast<unsigned>(ended.matchNumber),
                      static_cast<unsigned>(ended.winner), ended.onClock ? " on the clock" : "",
                      static_cast<unsigned long long>(host.MatchSeed()));
        }
        std::fflush(stdout);
      }
    }

    // A line a second, so a run of some minutes can be read afterwards rather than watched.
    const std::uint64_t second = MillisecondsSince(start) / 1000u;
    if (second != reportedAtSecond)
    {
      reportedAtSecond = second;
      std::printf("host: t=%llus ticks=%llu abandoned=%llu updates=%llu clients=%zu rejected=%llu\n",
                  static_cast<unsigned long long>(second), static_cast<unsigned long long>(schedule.TicksIssued()),
                  static_cast<unsigned long long>(schedule.TicksAbandoned()), static_cast<unsigned long long>(host.UpdatesSent()),
                  host.ClientCount(), static_cast<unsigned long long>(host.RejectedDatagramCount()));
      std::fflush(stdout);
    }

    // Wait for the next tick rather than spin. The deadline comes from the schedule, which is the
    // one thing in this process that knows what time it is in ticks.
    std::this_thread::sleep_until(schedule.NextDeadline());
  }

  std::printf("host: %llu ticks, %llu abandoned, %llu updates\n", static_cast<unsigned long long>(schedule.TicksIssued()),
              static_cast<unsigned long long>(schedule.TicksAbandoned()), static_cast<unsigned long long>(host.UpdatesSent()));
  std::printf("host: %llu joins, %llu commands from nobody seated, %llu misaddressed\n", static_cast<unsigned long long>(host.JoinCount()),
              static_cast<unsigned long long>(host.UnjoinedCommandCount()),
              static_cast<unsigned long long>(host.MisaddressedCommandCount()));
  host.Close();
  return 0;
}

[[nodiscard]] bool ParseNumber(std::string_view _text, std::uint32_t& _outValue) noexcept
{
  return std::from_chars(_text.data(), _text.data() + _text.size(), _outValue).ec == std::errc{};
}

/// The seed is sixty-four bits, so it does not fit the parser above and does not get a cast that
/// would quietly truncate it (ADR-013).
[[nodiscard]] bool ParseSeed(std::string_view _text, std::uint64_t& _outValue) noexcept
{
  return std::from_chars(_text.data(), _text.data() + _text.size(), _outValue).ec == std::errc{};
}

/// One string, because four copies of it were four chances to add an option to three of them.
inline constexpr const char* USAGE = "usage: Server [--port N] [--seconds N] [--seed N] [--players N [--stress]] [--ai N] [--probe]\n"
                                     "  --players is 1 to 4, or up to 254 with --stress (ADR-023)\n"
                                     "  --ai gives the last N seats to the stub AI (M3.10); at most the player count\n";
} // namespace

int main(int _argc, char** _argv)
{
  const std::span<char*> arguments{_argv, static_cast<std::size_t>(_argc)};

  std::uint32_t port = Outpost::Host::DEFAULT_PORT;
  std::uint32_t durationSeconds = 0;

  // ADR-013 makes the match seed the host's, and configuration. Zero is a legal seed, so the
  // default is a named constant rather than a sentinel.
  std::uint64_t matchSeed = Outpost::DEFAULT_MATCH_SEED;
  std::uint32_t playerCount = static_cast<std::uint32_t>(Outpost::Host::DEFAULT_PLAYER_COUNT);
  bool stress = false;
  bool probe = false;
  std::uint32_t aiSeats = 0;

  for (std::size_t index = 1; index < arguments.size(); ++index)
  {
    const std::string_view argument{arguments[index]};
    const bool hasValue = (index + 1) < arguments.size();

    if (argument == "--probe")
    {
      probe = true;
    }
    else if (argument == "--stress")
    {
      stress = true;
    }
    else if ((argument == "--players") && hasValue)
    {
      ++index;
      if (!ParseNumber(std::string_view{arguments[index]}, playerCount))
      {
        std::fputs(USAGE, stderr);
        return 2;
      }
    }
    else if ((argument == "--ai") && hasValue)
    {
      ++index;
      if (!ParseNumber(std::string_view{arguments[index]}, aiSeats))
      {
        std::fputs(USAGE, stderr);
        return 2;
      }
    }
    else if ((argument == "--port") && hasValue)
    {
      ++index;
      if (!ParseNumber(std::string_view{arguments[index]}, port) || (port > 65535))
      {
        std::fputs(USAGE, stderr);
        return 2;
      }
    }
    else if ((argument == "--seed") && hasValue)
    {
      ++index;
      if (!ParseSeed(std::string_view{arguments[index]}, matchSeed))
      {
        std::fputs(USAGE, stderr);
        return 2;
      }
    }
    else if ((argument == "--seconds") && hasValue)
    {
      ++index;
      if (!ParseNumber(std::string_view{arguments[index]}, durationSeconds))
      {
        std::fputs(USAGE, stderr);
        return 2;
      }
    }
    else
    {
      std::fputs(USAGE, stderr);
      return 2;
    }
  }

  // CHECKED AFTER EVERY ARGUMENT IS READ, so `--stress` may come before or after `--players`.
  if (!Outpost::PlayerCountAllowed(playerCount, stress) || (aiSeats > playerCount))
  {
    std::fputs(USAGE, stderr);
    return 2;
  }

  return probe ? RunProbe(durationSeconds)
               : RunHost(static_cast<std::uint16_t>(port), durationSeconds, matchSeed, static_cast<std::size_t>(playerCount), stress,
                         static_cast<std::size_t>(aiSeats));
}
