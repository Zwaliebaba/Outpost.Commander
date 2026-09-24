#include "pch.h"

#include "Host.h"

#include "Tick.h"

#include <array>

namespace Outpost
{

namespace
{
/// Comfortably larger than any datagram this build sends or accepts, so a receive is never
/// measuring the buffer instead of the packet.
inline constexpr std::size_t SCRATCH_BYTES = 2048;
} // namespace

PlayerBlock PlayerBlockFor(const CommandIntake& _intake, const BuildSystem& _build, PlayerId _player) noexcept
{
  return PlayerBlock{.credits = _build.Credits(_player),
                     .lastCommandSequenceApplied = _intake.LastAppliedSequence(_player),
                     .buildingDesign = _build.WireBuildingDesign(_player),
                     .buildProgressPercent = _build.WireProgressPercent(_player)};
}

Host::Host()
{
  BeginMatch(DEFAULT_MATCH_SEED);
}

void Host::BeginMatch(std::uint64_t _matchSeed, std::size_t _playerCount)
{
  // A RUNTIME VALUE EVERYWHERE IT MATTERS (Q27, ADR-023): an update carries only its recipient's block, so
  // nothing on the wire is sized by it (ADR-024), and every table is sized to the capacity. Raising it is
  // configuration and not a format change.
  const std::size_t players = (_playerCount > MAX_PLAYERS) ? MAX_PLAYERS : _playerCount;
  m_sessions.Begin(players, _matchSeed);
  m_sessions.ReserveSeats(m_aiSeats);
  m_intake = CommandIntake{};
  m_matchEndedRepeats = 0;
  ResetMatch(_matchSeed, players);
}

void Host::SetAiSeats(std::size_t _count) noexcept
{
  m_aiSeats = _count;
  m_sessions.ReserveSeats(_count);
  m_ai.Begin(m_sessions.PlayerCount(), _count);
}

void Host::Restart()
{
  // THE RESULT FIRST, from the match that just ended, before the reset forgets it.
  const MatchOutcome& outcome = m_victory.Outcome();
  m_lastEnded = MatchEnded{
    .matchNumber = static_cast<std::uint16_t>(m_lastEnded.matchNumber + 1), .winner = outcome.winner, .onClock = outcome.onClock};
  m_matchEndedRepeats = MATCH_ENDED_REPEAT_TICKS;

  const std::uint64_t seed = NextMatchSeed(m_sessions.MatchSeed());
  m_sessions.Reseed(seed);
  ResetMatch(seed, m_sessions.PlayerCount());
}

std::uint64_t Host::NextMatchSeed(std::uint64_t _matchSeed) noexcept
{
  std::uint64_t mixed = _matchSeed + 0x9E3779B97F4A7C15ull;
  mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ull;
  mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBull;
  return mixed ^ (mixed >> 31);
}

void Host::ResetMatch(std::uint64_t _matchSeed, std::size_t _players)
{
  // **THE COMMAND INTAKE IS NOT RESET HERE.** Its sequences are the seat's, and a restart keeps the seats: a client
  // carries on numbering its commands across the end of a match, and the host goes on acknowledging them.
  const std::size_t players = _players;
  m_world = World{};
  m_build.Begin(players);
  m_economy.Begin();
  m_victory.Begin(players, m_tick);
  m_ai.Begin(players, m_aiSeats);

  // EVERY CLIENT STARTS FROM NOTHING: the new match's entities are all due, and a client clears its store when it
  // hears the match ended (ADR-024).
  m_accumulator.Begin();

  // M2.6: THE FIELD, FROM THE SAME FUNCTION THE CLIENT CALLS (R23) -- the host's half, which until mining
  // needed a rock to go to had nothing to do with it. Asteroids are not entities before M3 (Q22), so the
  // rows sit beside the store and a mine order names one by index (Q52).
  m_world.SetField(GenerateField(_matchSeed, players));

  // M1.5: the stations, from `GameCore`'s generator -- the same function the client runs to draw the
  // same field (R23). A station needs no code of its own here because it is a row in the design
  // table (ADR-006), so this is `World::Create` like anything else.
  for (const Placement& placed : GenerateLayout(_matchSeed, players))
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

  // **A SEATED CLIENT STARTS FROM NOTHING** (ADR-024). It clears its store on a join or a rejoin, so the
  // accumulator forgets what it had sent it and everything is due within one sweep. A repeat of a join
  // already answered resets it again, which costs one sweep of records and nothing else.
  if ((reply.result == JoinResult::Accepted) || (reply.result == JoinResult::Rejoined))
  {
    m_accumulator.Reset(reply.player);
  }

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
    // twice -- once here and once inside whichever decoder takes it -- which is four bytes.
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

    // THE VIEW RIDES EVERY COMMAND PACKET, INCLUDING AN EMPTY ONE (ADR-024). The client sends one on a
    // cadence for exactly this, and a lost one leaves the accumulator scoring against the last it saw.
    m_accumulator.SetView(seated, packet.viewX, packet.viewY, packet.viewRadiusUnits);

    static_cast<void>(m_intake.ApplyPacket(m_world, m_build, packet));
  }
}

void Host::SendUpdates()
{
  std::array<std::byte, SCRATCH_BYTES> scratch{};

  // **THE END OF A MATCH, AHEAD OF THE NEXT MATCH'S UPDATES**, to every seat, for ten ticks (Q70).
  if (m_matchEndedRepeats > 0)
  {
    --m_matchEndedRepeats;
    for (const Sessions::Session& session : m_sessions.All())
    {
      Neuron::ByteWriter writer{scratch};
      if (Encode(m_lastEnded, writer))
      {
        static_cast<void>(m_transport.Send(session.endpoint, std::span<const std::byte>{scratch.data(), writer.WrittenBytes()}));
      }
    }
  }

  // **TO SESSIONS, NOT TO ENDPOINTS THAT HAVE SPOKEN** (ADR-013). A client that has not joined sees
  // nothing at all. Each seated client gets its own updates: its own block, its own sequence, and the
  // records its own accumulator says are due (ADR-024).
  for (const Sessions::Session& session : m_sessions.All())
  {
    const PlayerBlock own = PlayerBlockFor(m_intake, m_build, session.player);
    for (const Update& update : m_accumulator.Fill(m_world, session.player, own, m_tick))
    {
      Neuron::ByteWriter writer{scratch};

      // `Encode` refuses an update past the pinned payload, so what reaches the socket is one datagram
      // by construction rather than by a check here. A refusal is a bug in the accumulator's arithmetic
      // and is dropped rather than sent broken.
      if (!Encode(update, writer))
      {
        continue;
      }
      static_cast<void>(m_transport.Send(session.endpoint, std::span<const std::byte>{scratch.data(), writer.WrittenBytes()}));
      ++m_updatesSent;
    }
  }
}

void Host::RunOneTick()
{
  DrainAndApply();

  // M3.10: THE AI SEATS' ORDERS, AFTER THE CLIENTS' AND THROUGH THE SAME INTAKE (`TechnicalDesign.md` section 2:
  // orders, AI, movement), once a second, from what a client of their seat would be sent (Q48).
  m_ai.Advance(m_world, m_build, m_intake, m_tick);
  Tick(m_world);

  // M3.2: WEAPONS, AFTER MOVEMENT AND BEFORE MINING (`TechnicalDesign.md` section 2), so a ship fires from where it
  // moved to this tick. Every fire event rides every client's next updates (ADR-024).
  m_weapons.Advance(m_world, m_tick);
  for (const FireEvent& fire : m_weapons.Fired())
  {
    m_accumulator.NoteFire(fire);
  }

  // M2.6: MINING, AFTER MOVEMENT AND BEFORE BUILD QUEUES (`TechnicalDesign.md` section 2) -- and M2.7's
  // credits from what it delivered, before the build queue spends them.
  m_mining.Advance(m_world, m_weapons.Struck());
  m_economy.Credit(m_mining.Deliveries(), m_world, m_build);

  // AFTER THE MOVEMENT, so a ship that appears this tick does not also move on it -- which would
  // put it somewhere no update ever said it started from.
  m_build.Advance(m_world);

  // M3.4: DEATHS, LAST BEFORE VICTORY (`TechnicalDesign.md` section 2). Everything at zero hull goes at once, so
  // what died this tick is one list M3.7 reads, whatever the systems above did with it on the way.
  m_deaths.Advance(m_world);

  // M3.7: ELIMINATION AND VICTORY, LAST (`TechnicalDesign.md` section 2). A player whose station died above loses
  // everything else on this same tick, and the accumulator sends those removals like any death.
  m_victory.Advance(m_world, m_build, m_tick);

  ++m_tick;

  // M3.8: A MATCH THAT ENDED THIS TICK IS REPLACED BEFORE ANYTHING IS SENT, so the first updates after the end are
  // already the next match's, and the `MatchEnded` goes out ahead of them.
  if (m_victory.Outcome().over)
  {
    Restart();
  }
  SendUpdates();
}

} // namespace Outpost
