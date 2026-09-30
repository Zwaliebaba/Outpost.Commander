#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
// vstest reports "no tests found" as a pass, so an empty suite would be a green check mark over a library nobody
// exercised. This placeholder keeps the suite honest until the first real test lands; delete it then, never before
// (AGENTS.md §3).
TEST_CLASS(SuiteSmoke)
{
public:
  TEST_METHOD(Runs)
  {
    Assert::IsTrue(true);
  }
};
} // namespace GameLogicTests
