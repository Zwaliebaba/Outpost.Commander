#include "pch.h"
#include "RepositoryData.h"
#include "UniverseMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
// ADR-082 decision 4: the universe run, which every later phase measures with.
TEST_CLASS(UniverseRunTests)
{
public:
  // A universe with an AI empire in every seat, killed at random ticks and made afresh from its folder each time, ends the
  // same as the universe run straight through, to the bit. Each kill brings it back at or before the tick it was killed after.
  TEST_METHOD(AKilledUniverseEndsAsTheUniverseRunStraightThrough)
  {
    const ScopedHomeDirectory home(RepositoryHome());
    const TemporaryFolder folder;
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    const Outpost::UniverseRunResult result = Outpost::RunUniverses(
      {.seed = 4, .folder = folder.Path(), .ticks = 3ull * 60 * 20, .kills = 3, .makePlayer = [&settings](Outpost::PlayerId) {
         return std::make_unique<Outpost::AiEmpire>(settings, 20);
       }});
    Assert::AreEqual(std::size_t{3}, result.killTicks.size());
    Assert::AreEqual(std::size_t{3}, result.recoveredTicks.size());
    for (std::size_t kill = 0; kill < result.killTicks.size(); ++kill)
      Assert::IsTrue(result.recoveredTicks[kill] <= result.killTicks[kill]);
    Assert::IsTrue(result.same, L"the killed universe ends as the universe run straight through");
    Assert::IsTrue(std::filesystem::exists(folder.Path() / "Killed" / Outpost::UNIVERSE_LOG_FILE));
    Assert::ExpectException<Neuron::Exception>(
      [&]
      {
        (void)Outpost::RunUniverses({.folder = folder.Path(), .ticks = 1, .makePlayer = [&settings](Outpost::PlayerId) {
                                       return std::make_unique<Outpost::AiEmpire>(settings, 20);
                                     }});
      },
      L"a folder that holds a universe run already is refused");
  }
};
} // namespace GameLogicTests
