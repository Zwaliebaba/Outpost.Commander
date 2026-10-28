#pragma once

namespace Outpost
{
// The universe's log, in its folder beside its saves (ADR-082).
inline constexpr std::string_view UNIVERSE_LOG_FILE = "Universe.log";

// One seat after a tick: its player, who played it, the snapshot the server built for it, its connection over QUIC when the
// server sampled it after this tick (ADR-004 decision 6), and how many of its commands the tick applied, whoever gave them:
// its player, its deputy or the AI.
struct SeatTick
{
  PlayerId player;
  SeatPlay play = SeatPlay::Player;
  const Snapshot* snapshot = nullptr;
  std::optional<Neuron::QuicStatistics> connection;
  std::uint32_t orders = 0;
};

// One join's record: what came of it, the seat's player when it is known, the address the connection came from, the layout
// a hello of another build named (ADR-060 decision 5), and the code the game closed its connection with.
struct JoinRecord
{
  JoinOutcome outcome = JoinOutcome::Welcomed;
  PlayerId player;
  std::string address;
  std::uint64_t layout = 0;
  std::uint64_t code = 0;
};

// What came of a connection to the overseer's listener (ADR-101): it opened the overseer's channel; it closed, by the
// overseer, lost or taken by a newer one; or it was refused, for another build, a token that is not the universe's, a message
// that is no hello, or no hello in time.
enum class OverseerOutcome : std::uint8_t
{
  Opened,
  Closed,
  WrongVersion,
  TokenRefused,
  Malformed,
  Silent
};

// One of the overseer's comings and goings, and the address it came from.
struct OverseerRecord
{
  OverseerOutcome outcome = OverseerOutcome::Opened;
  std::string address;
};

// A universe's log (ADR-082 decision 1): ADR-038's match log for a universe that keeps running, written by its server
// beside its saves, for the owner's week and the universe run. It reads what the server knows after each tick: each
// seat's snapshot, and who played it.
//
// One line a record, every time in ticks:
//
//   start <tick> new                               a new universe, with its bases placed
//   start <tick> recovered save <tick> replayed <n>
//                                                  the server came back from the save of that tick and the n commands
//                                                  logged after it (ADR-077)
//   save <tick> bytes <n> encode_us <us>           a save, its size, and how long encoding it took the server's thread
//   written <tick> write_us <us>                   the save of that tick written by the folder's thread, and how long its
//                                                  write took there, to the system's cache (ADR-077 decision 7)
//   tick <tick> us <us> over <n> [<part> <us>]...  the minute's longest tick, when it took LONG_TICK or more: its time, how
//                                                  many of the minute's ticks took that long, and each part of it that took
//                                                  a quarter of it or more, longest first, by TickPartName
//   dropped <tick> ticks <n>                       the ticks the minute dropped after stalls, which never ran (ADR-009)
//   join <tick> welcomed player <p> from <address> a connection's hello took the seat
//   join <tick> refused <why> [layout <hex>] [player <p>] from <address>
//                                                  a connection refused: version, with the layout its hello named; seat,
//                                                  with the player it named; malformed; or silent, for no hello in time
//   join <tick> gone player <p> <taken, closed <code> or lost> from <address>
//                                                  a seat's connection gone: taken by a newer one, closed by the game with
//                                                  its code, or lost without a word
//   overseer <tick> opened from <address>          the overseer's channel opened, or closed, by the overseer, lost or taken
//   overseer <tick> closed from <address>          by a newer one (ADR-101)
//   overseer <tick> refused <why> from <address>   a connection to the overseer's listener refused: version, token,
//                                                  malformed, or silent for no hello in time
//   seat <tick> player <p> <player, deputy or ai>  who plays the seat, when it changes, and from the first tick
//   order <tick> player <p> order <id> <action> <given, held or refused>
//                                                  a scheduled order fired (ADR-080)
//   lost <tick> player <p>                         the player lost its production (ADR-084)
//   waiting <tick> player <p>                      its hour has passed and its start is not free
//   restart <tick> player <p> waited <ticks>       its seat restarted at its start, that long after its hour; the wait
//                                                  is left out when the server came back in the tick it restarted
//   hour <tick> player <p> ore <n> income <hundredths a second> fleet <command points> cap <n> nodes <n> researched <n>
//        present <s> deputy <s> orders <n> refused <n>
//                                                  every hour of ticks: the seat's bank, income, fleet against its cap,
//                                                  nodes held and topics researched, the seconds of the hour its player and
//                                                  its deputy played it, and the commands the server applied for it and
//                                                  refused it in the hour (ADR-082 decision 3)
//        [rtt_median_us <us> rtt_p95_us <us> sent <n> lost <n> backlog_most <bytes> sent_bytes <bytes>]
//                                                  and, when its player's connection was sampled in the hour, its round
//                                                  trip at the median and the 95th percentile, the packets sent and lost
//                                                  in the hour, its largest backlog, and the bytes sent in the hour
//                                                  (ADR-004 decision 6)
//   battle <start> <end> sector <id> player <p> <play> player <p> <play>
//                                                  two seats' ships firing at each other in one sector within 10 seconds,
//                                                  until 30 seconds pass with no such shot there, as the match log's
//                                                  engagement; each side is present when its player played it at any of
//                                                  its shots in the battle, and otherwise its deputy or the AI fought it. A
//                                                  sector of 0 is none.
class UniverseLog : Neuron::NonCopyable
{
public:
  static constexpr std::uint32_t BATTLE_WINDOW_SECONDS = 10;
  static constexpr std::uint32_t BATTLE_GAP_SECONDS = 30;
  static constexpr std::uint32_t HOUR_SECONDS = 3600;
  static constexpr std::uint32_t MINUTE_SECONDS = 60;
  // A tick this long or longer is a long one: the tick's budget (Docs/Budgets.md).
  static constexpr std::chrono::microseconds LONG_TICK{5'000};

  UniverseLog(std::ostream& _out, std::uint32_t _ticksPerSecond);

  void Started(std::uint64_t _tick, std::optional<std::uint64_t> _saveTick, std::size_t _replayed);
  void Saved(std::uint64_t _tick, std::size_t _bytes, std::chrono::microseconds _encode);
  // The save of _saveTick, written by the folder's thread in _write.
  void Written(std::uint64_t _saveTick, std::chrono::microseconds _write);
  // After each tick _tick: every seat, each with its own snapshot of the tick.
  void Record(std::uint64_t _tick, std::span<const SeatTick> _seats);
  // After each tick _tick, last: how long it took, which ends the minute's records when _tick ends a minute.
  void Timed(std::uint64_t _tick, const TickTiming& _timing);
  // Ticks the server dropped after a stall, before its next tick, which the minute's records sum.
  void Dropped(std::uint64_t _ticks);
  // What came of a connection, at _tick.
  void Joined(std::uint64_t _tick, const JoinRecord& _join);
  // The overseer's coming or going, at _tick.
  void Oversaw(std::uint64_t _tick, const OverseerRecord& _record);

  // From here on, keeps each line it writes until TakeLines, for the overseer's stream (ADR-101).
  void KeepLines() noexcept;
  // The lines written since the last call, oldest first.
  [[nodiscard]] std::vector<std::string> TakeLines() noexcept;
  // The battles under way, each with the seats fighting in it.
  [[nodiscard]] std::vector<BattleHealth> Battles() const;

  // The last _count lines of the log in _file, oldest first, read from its end; none when it cannot be read.
  [[nodiscard]] static std::vector<std::string> ReadLastRecords(const std::filesystem::path& _file, std::size_t _count);

private:
  struct SeatState
  {
    PlayerId player;
    std::optional<SeatPlay> play;
    bool waiting = false;
    // The tick the seat's last snapshot said it restarts at, while it has lost.
    std::optional<std::uint64_t> restartTick;
    std::uint64_t playerTicks = 0;
    std::uint64_t deputyTicks = 0;
    // The hour's commands applied for the seat, and refused it.
    std::uint64_t orders = 0;
    std::uint64_t refused = 0;
    // The hour's samples of the seat's connection: the round trips, the packets sent and lost, the largest backlog and the
    // bytes sent; and the counts the last sample read, since each connection counts from its own start.
    std::vector<std::uint32_t> roundTripsMicroseconds;
    std::uint64_t packetsSent = 0;
    std::uint64_t packetsLost = 0;
    std::uint64_t backlogMostBytes = 0;
    std::uint64_t bytesSent = 0;
    std::uint64_t lastPacketsSent = 0;
    std::uint64_t lastPacketsLost = 0;
    std::uint64_t lastBytesSent = 0;
  };

  // A sector's fire: the last tick each seat fired at another there, and the battle under way, if any.
  struct Battle
  {
    std::int32_t sector = 0;
    std::vector<std::pair<PlayerId, std::uint64_t>> lastFire;
    std::optional<std::uint64_t> start;
    std::uint64_t lastShot = 0;
    // The seats that fought in it, and whether each was present for any of it.
    std::vector<std::pair<PlayerId, SeatPlay>> sides;
  };

  void Write(const std::string& _line);
  void RecordSeat(std::uint64_t _tick, const SeatTick& _seat);
  static void RecordConnection(SeatState& _state, const Neuron::QuicStatistics& _connection);
  [[nodiscard]] static std::string ConnectionWords(SeatState& _state);
  void RecordShots(std::uint64_t _tick, std::span<const SeatTick> _seats);
  void EndBattles(std::uint64_t _tick);
  // Writes the minute's longest tick and its dropped ticks, when it had either, at _tick, the minute's last.
  void EndMinute(std::uint64_t _tick);

  std::ostream* m_out;
  std::uint32_t m_ticksPerSecond = 0;
  std::vector<SeatState> m_seats;
  std::vector<Battle> m_battles;
  // The minute's longest tick that took LONG_TICK or more, and its tick; how many took that long; and the ticks dropped.
  std::optional<std::pair<std::uint64_t, TickTiming>> m_longest;
  std::uint32_t m_longTicks = 0;
  std::uint64_t m_droppedTicks = 0;
  // The lines written since TakeLines, once KeepLines has asked for them.
  bool m_keepLines = false;
  std::vector<std::string> m_lines;
};
} // namespace Outpost
