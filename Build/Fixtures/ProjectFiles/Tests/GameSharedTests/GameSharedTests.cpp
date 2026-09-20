#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameSharedTests
{

TEST_CLASS(GameSharedTests)
{
public:
  TEST_METHOD(ARealTest)
  {
    Assert::AreEqual(1, 1);
  }
};

} // namespace GameSharedTests
