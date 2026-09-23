#pragma once

#include "GameCore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// What a flooder sends. **Every one is refused by the host, and each by a different path** (ADR-022).
enum class FloodKind : std::uint8_t
{
  /// A well-formed `Join` with no token. `Sessions::Admit` answers `MatchFull` -- but only once the match is
  /// full, which is why this kind waits for `EnableJoins`.
  JoinPastFull,
  /// A well-formed command packet from an endpoint the host never seated. It decodes cleanly and is refused
  /// by the session table, not by a decoder: the teeth of ADR-013.
  UnseatedCommand,
  /// Fewer bytes than a packet header. `PacketFault::Truncated`.
  TruncatedHeader,
  /// A header naming another protocol version. `PacketFault::VersionMismatch`.
  WrongVersion,
  /// A command packet whose count the rest of the datagram cannot hold. `CommandFault::Malformed`, before
  /// anything is reserved from the count.
  ImpossibleCount
};

inline constexpr std::size_t FLOOD_KIND_COUNT = 5;

/// One datagram to send, and what it is. R8: a public aggregate.
struct FloodDatagram
{
  FloodKind kind = FloodKind::TruncatedHeader;
  std::vector<std::byte> bytes;
};

/// **WHICH REFUSABLE DATAGRAM A FLOODER SENDS ON WHICH HARNESS TICK** (ADR-022).
///
/// **THE MALFORMED ONES ARE BUILT BY HAND**, through `Neuron::ByteWriter` and not through the encoders, because
/// an encoder refuses to write what these are. The well-formed two go through the encoders, since a join and a
/// command that the host refuses for who sent them are exactly what a real client would send.
///
/// **A FLOODER IS NEVER SEATED, AND THIS CLASS IS WHAT KEEPS IT SO.** A join sent while the match has a free
/// slot is not refused -- it takes the slot, and a slot is never given back. So joins go out only once the
/// harness says every player and churner is seated (`EnableJoins`), and the first time the host seats this
/// flooder anyway -- a host started with more seats than the run has players -- it stops sending both kinds
/// that depend on being unseated. That seat is lost for the run, and the report says so.
class FloodSchedule
{
public:
  /// **ONE DATAGRAM A HARNESS TICK BY DEFAULT, THIRTY-TWO AT MOST.** One is twenty a second, which is a
  /// nuisance and not a flood; thirty-two is 640 a second from one flooder, and a run wanting more adds
  /// flooders, which also spreads the load over endpoints the way a real flood would. **The real rate is
  /// bounded lower by the transport**, which refuses a send while the previous one is in flight; the report
  /// counts those rather than hiding them.
  static constexpr std::uint32_t DEFAULT_DATAGRAMS_PER_TICK = 1;
  static constexpr std::uint32_t MAX_DATAGRAMS_PER_TICK = 32;

  /// _datagramsPerTick is clamped to one through `MAX_DATAGRAMS_PER_TICK`.
  FloodSchedule(std::uint64_t _runSeed, std::uint32_t _botIndex, std::uint32_t _datagramsPerTick) noexcept;

  /// Appends one harness tick's datagrams to _outDatagrams; call it once a tick. The kinds rotate from a
  /// seeded start, skipping the two that are not yet or no longer safe, so every kind this flooder may send
  /// is sent every `FLOOD_KIND_COUNT` datagrams and a run of any length covers all of them.
  void Produce(std::vector<FloodDatagram>& _outDatagrams);

  /// Every player and churner in the run is seated, so a join from here is one past a full match.
  void EnableJoins() noexcept
  {
    m_joinsEnabled = true;
  }

  /// The host answered one of this flooder's joins. `MatchFull` is the refusal it was sent for; anything else
  /// seated it, which ends both kinds that need it unseated.
  void NoteJoinReply(JoinResult _result) noexcept;

  [[nodiscard]] bool WasSeated() const noexcept
  {
    return m_wasSeated;
  }

  [[nodiscard]] std::uint32_t DatagramsPerTick() const noexcept
  {
    return m_datagramsPerTick;
  }

  [[nodiscard]] std::uint64_t SentCount(FloodKind _kind) const noexcept
  {
    return m_sent[static_cast<std::size_t>(_kind)];
  }

private:
  [[nodiscard]] bool Allowed(FloodKind _kind) const noexcept;
  [[nodiscard]] FloodDatagram Build(FloodKind _kind);

  Neuron::Pcg32 m_random;
  std::uint32_t m_datagramsPerTick = DEFAULT_DATAGRAMS_PER_TICK;
  std::uint32_t m_nextKind = 0;
  std::uint16_t m_sequence = 0;
  bool m_joinsEnabled = false;
  bool m_wasSeated = false;
  std::array<std::uint64_t, FLOOD_KIND_COUNT> m_sent{};
};

} // namespace Outpost
