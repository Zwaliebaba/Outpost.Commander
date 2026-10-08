#include "pch.h"
#include "WorldFolder.h"

#include <algorithm>
#include <charconv>
#include <ranges>

namespace
{
// A save's file and a log's are named for the tick they begin at, with the tick written out to twenty digits, so that the
// names sort in tick order.
constexpr std::string_view SAVE_PREFIX = "World-";
constexpr std::string_view SAVE_EXTENSION = ".save";
constexpr std::string_view LOG_PREFIX = "Commands-";
constexpr std::string_view LOG_EXTENSION = ".log";
// A save is written under this name first, and renamed once whole.
constexpr std::string_view PARTIAL_EXTENSION = ".partial";
// A log record: the tick, the length of the command's bytes, and the bytes, which are the wire's (ADR-060).
constexpr std::size_t RECORD_HEADER_BYTES = sizeof(std::uint64_t) + sizeof(std::uint32_t);

std::string FileName(std::string_view _prefix, std::uint64_t _tick, std::string_view _extension)
{
  return std::format("{}{:020}{}", _prefix, _tick, _extension);
}

// The files in _folder named _prefix, a tick and _extension, by tick, oldest first.
std::vector<std::pair<std::uint64_t, std::filesystem::path>> FilesOf(const std::filesystem::path& _folder, std::string_view _prefix,
                                                                     std::string_view _extension)
{
  std::vector<std::pair<std::uint64_t, std::filesystem::path>> files;
  for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(_folder))
  {
    const std::string name = entry.path().filename().string();
    if (!entry.is_regular_file() || name.size() != _prefix.size() + 20 + _extension.size() || !name.starts_with(_prefix) ||
        !name.ends_with(_extension))
    {
      continue;
    }
    std::uint64_t tick = 0;
    const char* digits = name.data() + _prefix.size();
    if (std::from_chars(digits, digits + 20, tick).ptr == digits + 20)
      files.emplace_back(tick, entry.path());
  }
  std::ranges::sort(files);
  return files;
}

std::vector<std::byte> ReadWhole(const std::filesystem::path& _path)
{
  std::ifstream file(_path, std::ios::binary);
  if (!file)
    throw Neuron::Exception(std::format("The world's file {} cannot be read.", _path.string()));
  const std::vector<char> characters{std::istreambuf_iterator<char>(file), {}};
  std::vector<std::byte> bytes(characters.size());
  std::ranges::transform(characters, bytes.begin(), [](char _character) { return static_cast<std::byte>(_character); });
  return bytes;
}

template <std::unsigned_integral T> T LittleEndian(std::span<const std::byte> _bytes) noexcept
{
  T value = 0;
  for (std::size_t i = 0; i < sizeof(T); ++i)
    value = static_cast<T>(value | (static_cast<T>(_bytes[i]) << (8 * i)));
  return value;
}

// Appends the commands of one log file, from _fromTick on, to _commands. A record cut short at the end is ignored when
// _last says the file is the newest, which only a killed process leaves; anywhere else it is a broken log.
void ReadLog(const std::filesystem::path& _path, std::uint64_t _fromTick, bool _last, std::vector<Outpost::LoggedCommand>& _commands)
{
  const std::vector<std::byte> bytes = ReadWhole(_path);
  std::span<const std::byte> left(bytes);
  while (!left.empty())
  {
    const auto broken = [&_path](std::string_view _why)
    { return Neuron::Exception(std::format("The world's command log {} is broken: {}.", _path.string(), _why)); };
    if (left.size() < RECORD_HEADER_BYTES)
    {
      if (_last)
        return;
      throw broken("its last record is cut short");
    }
    const auto tick = LittleEndian<std::uint64_t>(left);
    const auto length = LittleEndian<std::uint32_t>(left.subspan(sizeof(std::uint64_t)));
    if (left.size() - RECORD_HEADER_BYTES < length)
    {
      if (_last)
        return;
      throw broken("its last record is cut short");
    }
    const Outpost::Message message = Outpost::DecodeMessage(left.subspan(RECORD_HEADER_BYTES, length));
    const Outpost::Command* command = std::get_if<Outpost::Command>(&message);
    if (command == nullptr)
      throw broken("a record holds no command");
    if (!_commands.empty() && tick < _commands.back().tick)
      throw broken("its ticks go back");
    if (tick >= _fromTick)
      _commands.push_back({tick, *command});
    left = left.subspan(RECORD_HEADER_BYTES + length);
  }
}
} // namespace

std::optional<Outpost::WorldFolder::Recovery> Outpost::WorldFolder::Recover(const std::filesystem::path& _folder)
{
  if (!std::filesystem::exists(_folder))
    return std::nullopt;
  const auto saves = FilesOf(_folder, SAVE_PREFIX, SAVE_EXTENSION);
  if (saves.empty())
    return std::nullopt;
  std::string refused;
  for (const auto& [tick, path] : saves | std::views::reverse)
  {
    Recovery recovery;
    try
    {
      recovery.save = ReadWhole(path);
      recovery.header = ReadSaveHeader(recovery.save);
    }
    catch (const Neuron::Exception& exception)
    {
      refused += std::format(" {}: {}", path.filename().string(), exception.what());
      continue;
    }
    // Every log from this save's on: each begins at a save, and the newest may end in a record cut short.
    const auto logs = FilesOf(_folder, LOG_PREFIX, LOG_EXTENSION);
    for (std::size_t i = 0; i < logs.size(); ++i)
    {
      if (logs[i].first >= recovery.header.tick)
        ReadLog(logs[i].second, recovery.header.tick, i + 1 == logs.size(), recovery.commands);
    }
    return recovery;
  }
  throw Neuron::Exception(std::format("The world in {} has no save that reads whole:{}", _folder.string(), refused));
}

Outpost::WorldFolder::WorldFolder(std::filesystem::path _folder)
  : m_folder(std::move(_folder))
{
  std::filesystem::create_directories(m_folder);
  m_writer = std::jthread([this](const std::stop_token& _stop) { Write(_stop); });
}

// The writer's thread is stopped and joined first, as the last member: it writes what is pending before it stops.
Outpost::WorldFolder::~WorldFolder() = default;

void Outpost::WorldFolder::Save(std::uint64_t _tick, std::vector<std::byte> _save)
{
  {
    const std::scoped_lock lock(m_mutex);
    if (m_failure)
      std::rethrow_exception(m_failure);
    // A world that comes back at its save's tick, having logged nothing after it, makes that save again: the folder has
    // it already.
    if (!std::filesystem::exists(m_folder / FileName(SAVE_PREFIX, _tick, SAVE_EXTENSION)))
      m_pending.emplace_back(_tick, std::move(_save));
  }
  m_changed.notify_all();
  OpenLog(_tick);
}

void Outpost::WorldFolder::Log(std::uint64_t _tick, std::span<const Command> _commands)
{
  if (_commands.empty())
    return;
  if (!m_log.is_open())
    throw Neuron::Exception("WorldFolder: a world logs its commands only once its first save is made");
  for (const Command& command : _commands)
  {
    const std::vector<std::byte> bytes = EncodeMessage(command);
    std::array<char, RECORD_HEADER_BYTES> header{};
    for (std::size_t i = 0; i < sizeof(std::uint64_t); ++i)
      header[i] = static_cast<char>(_tick >> (8 * i));
    const auto length = static_cast<std::uint32_t>(bytes.size());
    for (std::size_t i = 0; i < sizeof(std::uint32_t); ++i)
      header[sizeof(std::uint64_t) + i] = static_cast<char>(length >> (8 * i));
    m_log.write(header.data(), header.size());
    m_log.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  // Handed to the system now, so that a process killed after this tick keeps them (Phase 5 design §5).
  m_log.flush();
  if (!m_log)
    throw Neuron::Exception(std::format("The world's command log in {} cannot be written.", m_folder.string()));
}

void Outpost::WorldFolder::Flush()
{
  std::unique_lock lock(m_mutex);
  m_changed.wait(lock, [this]() { return (m_pending.empty() && !m_writing) || m_failure != nullptr; });
  if (m_failure)
    std::rethrow_exception(m_failure);
}

void Outpost::WorldFolder::OpenLog(std::uint64_t _tick)
{
  m_log.close();
  m_log.open(m_folder / FileName(LOG_PREFIX, _tick, LOG_EXTENSION), std::ios::binary | std::ios::trunc);
  if (!m_log)
    throw Neuron::Exception(std::format("The world's command log in {} cannot be made.", m_folder.string()));
}

void Outpost::WorldFolder::Write(const std::stop_token& _stop)
{
  while (true)
  {
    std::pair<std::uint64_t, std::vector<std::byte>> save;
    {
      std::unique_lock lock(m_mutex);
      // A stop is acted on once nothing is pending, so that the world's last saves are written.
      if (!m_changed.wait(lock, _stop, [this]() { return !m_pending.empty(); }) && m_pending.empty())
        return;
      save = std::move(m_pending.front());
      m_pending.pop_front();
      m_writing = true;
    }
    try
    {
      const std::filesystem::path path = m_folder / FileName(SAVE_PREFIX, save.first, SAVE_EXTENSION);
      std::filesystem::path partial = path;
      partial += PARTIAL_EXTENSION;
      {
        std::ofstream file(partial, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(save.second.data()), static_cast<std::streamsize>(save.second.size()));
        file.close();
        if (!file)
          throw Neuron::Exception(std::format("The world's save {} cannot be written.", partial.string()));
      }
      std::filesystem::rename(partial, path);
      // The newest saves are kept, and the logs from the oldest of them on: a log begins at its save.
      const auto saves = FilesOf(m_folder, SAVE_PREFIX, SAVE_EXTENSION);
      if (saves.size() > SAVES_KEPT)
      {
        const std::uint64_t oldestKept = saves[saves.size() - SAVES_KEPT].first;
        for (std::size_t i = 0; i + SAVES_KEPT < saves.size(); ++i)
          std::filesystem::remove(saves[i].second);
        for (const auto& [tick, log] : FilesOf(m_folder, LOG_PREFIX, LOG_EXTENSION))
        {
          if (tick < oldestKept)
            std::filesystem::remove(log);
        }
      }
    }
    catch (...)
    {
      const std::scoped_lock lock(m_mutex);
      m_failure = std::current_exception();
    }
    {
      const std::scoped_lock lock(m_mutex);
      m_writing = false;
    }
    m_changed.notify_all();
  }
}
