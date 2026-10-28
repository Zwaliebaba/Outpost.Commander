#pragma once

namespace Neuron
{
// What a process takes of its machine (ADR-101): its working set and its private bytes; the CPU it used since the meter last
// measured, in hundredths of a core, so that 200 is two cores' worth; its threads and its handles; how long it has run; and
// the disk free to it under a folder.
struct ProcessFigures
{
  std::uint64_t workingSetBytes = 0;
  std::uint64_t privateBytes = 0;
  std::uint32_t cpuHundredths = 0;
  std::uint32_t threads = 0;
  std::uint32_t handles = 0;
  std::chrono::seconds uptime{};
  std::uint64_t diskFreeBytes = 0;
};

// Measures the process it runs in, through the Windows SDK alone (R14). A figure Windows does not give is zero, rather than
// a failure: the meter only reports. One thread at a time.
class ProcessMeter : NonCopyable
{
public:
  ProcessMeter() noexcept;

  // The process now, its CPU since the last call or since the meter was made, and the disk free under _folder.
  [[nodiscard]] ProcessFigures Measure(const std::filesystem::path& _folder);

private:
  std::chrono::steady_clock::time_point m_lastWall;
  // The kernel and user time the process had used then, in hundreds of nanoseconds.
  std::uint64_t m_lastCpu = 0;
};
} // namespace Neuron
