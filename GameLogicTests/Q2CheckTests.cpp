#include "pch.h"
#include "RepositoryData.h"
#include "Q2Check.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
std::vector<CheckDesign> RepositoryDesigns(const Outpost::Tuning& _tuning)
{
  return DesignsFrom(_tuning, PartsFrom(_tuning));
}

const CheckDesign& Named(const std::vector<CheckDesign>& _designs, std::string_view _code)
{
  const auto found = std::ranges::find(_designs, _code, &CheckDesign::code);
  Assert::IsTrue(found != _designs.end());
  return *found;
}

// How many of _battles _a wins against _b.
int Wins(const CheckDesign& _a, const CheckDesign& _b, double _budgetOre, FireMode _mode, std::uint32_t _battles)
{
  int wins = 0;
  for (std::uint32_t battle = 0; battle < _battles; ++battle)
    wins += Fight(_a, _b, _budgetOre, _mode, battle, _battles) > 0 ? 1 : 0;
  return wins;
}
} // namespace

TEST_CLASS(Q2CheckTests)
{
public:
  // The check's designs are the game's: the same stats DesignStatsFor derives, and the model's short codes.
  TEST_METHOD(FieldsTheGamesDesigns)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const std::vector<CheckDesign> designs = RepositoryDesigns(tuning);
    // Forty-five: three hulls, three drives and five weapons (Phase 1 design §5).
    Assert::AreEqual(size_t{45}, designs.size());
    for (const CheckDesign& design : designs)
    {
      const Outpost::DesignStats stats =
        Outpost::DesignStatsFor(tuning, design.components.hull, design.components.drive, design.components.weapon);
      Assert::IsTrue(design.stats == stats, std::wstring(design.code.begin(), design.code.end()).c_str());
    }
    Assert::AreEqual(std::string("S+I+MD"), designs.front().code);
    Assert::AreEqual(std::string("L+P+RC"), designs.back().code);
    Assert::AreEqual(30.0f, Named(designs, "L+F+MR").stats.splashRadiusMeters);
    Assert::AreEqual(26.0f, Named(designs, "S+P+FB").stats.splashRadiusMeters);

    // Through tier 1, the MVP's eighteen: the Pulse Drive, the Flak Battery and the Rail Cannon come with tiers 2 and 3.
    const std::vector<CheckDesign> tierOne = DesignsFrom(tuning, PartsThrough(tuning, 1));
    Assert::AreEqual(size_t{18}, tierOne.size());
    Assert::AreEqual(std::string("L+F+MR"), tierOne.back().code);
    Assert::AreEqual(size_t{36}, DesignsFrom(tuning, PartsThrough(tuning, 2)).size());
    Assert::AreEqual(size_t{45}, DesignsFrom(tuning, PartsThrough(tuning, 3)).size());
  }

  // A battle replays: the same battle of the same pairing has the same outcome.
  TEST_METHOD(ABattleIsTheSameEveryTime)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const std::vector<CheckDesign> designs = RepositoryDesigns(tuning);
    for (std::uint32_t battle = 0; battle < 4; ++battle)
    {
      const int first = Fight(Named(designs, "M+I+La"), Named(designs, "M+I+MD"), 2000.0, FireMode::Spread, battle, 4);
      Assert::AreEqual(first, Fight(Named(designs, "M+I+La"), Named(designs, "M+I+MD"), 2000.0, FireMode::Spread, battle, 4));
    }
  }

  // ADR-024: a battle under fog of war ends as it does without, since every armed ship sees beyond its weapon's range and
  // nothing a ship fires at depends on what its side sees. The swarm against the line, the picket against a heavy, and the
  // Missile Rack's splash, under both targeting extremes.
  TEST_METHOD(FogChangesNoBattle)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const std::vector<CheckDesign> designs = RepositoryDesigns(tuning);
    const std::array<std::pair<std::string_view, std::string_view>, 3> pairings{
      {{"S+I+MD", "M+I+La"}, {"S+I+La", "L+F+MD"}, {"M+I+MR", "M+I+MD"}}};
    for (const auto& [a, b] : pairings)
    {
      for (const FireMode mode : {FireMode::Spread, FireMode::Focus})
      {
        for (std::uint32_t battle = 0; battle < 2; ++battle)
        {
          const int clear = Fight(Named(designs, a), Named(designs, b), 3000.0, mode, battle, 2);
          Assert::AreEqual(clear, Fight(Named(designs, a), Named(designs, b), 3000.0, mode, battle, 2, tuning.sight));
        }
      }
    }
  }

  // A design against itself wins about as often on either side. The arena once placed the second side as the first's
  // mirror image, and a partial last row then gave the second side every battle at 9,000 Ore.
  TEST_METHOD(AMirrorMatchIsFair)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const std::vector<CheckDesign> designs = RepositoryDesigns(tuning);
    const CheckDesign& swarm = Named(designs, "S+I+MD");
    constexpr std::uint32_t BATTLES = 30;
    int first = 0;
    int second = 0;
    for (std::uint32_t battle = 0; battle < BATTLES; ++battle)
    {
      const int outcome = Fight(swarm, swarm, 4500.0, FireMode::Spread, battle, BATTLES);
      first += outcome > 0 ? 1 : 0;
      second += outcome < 0 ? 1 : 0;
    }
    // Far outside chance for a fair coin over 30 battles is fewer than 7 for either side.
    Assert::IsTrue(first >= 7 && second >= 7, (std::to_wstring(first) + L" to " + std::to_wstring(second)).c_str());
  }

  // Task 3.4, the part CI runs: the counters that design §12 records as holding in the simulation, at the smallest
  // budget, so that a change that breaks them fails here rather than only in the full check.
  TEST_METHOD(TheRecordedCountersHold)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const std::vector<CheckDesign> designs = RepositoryDesigns(tuning);
    constexpr std::uint32_t BATTLES = 20;
    for (const FireMode mode : {FireMode::Spread, FireMode::Focus})
    {
      // The swarm beats the line, and the brawler beats the swarm (design §7).
      Assert::IsTrue(Wins(Named(designs, "S+I+MD"), Named(designs, "M+I+La"), 2000.0, mode, BATTLES) >= 16);
      Assert::IsTrue(Wins(Named(designs, "M+I+MD"), Named(designs, "S+I+MD"), 2000.0, mode, BATTLES) >= 16);
      // The picket beats the heavy.
      Assert::IsTrue(Wins(Named(designs, "S+I+La"), Named(designs, "L+F+MD"), 2000.0, mode, BATTLES) >= 16);
    }
    // The line beats the brawler under focus fire only: under spread fire it does not (design §12).
    Assert::IsTrue(Wins(Named(designs, "M+I+La"), Named(designs, "M+I+MD"), 2000.0, FireMode::Focus, BATTLES) >= 16);

    // Phase 1 design §5, at the smallest budget of each component's tier (task 10.3): the Flak Battery breaks the swarm,
    // a Pulse picket beats the heavy brawler, and the Rail Cannon kills the heavy Lance line.
    for (const FireMode mode : {FireMode::Spread, FireMode::Focus})
    {
      Assert::IsTrue(Wins(Named(designs, "M+I+FB"), Named(designs, "S+I+MD"), 4500.0, mode, BATTLES) >= 16);
      Assert::IsTrue(Wins(Named(designs, "S+P+La"), Named(designs, "L+F+MD"), 4500.0, mode, BATTLES) >= 16);
      Assert::IsTrue(Wins(Named(designs, "L+F+RC"), Named(designs, "L+F+La"), 6000.0, mode, BATTLES) >= 16);
    }
  }

  // Task 3.4: the whole Q2 check against the simulation. It takes minutes in Release and hours in Debug, so it runs only
  // when OUTPOST_Q2_FULL is set, and otherwise says that it did not. CI never sets it; the owner runs it in Release:
  //   set OUTPOST_Q2_FULL=1
  //   vstest.console.exe x64\Release\GameLogicTests.dll
  // The switch is in the test rather than a vstest filter because the native test adapter ignores a filter on its
  // TestCategory trait. The report goes to the test's output and to Q2Check-report.txt in the temporary folder. The
  // verdicts are recorded in design §12.
  BEGIN_TEST_METHOD_ATTRIBUTE(TheFullCheck) TEST_METHOD_ATTRIBUTE(L"TestCategory", L"Q2Full") END_TEST_METHOD_ATTRIBUTE()
  TEST_METHOD(TheFullCheck)
  {
    if (GetEnvironmentVariableW(L"OUTPOST_Q2_FULL", nullptr, 0) == 0)
    {
      Logger::WriteMessage("Not run: set OUTPOST_Q2_FULL to run the full Q2 check.");
      return;
    }
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const CheckResult result = RunQ2Check(tuning, {});
    std::ofstream(std::filesystem::temp_directory_path() / "Q2Check-report.txt") << result.report;
    Logger::WriteMessage(result.report.c_str());
    Assert::IsTrue(result.Passed(), L"The Q2 check does not pass; the report says where.");
  }
};
} // namespace GameLogicTests