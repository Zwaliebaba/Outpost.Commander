#include "pch.h"

#include <cstdint>
#include <map>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;

/// A world of _count miners on a line, alternating between two owners, a unit apart per slot.
[[nodiscard]] Outpost::World LineOf(int _count)
{
  Outpost::World world;
  for (int entity = 0; entity < _count; ++entity)
  {
    static_cast<void>(world.Create(Neuron::Vec2{.x = entity * Neuron::FIXED_ONE, .y = 0}, 0, Outpost::DesignId::Miner,
                                   static_cast<Outpost::PlayerId>((entity % 2) + 1)));
  }
  return world;
}

[[nodiscard]] std::size_t RecordCount(const std::vector<Outpost::Update>& _updates) noexcept
{
  std::size_t count = 0;
  for (const Outpost::Update& update : _updates)
  {
    count += update.records.size();
  }
  return count;
}
} // namespace

/// ADR-024's accumulator. Every property M1.14c's completion check names is here, and none of them needs
/// a socket.
TEST_CLASS(TheAccumulator)
{
public:
  /// **ONE DATAGRAM, WHATEVER IS DUE.** A hundred players' worth of entities, everything never sent and
  /// therefore due at once: every update still encodes, and each is at most the pinned payload.
  TEST_METHOD(AnUpdateNeverExceedsThePayload)
  {
    const Outpost::World world = LineOf(5500);
    Outpost::Accumulator accumulator;
    const std::vector<Outpost::Update> updates = accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 1);

    Assert::AreEqual(Outpost::UPDATES_PER_TICK, updates.size(), L"with thousands due, both updates go out");
    for (const Outpost::Update& update : updates)
    {
      std::vector<std::byte> buffer(Outpost::UPDATE_PAYLOAD_BYTES);
      Neuron::ByteWriter writer{buffer};
      Assert::IsTrue(Outpost::Encode(update, writer), L"an update the encoder refuses");
      Assert::IsTrue(writer.WrittenBytes() <= Outpost::UPDATE_PAYLOAD_BYTES);
    }
  }

  /// **AT THE MVP, EVERY ENTITY EVERY TICK.** 110 entities in two updates of up to a hundred records.
  TEST_METHOD(AtTheMvpEveryEntityIsSentEveryTick)
  {
    const Outpost::World world = LineOf(110);
    Outpost::Accumulator accumulator;
    for (std::uint32_t tick = 1; tick <= 10; ++tick)
    {
      Assert::AreEqual(std::size_t{110}, RecordCount(accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, tick)));
    }
  }

  /// **THE SWEEP IS A GUARANTEE.** A thousand entities, a view that favors a hundred of them, forty ticks:
  /// no entity goes longer than the sweep without a record, however the scores fall.
  TEST_METHOD(NoEntityGoesASweepUnsent)
  {
    const Outpost::World world = LineOf(1000);
    Outpost::Accumulator accumulator;
    accumulator.SetView(MINE, 0, 0, 100);
    const std::uint32_t sweep = Outpost::SweepTicks(world.AliveCount());

    std::map<Outpost::WireIdentity, std::uint32_t> lastSent;
    for (std::uint32_t tick = 1; tick <= 40; ++tick)
    {
      for (const Outpost::Update& update : accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, tick))
      {
        for (const Outpost::EntityRecord& record : update.records)
        {
          const auto found = lastSent.find(record.identity);
          if (found != lastSent.end())
          {
            Assert::IsTrue((tick - found->second) <= sweep, L"an entity went longer than the sweep unsent");
          }
          lastSent[record.identity] = tick;
        }
      }
    }
    Assert::AreEqual(std::size_t{1000}, lastSent.size(), L"every entity was sent at least once");
  }

  /// **RELEVANCE DECIDES HOW OFTEN.** Entities inside the view are refreshed more often than those outside
  /// it, which is the whole reason the view is on the wire.
  TEST_METHOD(AnEntityInViewIsRefreshedMoreOftenThanOneOutside)
  {
    const Outpost::World world = LineOf(1000);
    Outpost::Accumulator accumulator;
    accumulator.SetView(MINE, 0, 0, 50);

    std::uint32_t nearSends = 0;
    std::uint32_t farSends = 0;
    const Outpost::WireIdentity nearIdentity = Outpost::PackIdentity(10, 1);
    const Outpost::WireIdentity farIdentity = Outpost::PackIdentity(900, 1);
    for (std::uint32_t tick = 1; tick <= 40; ++tick)
    {
      for (const Outpost::Update& update : accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, tick))
      {
        for (const Outpost::EntityRecord& record : update.records)
        {
          nearSends += (record.identity == nearIdentity) ? 1u : 0u;
          farSends += (record.identity == farIdentity) ? 1u : 0u;
        }
      }
    }
    Assert::IsTrue(nearSends > farSends, L"the view did not favor what is in it");
  }

  /// **THE SAME WORLD AND VIEW GIVE THE SAME SENT SET TWICE.** The sort is a total order on the slot, so
  /// two accumulators cannot disagree -- which is what lets a suite pin what a client saw.
  TEST_METHOD(TheSentSetIsTheSameTwice)
  {
    const Outpost::World world = LineOf(700);
    Outpost::Accumulator first;
    Outpost::Accumulator second;
    first.SetView(MINE, 64, 0, 200);
    second.SetView(MINE, 64, 0, 200);

    for (std::uint32_t tick = 1; tick <= 12; ++tick)
    {
      const std::vector<Outpost::Update> left = first.Fill(world, MINE, Outpost::PlayerBlock{}, tick);
      const std::vector<Outpost::Update> right = second.Fill(world, MINE, Outpost::PlayerBlock{}, tick);
      Assert::AreEqual(left.size(), right.size());
      for (std::size_t index = 0; index < left.size(); ++index)
      {
        Assert::IsTrue(left[index].records == right[index].records, L"two accumulators sent different sets");
      }
    }
  }

  /// **A DEATH RIDES TEN CONSECUTIVE UPDATES AND THEN STOPS.** Found by looking, not by being told: the
  /// slot this client was sent is no longer the entity it was sent.
  TEST_METHOD(ARemovalRidesTenConsecutiveUpdates)
  {
    Outpost::World world = LineOf(3);
    Outpost::Accumulator accumulator;
    static_cast<void>(accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 1));

    const Outpost::EntityId doomed{.index = 1, .generation = 1};
    const Outpost::WireIdentity doomedWire = Outpost::PackIdentity(doomed.index, doomed.generation);
    Assert::IsTrue(world.Destroy(doomed));

    for (std::uint32_t tick = 2; tick < 2 + Outpost::REMOVAL_REPEAT_TICKS; ++tick)
    {
      const std::vector<Outpost::Update> updates = accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, tick);
      Assert::AreEqual(std::size_t{1}, updates.front().removals.size(), L"the removal did not ride this update");
      Assert::AreEqual(doomedWire, updates.front().removals.front());
    }
    const std::vector<Outpost::Update> after = accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 2 + Outpost::REMOVAL_REPEAT_TICKS);
    Assert::AreEqual(std::size_t{0}, after.front().removals.size(), L"the removal rode past its ten ticks");
  }

  /// A slot freed and refilled in one tick is two facts: the old occupant's removal and the new one's
  /// first record. A reused slot must never read as the same entity.
  TEST_METHOD(AReusedSlotIsARemovalAndANewRecord)
  {
    Outpost::World world = LineOf(2);
    Outpost::Accumulator accumulator;
    static_cast<void>(accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 1));

    Assert::IsTrue(world.Destroy(Outpost::EntityId{.index = 0, .generation = 1}));
    const Outpost::EntityId reborn = world.Create(Neuron::Vec2{}, 0, Outpost::DesignId::Miner, MINE);
    Assert::AreEqual(std::uint16_t{0}, reborn.index, L"the test needs the slot reused");

    const std::vector<Outpost::Update> updates = accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 2);
    Assert::AreEqual(Outpost::PackIdentity(0, 1), updates.front().removals.front());
    bool sentReborn = false;
    for (const Outpost::EntityRecord& record : updates.front().records)
    {
      sentReborn = sentReborn || (record.identity == Outpost::PackIdentity(reborn.index, reborn.generation));
    }
    Assert::IsTrue(sentReborn, L"the new occupant was not sent at once");
  }

  /// **A REJOIN MAKES EVERYTHING DUE.** After a reset the client is sent everything again within one
  /// sweep, and the removals it missed are dropped rather than repeated to a store that is empty.
  TEST_METHOD(AResetMakesEverythingDueAndDropsPendingRemovals)
  {
    Outpost::World world = LineOf(110);
    Outpost::Accumulator accumulator;
    static_cast<void>(accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 1));
    Assert::IsTrue(world.Destroy(Outpost::EntityId{.index = 5, .generation = 1}));
    static_cast<void>(accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 2));

    accumulator.Reset(MINE);
    const std::vector<Outpost::Update> updates = accumulator.Fill(world, MINE, Outpost::PlayerBlock{}, 3);
    Assert::AreEqual(std::size_t{109}, RecordCount(updates));
    Assert::AreEqual(std::size_t{0}, updates.front().removals.size());
  }

  /// **AT LEAST ONE UPDATE, EVEN WITH NOTHING DUE.** The client hears its own block and the tick every
  /// tick; its link-silence detector depends on it.
  TEST_METHOD(AnEmptyWorldStillSendsOneUpdateWithTheOwnBlock)
  {
    const Outpost::World world;
    Outpost::Accumulator accumulator;
    const Outpost::PlayerBlock own{.credits = 900, .lastCommandSequenceApplied = 4};
    const std::vector<Outpost::Update> updates = accumulator.Fill(world, MINE, own, 7);

    Assert::AreEqual(std::size_t{1}, updates.size());
    Assert::AreEqual(std::uint32_t{7}, updates.front().tick);
    Assert::IsTrue(updates.front().own == own);
    Assert::AreEqual(std::uint16_t{0}, updates.front().liveEntityCount);
  }

  /// Each client's transport sequence is its own and goes up by one per update, which is what lets a
  /// client count its own loss.
  TEST_METHOD(EachClientsSequenceIsItsOwn)
  {
    const Outpost::World world = LineOf(300);
    Outpost::Accumulator accumulator;
    const std::vector<Outpost::Update> mine = accumulator.Fill(world, 1, Outpost::PlayerBlock{}, 1);
    const std::vector<Outpost::Update> theirs = accumulator.Fill(world, 2, Outpost::PlayerBlock{}, 1);

    Assert::AreEqual(std::uint16_t{0}, mine[0].sequence);
    Assert::AreEqual(std::uint16_t{1}, mine[1].sequence);
    Assert::AreEqual(std::uint16_t{0}, theirs[0].sequence);
    Assert::AreEqual(std::size_t{2}, accumulator.ClientCount());
  }
};

} // namespace GameLogicTests
