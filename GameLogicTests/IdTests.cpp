#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
// An identifier of one kind cannot stand in for another kind's (ADR-002: entities cross the transport by identifier only).
static_assert(!std::is_convertible_v<Outpost::EntityId, Outpost::PlayerId>);
static_assert(!std::is_convertible_v<Outpost::HullId, Outpost::WeaponId>);
// It crosses a transport as its number alone.
static_assert(std::is_trivially_copyable_v<Outpost::EntityId>);
static_assert(sizeof(Outpost::EntityId) == sizeof(std::uint32_t));

TEST_CLASS(IdTests)
{
public:
  TEST_METHOD(DefaultIsInvalid)
  {
    Assert::IsFalse(Outpost::EntityId{}.IsValid());
  }

  TEST_METHOD(NonZeroIsValid)
  {
    Assert::IsTrue(Outpost::EntityId{1}.IsValid());
  }

  TEST_METHOD(ComparesByValue)
  {
    Assert::IsTrue(Outpost::EntityId{7} == Outpost::EntityId{7});
    Assert::IsTrue(Outpost::EntityId{7} != Outpost::EntityId{8});
    Assert::IsTrue(Outpost::EntityId{7} < Outpost::EntityId{8});
  }
};
} // namespace GameLogicTests