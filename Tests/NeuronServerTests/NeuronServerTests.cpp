#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronServerTests
{
/// The placeholder every suite ships, because vstest reports "no tests found" as a pass and an
/// empty suite is therefore worse than no suite. It proves the library linked and that its master
/// include reached this translation unit. Delete it when the first real test lands, never before.
TEST_CLASS(SuiteSmoke)
{
public:
  TEST_METHOD(TheLibraryIsLinked)
  {
    Assert::IsTrue(Neuron::CoreLibraryName() == "NeuronCore");
    Assert::IsTrue(Neuron::ServerLibraryName() == "NeuronServer");
  }
};
} // namespace NeuronServerTests