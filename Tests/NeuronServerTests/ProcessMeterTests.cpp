#include "pch.h"

#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace NeuronServerTests
{
// ADR-101: what the server's process takes of its machine, for the overseer's Health.
TEST_CLASS(ProcessMeterTests)
{
public:
  TEST_METHOD(MeasuresItsOwnProcess)
  {
    Neuron::ProcessMeter meter;
    const Neuron::ProcessFigures figures = meter.Measure(std::filesystem::temp_directory_path());
    Assert::IsTrue(figures.workingSetBytes > 0, L"a working set");
    Assert::IsTrue(figures.privateBytes > 0, L"private bytes");
    Assert::IsTrue(figures.threads >= 1, L"this thread at least");
    Assert::IsTrue(figures.handles > 0, L"handles");
    Assert::IsTrue(figures.diskFreeBytes > 0, L"the disk free under the temporary folder");
    Assert::IsTrue(figures.uptime >= 0s);
  }

  // Its CPU is what it used since the meter last measured, in hundredths of a core: about a core's worth while one thread
  // spins, and little while it sleeps.
  TEST_METHOD(CountsTheCpuItUsedSinceItLastMeasured)
  {
    const std::filesystem::path folder = std::filesystem::temp_directory_path();
    Neuron::ProcessMeter meter;
    (void)meter.Measure(folder);
    std::uint64_t spins = 0;
    for (const auto until = std::chrono::steady_clock::now() + 400ms; std::chrono::steady_clock::now() < until;)
      ++spins;
    const Neuron::ProcessFigures busy = meter.Measure(folder);
    Assert::IsTrue(spins > 0);
    Assert::IsTrue(busy.cpuHundredths >= 50, std::format(L"a spinning thread used {} hundredths of a core", busy.cpuHundredths).c_str());
    Assert::IsTrue(busy.cpuHundredths <= (100 * std::max(1u, std::thread::hardware_concurrency())) + 50);
    std::this_thread::sleep_for(400ms);
    const Neuron::ProcessFigures idle = meter.Measure(folder);
    Assert::IsTrue(idle.cpuHundredths < 50, std::format(L"a sleeping thread used {} hundredths of a core", idle.cpuHundredths).c_str());
  }

  TEST_METHOD(SaysNoDiskWhereThereIsNoFolder)
  {
    Neuron::ProcessMeter meter;
    Assert::AreEqual(std::uint64_t{0}, meter.Measure(std::filesystem::temp_directory_path() / "No Such Folder" / "Below").diskFreeBytes);
  }
};
} // namespace NeuronServerTests
