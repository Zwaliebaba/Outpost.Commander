#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr Outpost::WireIdentity DOOMED = Outpost::PackIdentity(3, 1);
constexpr Outpost::WireIdentity WITNESS = Outpost::PackIdentity(4, 1);

[[nodiscard]] Outpost::EntityRecord RecordOf(Outpost::WireIdentity _identity, std::int16_t _positionX) noexcept
{
  Outpost::EntityRecord record;
  record.identity = _identity;
  record.positionX = _positionX;
  record.designIdentity = static_cast<std::uint8_t>(Outpost::DesignId::Fighter);
  record.owner = 2;
  record.hullPercentRemaining = 40;
  return record;
}

/// An update at _tick carrying the witness always, and the doomed entity when _withDoomed says so.
[[nodiscard]] Outpost::Update UpdateAt(std::uint32_t _tick, bool _withDoomed, bool _removeDoomed = false)
{
  Outpost::Update update;
  update.sequence = static_cast<std::uint16_t>(_tick);
  update.tick = _tick;
  update.liveEntityCount = 2;
  update.records.push_back(RecordOf(WITNESS, 0));
  if (_withDoomed)
  {
    update.records.push_back(RecordOf(DOOMED, 400));
  }
  if (_removeDoomed)
  {
    update.removals.push_back(DOOMED);
  }
  return update;
}

[[nodiscard]] bool Holds(const Outpost::ReplicaStore& _store, Outpost::WireIdentity _identity)
{
  for (const Outpost::EntityRecord& record : _store.Entities())
  {
    if (record.identity == _identity)
    {
      return true;
    }
  }
  return false;
}
} // namespace

/// M3.4. **Death is a removal, and never absence inside the horizon.**
TEST_CLASS(TheRemovalList)
{
public:
  /// ADR-024: an update carries what is due, not everything, so an entity missing from updates for up to the
  /// forget floor is **not** dead, is still selectable, and spawns no wreck.
  TEST_METHOD(AbsenceInsideTheHorizonIsNotDeath)
  {
    Outpost::ReplicaStore store;
    Outpost::WreckSet wrecks;
    static_cast<void>(store.Accept(UpdateAt(1, true), 1000));
    for (std::uint32_t tick = 2; tick <= 1 + Outpost::ReplicaStore::FORGET_FLOOR_TICKS; ++tick)
    {
      static_cast<void>(store.Accept(UpdateAt(tick, false), 1000 + (tick * 50)));
      wrecks.Spawn(store.Removed(), 1000 + (tick * 50));
      Assert::IsTrue(Holds(store, DOOMED), L"an entity was treated as dead by absence inside the horizon");
    }
    Assert::AreEqual(std::size_t{0}, wrecks.Wrecks().size(), L"absence spawned a wreck");

    Outpost::Selection selection;
    selection.ReplaceWith(DOOMED);
    Assert::AreEqual(std::size_t{0}, selection.RetainLiving(store.Entities()), L"absence took it out of the selection");
  }

  /// A removal takes it out at once, leaves the selection, and spawns one wreck where it last was.
  TEST_METHOD(ARemovalKillsLeavesTheSelectionAndSpawnsAWreck)
  {
    Outpost::ReplicaStore store;
    Outpost::WreckSet wrecks;
    Outpost::Selection selection;
    static_cast<void>(store.Accept(UpdateAt(1, true), 1000));
    selection.ReplaceWith(DOOMED);
    selection.Add(WITNESS);

    static_cast<void>(store.Accept(UpdateAt(2, false, true), 1050));
    wrecks.Spawn(store.Removed(), 1050);
    Assert::IsFalse(Holds(store, DOOMED));
    Assert::AreEqual(std::size_t{1}, selection.RetainLiving(store.Entities()), L"the dead ship stayed selected");
    Assert::IsTrue(selection.Contains(WITNESS));

    Assert::AreEqual(std::size_t{1}, wrecks.Wrecks().size());
    Assert::AreEqual(DOOMED, wrecks.Wrecks().front().last.identity);
    Assert::AreEqual(std::int16_t{400}, wrecks.Wrecks().front().last.positionX, L"the wreck is not where it died");

    // The removal rides ten updates; the repeats take nothing and spawn nothing.
    for (std::uint32_t tick = 3; tick < 12; ++tick)
    {
      static_cast<void>(store.Accept(UpdateAt(tick, false, true), 1000 + (tick * 50)));
      wrecks.Spawn(store.Removed(), 1000 + (tick * 50));
    }
    Assert::AreEqual(std::size_t{1}, wrecks.Wrecks().size(), L"a repeated removal spawned a second wreck");
  }

  /// **A FORGOTTEN ENTITY SPAWNS NO WRECK**: past the horizon the store drops it, deliberately, and whether it
  /// died is not known.
  TEST_METHOD(AForgottenEntitySpawnsNoWreck)
  {
    Outpost::ReplicaStore store;
    Outpost::WreckSet wrecks;
    static_cast<void>(store.Accept(UpdateAt(1, true), 1000));
    std::uint32_t forgotten = 0;
    for (std::uint32_t tick = 2; tick <= 40; ++tick)
    {
      forgotten += store.Accept(UpdateAt(tick, false), 1000 + (tick * 50)).forgotten;
      wrecks.Spawn(store.Removed(), 1000 + (tick * 50));
    }
    Assert::AreEqual(1u, forgotten);
    Assert::AreEqual(std::size_t{0}, wrecks.Wrecks().size());
  }
};

/// M3.4. **A wreck decays on the client's clock**, and the host is never told.
TEST_CLASS(TheWrecks)
{
public:
  TEST_METHOD(AWreckDarkensAndThenGoes)
  {
    Outpost::WreckSet wrecks;
    const std::vector<Outpost::EntityRecord> removed{RecordOf(DOOMED, 400)};
    wrecks.Spawn(removed, 5000);

    const float early = Outpost::WreckInstance(wrecks.Wrecks().front(), 5000).brightness;
    const float late = Outpost::WreckInstance(wrecks.Wrecks().front(), 5000 + Outpost::WRECK_LIFETIME_MILLISECONDS - 100).brightness;
    Assert::IsTrue(early > 0.0f);
    Assert::IsTrue(early < 1.0f, L"a wreck is as bright as a living hull");
    Assert::IsTrue(late < (early * 0.1f), L"a wreck did not darken");

    wrecks.Expire(5000 + Outpost::WRECK_LIFETIME_MILLISECONDS - 1);
    Assert::AreEqual(std::size_t{1}, wrecks.Wrecks().size());
    wrecks.Expire(5000 + Outpost::WRECK_LIFETIME_MILLISECONDS);
    Assert::AreEqual(std::size_t{0}, wrecks.Wrecks().size());
  }

  /// The burst lasts its half second and then nothing.
  TEST_METHOD(TheBlastIsBrief)
  {
    Outpost::WreckSet wrecks;
    const std::vector<Outpost::EntityRecord> removed{RecordOf(DOOMED, 400)};
    wrecks.Spawn(removed, 5000);
    std::vector<Neuron::BeamInstance> beams;
    Outpost::AppendBlasts(wrecks, 5100, 1.0f, beams);
    Assert::IsFalse(beams.empty());
    beams.clear();
    Outpost::AppendBlasts(wrecks, 5000 + Outpost::BLAST_MILLISECONDS, 1.0f, beams);
    Assert::IsTrue(beams.empty());
  }

  /// Past the cap the oldest goes first.
  TEST_METHOD(TheOldestWreckMakesRoom)
  {
    Outpost::WreckSet wrecks;
    for (std::uint16_t index = 0; index < Outpost::MAX_WRECKS + 5; ++index)
    {
      const std::vector<Outpost::EntityRecord> removed{RecordOf(Outpost::PackIdentity(index, 1), 0)};
      wrecks.Spawn(removed, index);
    }
    Assert::AreEqual(Outpost::MAX_WRECKS, wrecks.Wrecks().size());
    Assert::AreEqual(Outpost::PackIdentity(5, 1), wrecks.Wrecks().front().last.identity);
  }
};

} // namespace GameClientTests
