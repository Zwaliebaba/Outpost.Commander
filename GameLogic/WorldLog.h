#pragma once

namespace Outpost
{
// The world's log, in its folder beside its saves (ADR-082).
inline constexpr std::string_view WORLD_LOG_FILE = "World.log";

// Who plays a seat in a tick of a world (Phase 5 design §10): its player, its deputy while the player is away (ADR-079), or
// the AI, when the seat is an AI empire's.
enum class SeatPlay : std::uint8_t
{
  Player,
  Deputy,
  Ai
};

// One seat after a tick: its player, who played it, and the snapshot the server built for it.
struct SeatTick
{
  PlayerId player;
  SeatPlay play = SeatPlay::Player;
  const Snapshot* snapshot = nullptr;
};

// A world's log (Phase 5 design §10, ADR-082): ADR-038's match log for a world that keeps running, written by its server
// beside its saves, for the owner's week (W1–W6) and the world run. It reads what the server knows after each tick: each
// seat's snapshot, and who played it.
//
// One line a record, every time in ticks:
//
//   start <tick> new                               a new world, with its bases placed
//   start <tick> recovered save <tick> replayed <n>
//                                                  the server came back from the save of that tick and the n commands
//                                                  logged after it (ADR-077)
//   save <tick> bytes <n> encode_us <us>           a save, its size, and how long encoding it took the server's thread
//   seat <tick> player <p> <player, deputy or ai>  who plays the seat, when it changes, and from the first tick
//   order <tick> player <p> order <id> <action> <given, held or refused>
//                                                  a scheduled order fired (ADR-080)
//   lost <tick> player <p>                         the player lost its production (ADR-084)
//   waiting <tick> player <p>                      its hour has passed and its start is not free
//   restart <tick> player <p>                      its seat restarted at its start
//   hour <tick> player <p> ore <n> income <hundredths a second> fleet <command points> cap <n> nodes <n> researched <n>
//        present <s> deputy <s>                    every hour of ticks: the seat's bank, income, fleet against its cap,
//                                                  nodes held and topics researched, and the seconds of the hour its
//                                                  player and its deputy played it (design §9)
//   battle <start> <end> sector <id> player <p> <play> player <p> <play>
//                                                  two seats' ships firing at each other in one sector within 10 seconds,
//                                                  until 30 seconds pass with no such shot there, as the match log's
//                                                  engagement; each side is present when its player played it at any of
//                                                  its shots in the battle, and otherwise its deputy or the AI fought it. A
//                                                  sector of 0 is none.
class WorldLog : Neuron::NonCopyable
{
public:
  static constexpr std::uint32_t BATTLE_WINDOW_SECONDS = 10;
  static constexpr std::uint32_t BATTLE_GAP_SECONDS = 30;
  static constexpr std::uint32_t HOUR_SECONDS = 3600;

  WorldLog(std::ostream& _out, std::uint32_t _ticksPerSecond);

  void Started(std::uint64_t _tick, std::optional<std::uint64_t> _saveTick, std::size_t _replayed);
  void Saved(std::uint64_t _tick, std::size_t _bytes, std::chrono::microseconds _encode);
  // After each tick _tick: every seat, each with its own snapshot of the tick.
  void Record(std::uint64_t _tick, std::span<const SeatTick> _seats);

private:
  struct SeatState
  {
    PlayerId player;
    std::optional<SeatPlay> play;
    bool waiting = false;
    std::uint64_t playerTicks = 0;
    std::uint64_t deputyTicks = 0;
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
  void RecordShots(std::uint64_t _tick, std::span<const SeatTick> _seats);
  void EndBattles(std::uint64_t _tick);

  std::ostream* m_out;
  std::uint32_t m_ticksPerSecond = 0;
  std::vector<SeatState> m_seats;
  std::vector<Battle> m_battles;
};
} // namespace Outpost
