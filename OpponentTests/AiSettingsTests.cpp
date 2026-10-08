#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace OpponentTests
{
namespace
{
std::wstring Widen(std::string_view _text)
{
  return {_text.begin(), _text.end()};
}

// The repository's Opponent.json with the one occurrence of _from replaced by _to.
std::string Replace(std::string_view _from, std::string_view _to)
{
  std::string text = ReadRepositoryData("Opponent.json");
  const size_t at = text.find(_from);
  Assert::IsTrue(at != std::string::npos && text.find(_from, at + 1) == std::string::npos, Widen(_from).c_str());
  text.replace(at, _from.size(), _to);
  return text;
}

// Loading fails, and the message names the place.
void ExpectLoadError(const std::string& _text, std::string_view _where)
{
  try
  {
    (void)Outpost::LoadAiSettings(_text);
  }
  catch (const Neuron::Exception& error)
  {
    const std::string message = error.what();
    Assert::IsTrue(message.find(_where) != std::string::npos, Widen(std::format("\"{}\" does not name {}", message, _where)).c_str());
    return;
  }
  Assert::Fail(Widen(std::format("loaded, but {} is wrong", _where)).c_str());
}
} // namespace

TEST_CLASS(AiSettingsTests)
{
public:
  // Gate G9 (owner, 2026-10-01) had the AI attack with twelve ships; task 12.2 tuned the attack so that two AIs play a
  // 45-60 minute match (owner, 2026-10-03): twenty ships and twelve more for each tier past the first, behind two Defence
  // Platforms for each Shipyard. Phase 2's task 19.2 tuned the fall-back against S4 with territory: after losing one in
  // ten, regrouping for 240 seconds (ADR-041). Phase 3's task 25.2 tuned it against T1–T4: twenty-five ships, falling back
  // after losing 15 in a hundred. The rest is the owner's milestone 6 answers.
  TEST_METHOD(LoadsTheRepositoryFile)
  {
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    Assert::AreEqual(25, settings.attackGroupShips);
    Assert::AreEqual(12, settings.attackGroupGrowthPerTier);
    Assert::AreEqual(0.15, settings.retreatLossShare);
    Assert::AreEqual(240.0, settings.regroupSeconds);
    Assert::AreEqual(2, settings.homePlatformsPerShipyard);
    Assert::AreEqual(60.0, settings.reviewIntervalSeconds);
    Assert::AreEqual(4, settings.constructors);
    Assert::AreEqual(10.0, settings.incomePerShipyardOrePerSecond);
    Assert::AreEqual(3, settings.homeAsteroids);
    Assert::AreEqual(3, settings.contestedAsteroids);
    // Tier 1, then tier 2 with the Flak Battery first, then tier 3 with the Rail Cannon first (task 10.4).
    const std::vector<std::uint32_t> order{1, 2, 5, 6, 3, 4, 8, 7, 11, 10, 13, 16, 17, 14, 15, 12, 19, 20, 21, 22, 24, 23, 25};
    Assert::AreEqual(order.size(), settings.researchOrder.size());
    for (size_t i = 0; i < order.size(); ++i)
      Assert::AreEqual(order[i], settings.researchOrder[i].value);
    Assert::IsTrue(settings.defaultDesign == Outpost::DesignComponents{Outpost::HullId{2}, Outpost::DriveId{1}, Outpost::WeaponId{1}});
    Assert::AreEqual(size_t{24}, settings.counters.size());
  }

  // Task 18.1: territory's play (Phase 2 design §12). One scout, a Small+Ion+Mass Driver with a Sensor Array; raids of
  // two that fall back after losing one and wait three minutes between them; a main attack with a lead of one node or
  // half as many ships again; a Defence Platform by each Relay on its front; and two free sectors claimed beyond its rigs'
  // (Phase 3's task 25.2).
  TEST_METHOD(LoadsTheTerritoryPlay)
  {
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    Assert::AreEqual(1, settings.scouts);
    Assert::IsTrue(settings.scoutDesign ==
                   Outpost::DesignComponents{Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{1}, Outpost::ModuleId{1}});
    Assert::AreEqual(2, settings.raidShips);
    Assert::AreEqual(0.5, settings.raidLossShare);
    Assert::AreEqual(180.0, settings.raidIntervalSeconds);
    Assert::AreEqual(1, settings.attackNodeLead);
    Assert::AreEqual(1.5, settings.attackWithoutLeadShare);
    Assert::AreEqual(1, settings.frontPlatforms);
    Assert::AreEqual(2, settings.claimSectors);
  }

  // Task 24.1: structure levels' play (Phase 3 design §8). The Lab's second slot once tier 3 is open, and a level worth
  // 500 m when choosing what to attack.
  TEST_METHOD(LoadsTheLevelsPlay)
  {
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    Assert::AreEqual(3, settings.secondSlotTier);
    Assert::AreEqual(500.0, settings.attackLevelMeters);
  }

  // Task 27.4: the fleet cap's play (Phase 4 design §12). Its main attack goes once its reserve fills 80% of the room the cap
  // leaves it, at most a share of 1; the Easy AI waits for a full cap.
  TEST_METHOD(LoadsTheFleetCapPlay)
  {
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    Assert::AreEqual(0.8, settings.attackCapShare);
    Assert::AreEqual(1.0, Outpost::LoadAiSettings(ReadRepositoryData("OpponentEasy.json")).attackCapShare);
    ExpectLoadError(Replace("\"attackCapShare\": 0.8", "\"attackCapShare\": 0"), "attackCapShare");
    ExpectLoadError(Replace("\"attackCapShare\": 0.8", "\"attackCapShare\": 1.5"), "attackCapShare");
  }

  // ADR-065: the Easy and Hard AIs' files load, and differ from the Normal one where their difficulty says: Easy builds a
  // smaller economy, fewer Shipyards, never raids or claims beyond its rigs, reviews its answer less often and attacks later;
  // Hard keeps more Constructors, reviews its answer more often, raids with more ships, and claims at least as much.
  TEST_METHOD(LoadsEachDifficulty)
  {
    const Outpost::AiSettings normal = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    const Outpost::AiSettings easy = Outpost::LoadAiSettings(ReadRepositoryData("OpponentEasy.json"));
    const Outpost::AiSettings hard = Outpost::LoadAiSettings(ReadRepositoryData("OpponentHard.json"));
    Assert::IsTrue(easy.constructors < normal.constructors && normal.constructors < hard.constructors);
    Assert::IsTrue(easy.reviewIntervalSeconds > normal.reviewIntervalSeconds && normal.reviewIntervalSeconds > hard.reviewIntervalSeconds);
    Assert::IsTrue(easy.contestedAsteroids < normal.contestedAsteroids);
    Assert::IsTrue(easy.incomePerShipyardOrePerSecond > normal.incomePerShipyardOrePerSecond);
    Assert::IsTrue(easy.shipyardQueueJobs < normal.shipyardQueueJobs);
    Assert::IsTrue(easy.raidShips == 0 && easy.claimSectors == 0, L"Easy leaves the player's nodes to the player");
    Assert::IsTrue(easy.attackGroupShips > normal.attackGroupShips && easy.attackWithoutLeadShare > normal.attackWithoutLeadShare);
    Assert::IsTrue(hard.raidShips > normal.raidShips && hard.claimSectors >= normal.claimSectors);
  }

  TEST_METHOD(RejectsAMissingOrUnknownMember)
  {
    ExpectLoadError(Replace("\"attackGroupShips\": 25,", ""), "has no \"attackGroupShips\"");
    ExpectLoadError(Replace("\"constructors\": 4,", "\"constructors\": 4, \"constructor\": 4,"),
                    "constructor: is not a member the game knows");
    ExpectLoadError(Replace("\"defaultDesign\": { \"hull\": 2, ", "\"defaultDesign\": { "), "defaultDesign: has no \"hull\"");
  }

  TEST_METHOD(RejectsAWrongNumber)
  {
    ExpectLoadError(Replace("\"attackGroupShips\": 25,", "\"attackGroupShips\": 0,"), "attackGroupShips");
    ExpectLoadError(Replace("\"reviewIntervalSeconds\": 60,", "\"reviewIntervalSeconds\": 0,"), "reviewIntervalSeconds");
    ExpectLoadError(Replace("\"shipyardQueueJobs\": 2,", "\"shipyardQueueJobs\": 6,"), "shipyardQueueJobs");
    ExpectLoadError(Replace("\"retreatLossShare\": 0.15,", "\"retreatLossShare\": 1,"), "retreatLossShare");
    ExpectLoadError(Replace("\"raidLossShare\": 0.5,", "\"raidLossShare\": 1,"), "raidLossShare");
    ExpectLoadError(Replace("\"attackWithoutLeadShare\": 1.5,", "\"attackWithoutLeadShare\": 0,"), "attackWithoutLeadShare");
    ExpectLoadError(Replace("\"researchOrder\": [1, 2,", "\"researchOrder\": [1, 1,"), "researchOrder[1]");
    ExpectLoadError(Replace("\"researchOrder\": [1, 2,", "\"researchOrder\": [0, 2,"), "researchOrder[0]");
  }

  // A design may be answered more than once, in order of preference, but not twice with the same answer.
  TEST_METHOD(RejectsTheSameCounterTwice)
  {
    ExpectLoadError(
      Replace("{ \"enemy\": { \"hull\": 1, \"drive\": 1, \"weapon\": 1 }, \"answer\": { \"hull\": 2, \"drive\": 1, \"weapon\": 1 } }",
              "{ \"enemy\": { \"hull\": 1, \"drive\": 1, \"weapon\": 1 }, \"answer\": { \"hull\": 3, \"drive\": 2, \"weapon\": 1 } }"),
      "counters[2]: the same answer to the same design as counters[1]");
  }

  // The game reads each difficulty's settings from the package's Assets folder (ADR-065), Opponent.json when it names none.
  TEST_METHOD(LoadsEachPackagedFile)
  {
    const ScopedHomeDirectory home(RepositoryHome());
    for (const std::wstring_view file : {Outpost::NORMAL_AI_SETTINGS, Outpost::EASY_AI_SETTINGS, Outpost::HARD_AI_SETTINGS})
    {
      const Outpost::AiSettings packaged = Outpost::LoadPackagedAiSettings(file);
      const Outpost::AiSettings expected = Outpost::LoadAiSettings(ReadRepositoryData(winrt::to_string(file)));
      const std::wstring name(file);
      Assert::AreEqual(expected.constructors, packaged.constructors, name.c_str());
      Assert::AreEqual(expected.reviewIntervalSeconds, packaged.reviewIntervalSeconds, name.c_str());
      Assert::AreEqual(expected.raidShips, packaged.raidShips, name.c_str());
    }
    const Outpost::AiSettings normal = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    Assert::AreEqual(normal.constructors, Outpost::LoadPackagedAiSettings().constructors);
    Assert::AreEqual(normal.reviewIntervalSeconds, Outpost::LoadPackagedAiSettings().reviewIntervalSeconds);
  }

  TEST_METHOD(RefusesAPackagedFileThatIsMissing)
  {
    const TemporaryHomeDirectory home(L"AiSettingsTests");
    try
    {
      (void)Outpost::LoadPackagedAiSettings(Outpost::HARD_AI_SETTINGS);
    }
    catch (const Neuron::Exception& error)
    {
      Assert::AreEqual(std::string("The game data file Assets\\OpponentHard.json is missing or cannot be read."),
                       std::string(error.what()));
      return;
    }
    Assert::Fail(L"a missing file loaded");
  }

  // A packaged file that does not load is named before what is wrong with it.
  TEST_METHOD(NamesAPackagedFileThatIsInvalid)
  {
    const TemporaryHomeDirectory home(L"AiSettingsTests");
    home.WriteAsset(Outpost::NORMAL_AI_SETTINGS, Replace("\"researchOrder\": [1, 2,", "\"researchOrder\": [0, 2,"));
    try
    {
      (void)Outpost::LoadPackagedAiSettings();
    }
    catch (const Neuron::Exception& error)
    {
      const std::string message = error.what();
      Assert::IsTrue(message.starts_with("Opponent.json: ") && message.find("researchOrder[0]") != std::string::npos,
                     Widen(message).c_str());
      return;
    }
    Assert::Fail(L"an invalid file loaded");
  }
};
} // namespace OpponentTests
