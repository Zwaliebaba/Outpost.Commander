#include "pch.h"

#include <winrt/base.h>

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// The stress harness's shell, and nothing else (ADR-022). R20 names `Bot` with the other two executables:
// no decision a test could pin lives here. What a player orders is `Outpost::BotPolicy`, when a churner
// drops is `Outpost::ChurnSchedule`, what a flooder sends is `Outpost::FloodSchedule`, and what the run saw
// is `Outpost::StressReport`, all in `GameClient` where `GameClientTests` reaches them. This file parses the
// command line, owns a transport and a queue per bot, runs the one loop, and prints.
//
// **IT IS UNPACKAGED, SO THREE `NeuronClient` FUNCTIONS ARE OFF LIMITS**: `ReadHostAddress`, `ReadSessionToken`
// and `WriteSessionToken` need package identity. The address comes from the command line, through
// `HostAddressFromFileContents` so there is one parser; tokens live in each bot's `JoinState` for the life of
// the process. A call that reaches `ApplicationData` from here is a defect against ADR-022, and it would show
// as a `winrt::hresult_error` at run time rather than at link time.

namespace
{
/// Per bot. Sixty-four slots of one MTU: two updates a tick and a poll every few milliseconds leaves most of
/// them empty, and a hundred bots at the packaged client's megabyte each would be the harness's own ceiling.
inline constexpr std::size_t QUEUE_SLOTS = 64;
inline constexpr std::size_t QUEUE_SLOT_BYTES = 1536;

/// How long the loop sleeps between polls. Short beside the 50-millisecond tick, so a drain is never far
/// behind an arrival; the update tick gap is what says whether it kept up.
inline constexpr std::chrono::milliseconds POLL_INTERVAL{2};

inline constexpr const char* USAGE =
  "usage: Bot [--host ADDRESS] [--port N] [--players N] [--churners N] [--flooders N] [--flood-rate N] [--seed N] [--ticks N]\n"
  "  ticks are the harness's, at the host's 20 a second; the host keeps every seat until it is restarted (ADR-013)\n";

struct Arguments
{
  std::string host{Neuron::DEFAULT_HOST_ADDRESS};
  std::uint32_t port = Neuron::ProbePacket::PORT;
  std::uint32_t players = 2;
  std::uint32_t churners = 0;
  std::uint32_t flooders = 0;
  std::uint32_t floodRate = Outpost::FloodSchedule::DEFAULT_DATAGRAMS_PER_TICK;
  std::uint64_t seed = 1;
  std::uint64_t ticks = 1200;
};

/// One headless client. Not movable -- the transport holds a reference to the queue -- so bots live behind
/// pointers.
struct Bot
{
  Outpost::BotRole role = Outpost::BotRole::Player;
  std::uint32_t index = 0;
  Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
  Neuron::DatagramTransport transport{queue};
  Neuron::TransportRecovery recovery;
  Outpost::ClientFrame frame;
  std::optional<Outpost::BotPolicy> policy;
  std::optional<Outpost::ChurnSchedule> churn;
  std::optional<Outpost::FloodSchedule> flood;
  bool away = false;
};

[[nodiscard]] bool ParseNumber(std::string_view _text, std::uint32_t& _outValue) noexcept
{
  return std::from_chars(_text.data(), _text.data() + _text.size(), _outValue).ec == std::errc{};
}

[[nodiscard]] bool ParseNumber(std::string_view _text, std::uint64_t& _outValue) noexcept
{
  return std::from_chars(_text.data(), _text.data() + _text.size(), _outValue).ec == std::errc{};
}

[[nodiscard]] bool Parse(std::span<char*> _arguments, Arguments& _out)
{
  for (std::size_t index = 1; index < _arguments.size(); ++index)
  {
    const std::string_view argument{_arguments[index]};
    if ((index + 1) >= _arguments.size())
    {
      return false;
    }
    const std::string_view value{_arguments[++index]};

    bool parsed = true;
    if (argument == "--host")
    {
      _out.host = Neuron::HostAddressFromFileContents(value);
    }
    else if (argument == "--port")
    {
      parsed = ParseNumber(value, _out.port) && (_out.port <= 65535);
    }
    else if (argument == "--players")
    {
      parsed = ParseNumber(value, _out.players);
    }
    else if (argument == "--churners")
    {
      parsed = ParseNumber(value, _out.churners);
    }
    else if (argument == "--flooders")
    {
      parsed = ParseNumber(value, _out.flooders);
    }
    else if (argument == "--flood-rate")
    {
      parsed = ParseNumber(value, _out.floodRate);
    }
    else if (argument == "--seed")
    {
      parsed = ParseNumber(value, _out.seed);
    }
    else if (argument == "--ticks")
    {
      parsed = ParseNumber(value, _out.ticks);
    }
    else
    {
      parsed = false;
    }

    if (!parsed)
    {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::uint64_t MillisecondsSince(std::chrono::steady_clock::time_point _start) noexcept
{
  const auto elapsed = std::chrono::steady_clock::now() - _start;
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

/// Sends one datagram and tells the report whether the transport took it.
void Send(Bot& _bot, Outpost::StressReport& _report, std::span<const std::byte> _bytes, std::uint32_t _commands)
{
  const bool sent = _bot.transport.Send(_bytes);
  _report.NoteSent(_bot.index, sent ? 1u : 0u, sent ? _commands : 0u, sent ? 0u : 1u);
}

void SendJoin(Bot& _bot, Outpost::StressReport& _report)
{
  std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES + Outpost::Join::SIZE_BYTES> outgoing{};
  Neuron::ByteWriter writer{outgoing};
  if (Outpost::Encode(_bot.frame.CurrentJoin().Outgoing(), writer))
  {
    Send(_bot, _report, std::span<const std::byte>{outgoing.data(), writer.WrittenBytes()}, 0);
  }
}

/// A command packet: the view, and every order the policy still holds, oldest first (ADR-003). With no
/// policy or nothing outstanding it is the empty view report ADR-024 asks for.
void SendCommands(Bot& _bot, Outpost::StressReport& _report)
{
  Outpost::CommandPacket packet{.sequence = 0, .player = _bot.frame.Player(), .commands = {}};
  _bot.frame.StampView(packet);
  if (_bot.policy.has_value())
  {
    static_cast<void>(Outpost::FillOldestFirst(_bot.policy->Outstanding(), Outpost::UPDATE_PAYLOAD_BYTES, packet));
  }

  std::array<std::byte, QUEUE_SLOT_BYTES> outgoing{};
  Neuron::ByteWriter writer{outgoing};
  if (Outpost::Encode(packet, writer))
  {
    Send(_bot, _report, std::span<const std::byte>{outgoing.data(), writer.WrittenBytes()},
         static_cast<std::uint32_t>(packet.commands.size()));
  }
}

/// One poll of one bot: send what is due, drain, and hand the result to whatever decides.
void Poll(Bot& _bot, Outpost::StressReport& _report, const Arguments& _arguments, std::uint64_t _nowMilliseconds)
{
  const Neuron::TransportState state = _bot.transport.State();
  if (!_bot.away && _bot.recovery.ShouldReopen(state, _nowMilliseconds))
  {
    _bot.transport.Close();
    static_cast<void>(_bot.transport.Open(_arguments.host, static_cast<std::uint16_t>(_arguments.port)));
    _bot.frame.MutableJoin().Rejoin();
  }

  const bool ready = (state == Neuron::TransportState::Ready);
  if (ready && (_bot.role != Outpost::BotRole::Flooder))
  {
    if (_bot.frame.MutableJoin().ShouldSend(_nowMilliseconds))
    {
      SendJoin(_bot, _report);
    }
    if (_bot.frame.ShouldReportView(_nowMilliseconds))
    {
      SendCommands(_bot, _report);
    }
  }

  const Outpost::ClientFrame::DrainResult drained = _bot.frame.DrainPackets(_bot.queue, _nowMilliseconds);
  _report.NoteDrain(_bot.index, drained, _bot.frame.Replicas());
  if (drained.joinReplies > 0)
  {
    _report.NoteJoin(_bot.index, _bot.frame.CurrentJoin());
    if (_bot.flood.has_value())
    {
      const Outpost::JoinPhase phase = _bot.frame.CurrentJoin().Phase();
      _bot.flood->NoteJoinReply((phase == Outpost::JoinPhase::Joined) ? Outpost::JoinResult::Accepted : Outpost::JoinResult::MatchFull);
    }
  }

  if (!_bot.policy.has_value())
  {
    return;
  }
  if (drained.linkLost)
  {
    _bot.policy->Reset();
  }

  if (const Outpost::PlayerBlock* own = _bot.frame.Replicas().Own(); own != nullptr)
  {
    std::vector<std::uint64_t> waits;
    static_cast<void>(_bot.policy->Acknowledge(own->lastCommandSequenceApplied, _nowMilliseconds, waits));
    for (const std::uint64_t wait : waits)
    {
      _report.NoteAcknowledged(_bot.index, wait);
    }
  }

  if (_bot.frame.CurrentJoin().IsJoined() && !_bot.policy->Decide(_bot.frame.Replicas(), _bot.frame.Player(), _nowMilliseconds).empty() &&
      ready)
  {
    SendCommands(_bot, _report);
  }
}

/// What happens once a harness tick: the churn and the flood.
void Tick(Bot& _bot, Outpost::StressReport& _report, const Arguments& _arguments, std::uint64_t _harnessTick,
          std::vector<Outpost::FloodDatagram>& _scratch)
{
  if (_bot.churn.has_value())
  {
    switch (_bot.churn->Advance(_harnessTick, _bot.frame.CurrentJoin().IsJoined()))
    {
    case Outpost::ChurnAction::Drop:
      _bot.transport.Close();
      _bot.away = true;
      break;
    case Outpost::ChurnAction::Rejoin:
      // A NEW SOCKET IS A NEW ENDPOINT, and the token the join state holds is what gets the same seat at it.
      _bot.frame.MutableJoin().Rejoin();
      _report.NoteRejoin(_bot.index);
      _bot.away = false;
      static_cast<void>(_bot.transport.Open(_arguments.host, static_cast<std::uint16_t>(_arguments.port)));
      break;
    case Outpost::ChurnAction::None:
      break;
    }
  }

  if (_bot.flood.has_value() && (_bot.transport.State() == Neuron::TransportState::Ready))
  {
    _scratch.clear();
    _bot.flood->Produce(_scratch);
    for (const Outpost::FloodDatagram& datagram : _scratch)
    {
      Send(_bot, _report, datagram.bytes, 0);
    }
  }
}
} // namespace

int main(int _argc, char** _argv)
{
  Arguments arguments;
  if (!Parse(std::span<char*>{_argv, static_cast<std::size_t>(_argc)}, arguments))
  {
    std::fputs(USAGE, stderr);
    return 2;
  }

  const std::uint64_t total = std::uint64_t{arguments.players} + arguments.churners + arguments.flooders;
  if ((total == 0) || (total > Outpost::HARNESS_BOT_CEILING))
  {
    std::fprintf(stderr, "bot: %llu bots; one process runs 1 to %u (ADR-022: its own ceiling is not the host's)\n",
                 static_cast<unsigned long long>(total), Outpost::HARNESS_BOT_CEILING);
    return 2;
  }

  // Multithreaded: a console has no dispatcher, and `DatagramSocket` delivers on the thread pool regardless.
  winrt::init_apartment(winrt::apartment_type::multi_threaded);

  std::vector<Outpost::BotRole> roles;
  std::vector<std::unique_ptr<Bot>> bots;
  const auto addBots = [&](Outpost::BotRole _role, std::uint32_t _count)
  {
    for (std::uint32_t count = 0; count < _count; ++count)
    {
      auto bot = std::make_unique<Bot>();
      bot->role = _role;
      bot->index = static_cast<std::uint32_t>(bots.size());
      switch (_role)
      {
      case Outpost::BotRole::Player:
        bot->policy.emplace(arguments.seed, bot->index);
        break;
      case Outpost::BotRole::Churner:
        bot->churn.emplace(arguments.seed, bot->index);
        break;
      case Outpost::BotRole::Flooder:
        bot->flood.emplace(arguments.seed, bot->index, arguments.floodRate);
        break;
      }
      bot->frame.MutableJoin().Begin(Outpost::NO_SESSION_TOKEN);
      roles.push_back(_role);
      bots.push_back(std::move(bot));
    }
  };
  addBots(Outpost::BotRole::Player, arguments.players);
  addBots(Outpost::BotRole::Churner, arguments.churners);
  addBots(Outpost::BotRole::Flooder, arguments.flooders);

  Outpost::StressReport report{roles};
  for (const std::unique_ptr<Bot>& bot : bots)
  {
    if (!bot->transport.Open(arguments.host, static_cast<std::uint16_t>(arguments.port)))
    {
      std::fprintf(stderr, "bot: socket %u could not be created at all\n", bot->index);
      return 1;
    }
  }

  std::printf("bot: %u players, %u churners, %u flooders against %s:%u for %llu ticks, seed %llu\n", arguments.players, arguments.churners,
              arguments.flooders, arguments.host.c_str(), arguments.port, static_cast<unsigned long long>(arguments.ticks),
              static_cast<unsigned long long>(arguments.seed));
  std::fflush(stdout);

  const auto start = std::chrono::steady_clock::now();
  std::uint64_t harnessTick = 0;
  bool joinsEnabled = false;
  bool warnedFull = false;
  std::vector<Outpost::FloodDatagram> scratch;

  while (harnessTick < arguments.ticks)
  {
    const std::uint64_t now = MillisecondsSince(start);
    for (const std::unique_ptr<Bot>& bot : bots)
    {
      Poll(*bot, report, arguments, now);
    }

    const std::uint64_t due = now / Outpost::HARNESS_TICK_MILLISECONDS;
    if (due > harnessTick)
    {
      harnessTick = due;
      for (const std::unique_ptr<Bot>& bot : bots)
      {
        Tick(*bot, report, arguments, harnessTick, scratch);
      }
    }

    // THE FLOODERS' JOINS WAIT FOR A FULL MATCH: every seat this run needs taken, so a flooder's join is
    // refused rather than seated (`FloodSchedule`).
    bool everyoneSeated = true;
    for (const std::unique_ptr<Bot>& bot : bots)
    {
      if ((bot->role != Outpost::BotRole::Flooder) && !bot->frame.CurrentJoin().IsJoined())
      {
        everyoneSeated = false;
      }
      if ((bot->role != Outpost::BotRole::Flooder) && (bot->frame.CurrentJoin().Phase() == Outpost::JoinPhase::Refused) && !warnedFull)
      {
        std::printf("bot: the host refused a seat -- every seat an earlier run took is still held; restart the host\n");
        warnedFull = true;
      }
    }
    if (everyoneSeated && !joinsEnabled)
    {
      for (const std::unique_ptr<Bot>& bot : bots)
      {
        if (bot->flood.has_value())
        {
          bot->flood->EnableJoins();
        }
      }
      joinsEnabled = true;
    }

    std::this_thread::sleep_for(POLL_INTERVAL);
  }

  for (const std::unique_ptr<Bot>& bot : bots)
  {
    if (bot->flood.has_value() && bot->flood->WasSeated())
    {
      std::printf("bot: flooder %u was SEATED -- the host has more seats than this run's players and churners\n", bot->index);
    }
    bot->transport.Close();
  }

  std::fputs(report.Format(harnessTick).c_str(), stdout);
  return 0;
}
