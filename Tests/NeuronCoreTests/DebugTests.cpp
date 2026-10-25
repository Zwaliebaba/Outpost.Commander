#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
// A failed assertion ends the process (ADR-093), so no test can watch one fail. These hold what a passing one does, and
// make MSVC compile the macros and Neuron::Fatal, which nothing else in the tree calls yet.
TEST_CLASS(DebugTests)
{
public:
  TEST_METHOD(ChecksAnAssertionsExpressionOnce)
  {
    int checks = 0;
    ASSERT(++checks == 1);
    Assert::AreEqual(1, checks);
  }

  TEST_METHOD(FormatsAnAssertionsTextOnlyWhenItFails)
  {
    int checks = 0;
    int formats = 0;
    const auto counted = [&formats] { return ++formats; };
    ASSERT_TEXT(++checks == 1, "{} formats", counted());
    Assert::AreEqual(1, checks, L"checked once");
    Assert::AreEqual(0, formats, L"never formatted");
  }

  TEST_METHOD(ChecksADebugAssertionOnlyInDebug)
  {
    int checks = 0;
    DEBUG_ASSERT(++checks == 1);
#ifdef _DEBUG
    Assert::AreEqual(1, checks);
#else
    Assert::AreEqual(0, checks);
#endif
  }
};
} // namespace NeuronCoreTests
