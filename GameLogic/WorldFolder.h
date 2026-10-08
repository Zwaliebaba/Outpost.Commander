#pragma once

namespace Outpost
{
// A command as the server applied it: the tick it was applied at the start of, and the order with its player as the
// connection set it. A seed and this log reproduce the match on the same build (ADR-009), and a world's save and the log
// since it reproduce the world (ADR-077).
struct LoggedCommand
{
  std::uint64_t tick = 0;
  Command command;
};

// How often a world is saved, in seconds of its ticks (Phase 5 design §5, gate H7).
inline constexpr std::uint32_t WORLD_SAVE_SECONDS = 60;

// A world's folder (ADR-077): its saves, the newest WorldFolder::SAVES_KEPT of them, and its command log, a file begun at
// each save. A thread of the folder's own writes each save to a new file and renames it into place, so a killed process
// never leaves half a save; the command log is written by the server's thread as each tick applies it.
class WorldFolder : Neuron::NonCopyable
{
public:
  static constexpr std::size_t SAVES_KEPT = 3;

  // What a folder holds to come back from: the newest save that reads whole, and every command logged from its tick on,
  // in the order the server applied them.
  struct Recovery
  {
    std::vector<std::byte> save;
    SaveHeader header;
    std::vector<LoggedCommand> commands;
  };

  // The newest save in _folder that reads whole, and the commands logged since it; none when the folder holds no save.
  // A save that does not read whole, such as one cut short, is passed over for the one before it, and so is a log's last
  // record cut short, which only a killed process leaves. Throws Neuron::Exception when the folder holds saves and none
  // reads whole, or a log is broken anywhere else.
  [[nodiscard]] static std::optional<Recovery> Recover(const std::filesystem::path& _folder);

  // Takes _folder, which it makes if it does not exist. Saves and the log go there from here on.
  explicit WorldFolder(std::filesystem::path _folder);
  // Writes every save handed over that is not written yet, and closes the log.
  ~WorldFolder();

  // Hands a save made at _tick to the writer's thread and returns at once, and begins a new log file from _tick, which
  // the ticks from then on are logged to. A save of a tick the folder holds already is that save, and is not written
  // again. Throws, on the caller's thread, what the writer's thread met with an earlier save.
  void Save(std::uint64_t _tick, std::vector<std::byte> _save);

  // Appends the commands applied at the start of _tick to the log, and hands them to the system before it returns, so
  // that a killed process keeps them. Nothing is written for none.
  void Log(std::uint64_t _tick, std::span<const Command> _commands);

  // Waits until every save handed over is written. Throws what the writer's thread met.
  void Flush();

private:
  // The writer's thread: it writes each save handed over, oldest first, and drops the saves and logs no longer kept.
  void Write(const std::stop_token& _stop);
  void OpenLog(std::uint64_t _tick);

  std::filesystem::path m_folder;
  std::ofstream m_log;
  // The saves handed over and not yet written, guarded by m_mutex, and what the writer's thread met.
  std::mutex m_mutex;
  std::condition_variable_any m_changed;
  std::deque<std::pair<std::uint64_t, std::vector<std::byte>>> m_pending;
  bool m_writing = false;
  std::exception_ptr m_failure;
  // Last, so that it is stopped and joined before anything it uses goes.
  std::jthread m_writer;
};
} // namespace Outpost
