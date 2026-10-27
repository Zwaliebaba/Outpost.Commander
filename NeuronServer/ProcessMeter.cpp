#include "pch.h"
#include "ProcessMeter.h"

#include <psapi.h>
#include <tlhelp32.h>

namespace
{
// A FILETIME's count of hundreds of nanoseconds.
std::uint64_t Ticks(const FILETIME& _time) noexcept
{
  return (static_cast<std::uint64_t>(_time.dwHighDateTime) << 32) | _time.dwLowDateTime;
}

// The kernel and user time the process has used, and when it was made, each in hundreds of nanoseconds; zero when Windows
// does not say.
struct ProcessTimes
{
  std::uint64_t cpu = 0;
  std::uint64_t created = 0;
};

ProcessTimes TimesOfProcess() noexcept
{
  FILETIME created{};
  FILETIME exited{};
  FILETIME kernel{};
  FILETIME user{};
  if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) == FALSE)
    return {};
  return {.cpu = Ticks(kernel) + Ticks(user), .created = Ticks(created)};
}

// The threads of the process, from a snapshot of the system's processes; zero when Windows makes none.
std::uint32_t ThreadsOfProcess() noexcept
{
  const winrt::file_handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
  if (!snapshot)
    return 0;
  const DWORD self = GetCurrentProcessId();
  PROCESSENTRY32W entry{.dwSize = sizeof(PROCESSENTRY32W)};
  for (BOOL found = Process32FirstW(snapshot.get(), &entry); found != FALSE; found = Process32NextW(snapshot.get(), &entry))
  {
    if (entry.th32ProcessID == self)
      return entry.cntThreads;
  }
  return 0;
}
} // namespace

Neuron::ProcessMeter::ProcessMeter() noexcept
  : m_lastWall(std::chrono::steady_clock::now()),
    m_lastCpu(TimesOfProcess().cpu)
{
}

Neuron::ProcessFigures Neuron::ProcessMeter::Measure(const std::filesystem::path& _folder)
{
  ProcessFigures figures;
  PROCESS_MEMORY_COUNTERS_EX memory{};
  if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE)
  {
    figures.workingSetBytes = memory.WorkingSetSize;
    figures.privateBytes = memory.PrivateUsage;
  }

  // The CPU since the last measure, against the wall time since: both in hundreds of nanoseconds.
  const ProcessTimes times = TimesOfProcess();
  const auto now = std::chrono::steady_clock::now();
  const auto wall = std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>>(now - m_lastWall).count();
  if (wall > 0 && times.cpu >= m_lastCpu)
    figures.cpuHundredths = static_cast<std::uint32_t>(((times.cpu - m_lastCpu) * 100) / static_cast<std::uint64_t>(wall));
  m_lastWall = now;
  m_lastCpu = times.cpu;

  FILETIME system{};
  GetSystemTimeAsFileTime(&system);
  if (times.created != 0 && Ticks(system) > times.created)
    figures.uptime = std::chrono::seconds((Ticks(system) - times.created) / 10'000'000);

  figures.threads = ThreadsOfProcess();
  DWORD handles = 0;
  if (GetProcessHandleCount(GetCurrentProcess(), &handles) != FALSE)
    figures.handles = handles;
  ULARGE_INTEGER available{};
  if (GetDiskFreeSpaceExW(_folder.wstring().c_str(), &available, nullptr, nullptr) != FALSE)
    figures.diskFreeBytes = available.QuadPart;
  return figures;
}
