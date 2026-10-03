#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
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

template <typename Element, typename IdType> bool Exists(const std::vector<Element>& _list, IdType _id)
{
  return std::ranges::any_of(_list, [_id](const Element& _element) { return _element.id == _id; });
}
} // namespace

TEST_CLASS(AiSettingsTests)
{
public:
  // Gate G9 (owner, 2026-10-01): the AI attacks with twelve ships. The rest is the owner's milestone 6 answers.
  TEST_METHOD(LoadsTheRepositoryFile)
  {
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    Assert::AreEqual(12, settings.attackGroupShips);
    Assert::AreEqual(60.0, settings.reviewIntervalSeconds);
    Assert::AreEqual(4, settings.constructors);
    Assert::AreEqual(10.0, settings.incomePerShipyardOrePerSecond);
    Assert::AreEqual(3, settings.homeAsteroids);
    Assert::AreEqual(3, settings.contestedAsteroids);
    // Tier 1, then tier 2 with the Flak Battery first, then tier 3 with the Rail Cannon first (task 10.4).
    const std::vector<std::uint32_t> order{1, 2, 5, 6, 3, 4, 8, 7, 9, 11, 10, 13, 16, 17, 14, 15, 12, 18, 19, 20, 21, 22, 24, 23, 25};
    Assert::AreEqual(order.size(), settings.researchOrder.size());
    for (size_t i = 0; i < order.size(); ++i)
      Assert::AreEqual(order[i], settings.researchOrder[i].value);
    Assert::IsTrue(settings.defaultDesign == Outpost::DesignComponents{Outpost::HullId{2}, Outpost::DriveId{1}, Outpost::WeaponId{1}});
    Assert::AreEqual(size_t{24}, settings.counters.size());
  }

  // The AI cannot check its identifiers against the tuning data, which only the server reads, so this does: every
  // component and topic it names exists, and every topic comes after its prerequisites.
  TEST_METHOD(NamesOnlyWhatTheTuningDataHas)
  {
    const Outpost::AiSettings settings = Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    std::vector<Outpost::DesignComponents> designs{settings.defaultDesign};
    for (const Outpost::CounterRule& rule : settings.counters)
    {
      designs.push_back(rule.enemy);
      designs.push_back(rule.answer);
    }
    for (const Outpost::DesignComponents& design : designs)
      Assert::IsTrue(Exists(tuning.hulls, design.hull) && Exists(tuning.drives, design.drive) && Exists(tuning.weapons, design.weapon));
    for (size_t i = 0; i < settings.researchOrder.size(); ++i)
    {
      const auto topic = std::ranges::find(tuning.research, settings.researchOrder[i], &Outpost::ResearchTopicTuning::id);
      Assert::IsTrue(topic != tuning.research.end());
      for (const Outpost::ResearchTopicId prerequisite : topic->prerequisites)
      {
        const auto before = settings.researchOrder.begin() + static_cast<std::ptrdiff_t>(i);
        Assert::IsTrue(std::find(settings.researchOrder.begin(), before, prerequisite) != before, L"a topic before its prerequisite");
      }
    }
  }

  TEST_METHOD(RejectsAMissingOrUnknownMember)
  {
    ExpectLoadError(Replace("\"attackGroupShips\": 12,", ""), "has no \"attackGroupShips\"");
    ExpectLoadError(Replace("\"constructors\": 4,", "\"constructors\": 4, \"constructor\": 4,"),
                    "constructor: is not a member the game knows");
    ExpectLoadError(Replace("\"defaultDesign\": { \"hull\": 2, ", "\"defaultDesign\": { "), "defaultDesign: has no \"hull\"");
  }

  TEST_METHOD(RejectsAWrongNumber)
  {
    ExpectLoadError(Replace("\"attackGroupShips\": 12,", "\"attackGroupShips\": 0,"), "attackGroupShips");
    ExpectLoadError(Replace("\"reviewIntervalSeconds\": 60,", "\"reviewIntervalSeconds\": 0,"), "reviewIntervalSeconds");
    ExpectLoadError(Replace("\"shipyardQueueJobs\": 2,", "\"shipyardQueueJobs\": 6,"), "shipyardQueueJobs");
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
};
} // namespace GameLogicTests
