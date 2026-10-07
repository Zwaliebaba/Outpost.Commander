#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
// The options _arguments ask for. A braced list becomes the vector, which the span is made from.
Outpost::AiMatchesOptions Read(const std::vector<std::wstring>& _arguments)
{
  return Outpost::ReadAiMatchesOptions(_arguments);
}

// The message ReadAiMatchesOptions refuses _arguments with.
std::string RefusalOf(const std::vector<std::wstring>& _arguments)
{
  std::string message;
  try
  {
    (void)Outpost::ReadAiMatchesOptions(_arguments);
  }
  catch (const Neuron::Exception& error)
  {
    message = error.what();
  }
  Assert::IsFalse(message.empty(), L"the arguments were taken");
  return message;
}
} // namespace

// ADR-063: --ai-matches takes options naming the seeds, each AI's settings and the log, and anything else fails, naming the
// argument, so that a mistyped option cannot run the packaged AI in its place. The shell has already taken out the
// switch itself and --quiet.
TEST_CLASS(AiMatchesOptionsTests)
{
public:
  // With no options it plays ADR-038's ten matches of the packaged AI, into the log in the temporary folder.
  TEST_METHOD(PlaysTheTenPackagedMatchesWithNoOptions)
  {
    const Outpost::AiMatchesOptions options = Read({});
    Assert::AreEqual(std::uint64_t{1}, options.desc.firstSeed);
    Assert::AreEqual(10u, options.desc.matches);
    Assert::AreEqual(120u, options.desc.limitMinutes);
    Assert::IsTrue(options.settings[0].empty());
    Assert::IsTrue(options.settings[1].empty());
    Assert::AreEqual((std::filesystem::temp_directory_path() / L"OutpostCommander-ai-matches.log").wstring(), options.log.wstring());
  }

  TEST_METHOD(ReadsEveryOptionInAnyOrder)
  {
    const std::vector<std::wstring> arguments{
      L"--log",      L"C:\\Self Play\\run 7.log", L"--ai2", L"second.json", L"--matches", L"4", L"--first-seed", L"1000", L"--ai1",
      L"first.json", L"--limit-minutes",          L"45",
    };
    const Outpost::AiMatchesOptions options = Read(arguments);
    Assert::AreEqual(std::uint64_t{1000}, options.desc.firstSeed);
    Assert::AreEqual(4u, options.desc.matches);
    Assert::AreEqual(45u, options.desc.limitMinutes);
    Assert::AreEqual(std::wstring(L"first.json"), options.settings[0].wstring());
    Assert::AreEqual(std::wstring(L"second.json"), options.settings[1].wstring());
    // A quoted path with spaces is one argument once CommandLineToArgvW has split the line, and is kept whole.
    Assert::AreEqual(std::wstring(L"C:\\Self Play\\run 7.log"), options.log.wstring());
  }

  // An option left out keeps its default while the others are read.
  TEST_METHOD(KeepsTheDefaultsOfOptionsNotGiven)
  {
    const Outpost::AiMatchesOptions options = Read({L"--ai2", L"second.json", L"--matches", L"2"});
    Assert::AreEqual(std::uint64_t{1}, options.desc.firstSeed);
    Assert::AreEqual(2u, options.desc.matches);
    Assert::AreEqual(120u, options.desc.limitMinutes);
    Assert::IsTrue(options.settings[0].empty());
    Assert::AreEqual(std::wstring(L"second.json"), options.settings[1].wstring());
    Assert::AreEqual(std::wstring(L"OutpostCommander-ai-matches.log"), options.log.filename().wstring());
  }

  // The seed may be 0 and any 64-bit number; a count of matches or minutes is at least 1 and fits 32 bits.
  TEST_METHOD(ReadsEachNumberAcrossItsWholeRange)
  {
    Assert::AreEqual(std::uint64_t{0}, Read({L"--first-seed", L"0"}).desc.firstSeed);
    Assert::AreEqual(std::numeric_limits<std::uint64_t>::max(), Read({L"--first-seed", L"18446744073709551615"}).desc.firstSeed);
    Assert::AreEqual(1u, Read({L"--matches", L"1"}).desc.matches);
    Assert::AreEqual(std::numeric_limits<std::uint32_t>::max(), Read({L"--matches", L"4294967295"}).desc.matches);
    Assert::AreEqual(1u, Read({L"--limit-minutes", L"1"}).desc.limitMinutes);
    Assert::AreEqual(std::numeric_limits<std::uint32_t>::max(), Read({L"--limit-minutes", L"4294967295"}).desc.limitMinutes);
    // Leading zeros are still decimal digits and nothing else.
    Assert::AreEqual(7u, Read({L"--matches", L"007"}).desc.matches);
  }

  TEST_METHOD(RefusesAnOptionItDoesNotTake)
  {
    Assert::AreEqual(std::string("--ai-matches: --seed is not an option it takes."), RefusalOf({L"--seed", L"5"}));
    // Options are matched exactly, so a capital letter is a mistake rather than the option.
    Assert::AreEqual(std::string("--ai-matches: --Matches is not an option it takes."), RefusalOf({L"--Matches", L"5"}));
    // A value with no option before it is read where an option should be.
    Assert::AreEqual(std::string("--ai-matches: stray is not an option it takes."), RefusalOf({L"--matches", L"5", L"stray"}));
  }

  TEST_METHOD(RefusesAnOptionGivenTwice)
  {
    Assert::AreEqual(std::string("--ai-matches: --ai1 is given twice."), RefusalOf({L"--ai1", L"a.json", L"--ai1", L"b.json"}));
    Assert::AreEqual(std::string("--ai-matches: --matches is given twice."),
                     RefusalOf({L"--matches", L"2", L"--log", L"x.log", L"--matches", L"2"}));
  }

  // The value follows its option, and is neither missing, empty nor another option.
  TEST_METHOD(RefusesAnOptionWithoutAValue)
  {
    Assert::AreEqual(std::string("--ai-matches: --log needs a value."), RefusalOf({L"--log"}));
    Assert::AreEqual(std::string("--ai-matches: --ai2 needs a value."), RefusalOf({L"--ai2", L""}));
    Assert::AreEqual(std::string("--ai-matches: --ai1 needs a value."), RefusalOf({L"--ai1", L"--ai2", L"b.json"}));
    Assert::AreEqual(std::string("--ai-matches: --first-seed needs a value."), RefusalOf({L"--matches", L"3", L"--first-seed"}));
  }

  TEST_METHOD(RefusesANumberOutOfRange)
  {
    Assert::AreEqual(std::string("--ai-matches: --matches takes a whole number from 1 to 4294967295, not '0'."),
                     RefusalOf({L"--matches", L"0"}));
    Assert::AreEqual(std::string("--ai-matches: --limit-minutes takes a whole number from 1 to 4294967295, not '0'."),
                     RefusalOf({L"--limit-minutes", L"0"}));
    Assert::AreEqual(std::string("--ai-matches: --matches takes a whole number from 1 to 4294967295, not '4294967296'."),
                     RefusalOf({L"--matches", L"4294967296"}));
    // One past the largest 64-bit seed overflows the reading itself.
    Assert::AreEqual(std::string("--ai-matches: --first-seed takes a whole number from 0 to 18446744073709551615, not "
                                 "'18446744073709551616'."),
                     RefusalOf({L"--first-seed", L"18446744073709551616"}));
  }

  TEST_METHOD(RefusesANumberWithAnythingButDecimalDigits)
  {
    for (const wchar_t* value : {L"-1", L"+5", L"1.5", L"1e3", L" 5", L"5 ", L"0x10", L"ten"})
    {
      const std::string message = RefusalOf({L"--matches", value});
      Assert::IsTrue(message.starts_with("--ai-matches: --matches takes a whole number from 1 to 4294967295, not '"),
                     winrt::to_hstring(message).c_str());
    }
  }

  // A wide character is never narrowed into a digit it is not: U+0135's low byte is '5', and U+0661 is an Arabic-Indic one.
  TEST_METHOD(RefusesADigitOutsideAscii)
  {
    for (const wchar_t* value : {L"\u0135", L"1\u0135", L"\u0661"})
      (void)RefusalOf({L"--matches", value});
  }

  // Of several mistakes, the first argument it cannot take is the one named.
  TEST_METHOD(NamesTheFirstArgumentItCannotTake)
  {
    Assert::AreEqual(std::string("--ai-matches: --matches takes a whole number from 1 to 4294967295, not 'many'."),
                     RefusalOf({L"--ai1", L"a.json", L"--matches", L"many", L"--bogus", L"1", L"--ai1", L"b.json"}));
  }
};
} // namespace GameLogicTests
