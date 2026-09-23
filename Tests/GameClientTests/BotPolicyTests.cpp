#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr Outpost::PlayerId ME = 1;
constexpr Outpost::PlayerId RIVAL = 2;
constexpr std::uint64_t SEED = 0x5EED5EEDull;

[[nodiscard]] Outpost::EntityRecord Record(std::uint16_t _index, Outpost::PlayerId _owner, Outpost::DesignId _design)
{
  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(_index, 1);
  record.owner = _owner;
  record.designIdentity = static_cast<std::uint8_t>(_design);
  record.hullPercentRemaining = 100;
  return record;
}

/// **THIS BOT'S VIEW OF A SMALL MATCH**: its station, three of its ships, and a rival's station and ship --
/// the rival's are the ones a policy that got ownership wrong would name.
[[nodiscard]] Outpost::Update World(std::uint32_t _tick, std::uint32_t _credits, std::uint8_t _building = 0, std::uint16_t _lastApplied = 0)
{
  Outpost::Update update;
  update.sequence = static_cast<std::uint16_t>(_tick);
  update.tick = _tick;
  update.liveEntityCount = 6;
  update.own = Outpost::PlayerBlock{
    .credits = _credits, .lastCommandSequenceApplied = _lastApplied, .buildingDesign = _building, .buildProgressPercent = 0};
  update.records = {Record(1, ME, Outpost::DesignId::Station),    Record(2, ME, Outpost::DesignId::Miner),
                    Record(3, ME, Outpost::DesignId::Fighter),    Record(4, ME, Outpost::DesignId::Miner),
                    Record(5, RIVAL, Outpost::DesignId::Station), Record(6, RIVAL, Outpost::DesignId::Fighter)};
  return update;
}

/// Runs a policy over a decision tick every interval for _decisions decisions, and returns every order.
[[nodiscard]] std::vector<Outpost::Command> Run(Outpost::BotPolicy& _policy, std::uint32_t _credits, std::uint8_t _building,
                                                std::uint32_t _decisions)
{
  Outpost::ReplicaStore store;
  std::vector<Outpost::Command> all;
  for (std::uint32_t decision = 0; decision < _decisions; ++decision)
  {
    const std::uint32_t tick = 100 + (decision * Outpost::BotPolicy::DECISION_INTERVAL_TICKS);
    static_cast<void>(store.Accept(World(tick, _credits, _building), tick * 50ull));
    const std::vector<Outpost::Command> orders = _policy.Decide(store, ME, tick * 50ull);
    all.insert(all.end(), orders.begin(), orders.end());
  }
  return all;
}
} // namespace

/// ADR-022's player policy. **Every one of these runs without a socket**: the policy reads a replica store and
/// returns commands, which is what lets `Bot/` hold none of it (R20).
TEST_CLASS(TheBotPolicy)
{
public:
  /// **THE SAME UPDATES AND THE SAME SEED GIVE THE SAME ORDERS**, so a stress run can be replayed from its
  /// command line apart from what the network did.
  TEST_METHOD(TheSameInputsGiveTheSameOrdersTwice)
  {
    Outpost::BotPolicy first{SEED, 3};
    Outpost::BotPolicy second{SEED, 3};
    const std::vector<Outpost::Command> a = Run(first, 1000, 0, 200);
    const std::vector<Outpost::Command> b = Run(second, 1000, 0, 200);

    Assert::IsFalse(a.empty(), L"two hundred decisions ordered nothing");
    Assert::IsTrue(a == b, L"the same inputs gave different orders");
  }

  /// And two bots are not the same bot: the stream is per bot, so a run of many does not move in lockstep.
  TEST_METHOD(TwoBotsDrawFromTwoStreams)
  {
    Outpost::BotPolicy first{SEED, 0};
    Outpost::BotPolicy second{SEED, 1};
    Assert::IsFalse(Run(first, 1000, 0, 200) == Run(second, 1000, 0, 200));
  }

  /// **IT NEVER NAMES AN ENTITY IT DOES NOT OWN**, and never one that cannot move. The flooder is the role
  /// that sends refusable orders, deliberately and counted; a player that did would be measuring the wrong
  /// refusal path.
  TEST_METHOD(ItNeverCommandsAnEntityItDoesNotOwn)
  {
    Outpost::BotPolicy policy{SEED, 7};
    const std::vector<Outpost::Command> orders = Run(policy, 1000, 0, 500);

    std::size_t moves = 0;
    for (const Outpost::Command& order : orders)
    {
      if (order.type != Outpost::CommandType::MoveTo)
      {
        Assert::IsTrue(order.selection.empty(), L"a station order carried a selection");
        continue;
      }
      ++moves;
      Assert::IsFalse(order.selection.empty(), L"a move with nothing selected");
      for (const Outpost::WireIdentity selected : order.selection)
      {
        const std::uint16_t index = Outpost::IndexOf(selected);
        Assert::IsTrue((index == 2) || (index == 3) || (index == 4), L"a move named a rival's entity or a station");
      }
    }
    Assert::IsTrue(moves > 0, L"five hundred decisions moved nothing");
  }

  /// **IT NEVER BUILDS WHAT ITS CREDITS CANNOT COVER**, by the derived cost (R24) and not a figure of its own.
  TEST_METHOD(ItNeverBuildsWhatItCannotAfford)
  {
    const std::uint32_t minerCost = Outpost::Derive(Outpost::DesignId::Miner).cost;
    const std::uint32_t fighterCost = Outpost::Derive(Outpost::DesignId::Fighter).cost;
    Assert::IsTrue(minerCost < fighterCost, L"the fixture assumes the miner is the cheaper design");

    Outpost::BotPolicy poor{SEED, 1};
    for (const Outpost::Command& order : Run(poor, minerCost - 1, 0, 300))
    {
      Assert::IsTrue(order.type != Outpost::CommandType::Build, L"built with too few credits for anything");
    }

    Outpost::BotPolicy middling{SEED, 1};
    std::size_t builds = 0;
    for (const Outpost::Command& order : Run(middling, fighterCost - 1, 0, 300))
    {
      if (order.type == Outpost::CommandType::Build)
      {
        ++builds;
        Assert::IsTrue(order.TargetDesign() == static_cast<std::uint8_t>(Outpost::DesignId::Miner), L"built what it could not afford");
      }
    }
    Assert::IsTrue(builds > 0, L"never built what it could afford");
  }

  /// With something already building it does not build over it -- which would be a refund and a charge the
  /// cancel already exercises -- and it does sometimes cancel.
  TEST_METHOD(ItBuildsOnlyIntoAnEmptySlot)
  {
    Outpost::BotPolicy policy{SEED, 2};
    std::size_t cancels = 0;
    for (const Outpost::Command& order : Run(policy, 100000, 1, 300))
    {
      Assert::IsTrue(order.type != Outpost::CommandType::Build, L"built over an item already building");
      if (order.type == Outpost::CommandType::CancelBuild)
      {
        ++cancels;
      }
    }
    Assert::IsTrue(cancels > 0, L"three hundred decisions never canceled");
  }

  /// **SILENT BETWEEN DECISION TICKS.** An update every tick does not mean an order every tick.
  TEST_METHOD(ItIsSilentBetweenDecisionTicks)
  {
    Outpost::BotPolicy policy{SEED, 4};
    Outpost::ReplicaStore store;

    static_cast<void>(store.Accept(World(100, 1000), 5000));
    Assert::IsFalse(policy.Decide(store, ME, 5000).empty(), L"the first decision with credits and an empty slot built nothing");

    for (std::uint32_t tick = 101; tick < 100 + Outpost::BotPolicy::DECISION_INTERVAL_TICKS; ++tick)
    {
      static_cast<void>(store.Accept(World(tick, 1000, 1), tick * 50ull));
      Assert::IsTrue(policy.Decide(store, ME, tick * 50ull).empty(), L"ordered between decision ticks");
    }
  }

  /// Nothing before an update: no credits known, no ships known, and no player to be.
  TEST_METHOD(ItIsSilentBeforeItHasBeenToldAnything)
  {
    Outpost::BotPolicy policy{SEED, 0};
    const Outpost::ReplicaStore empty;
    Assert::IsTrue(policy.Decide(empty, ME, 0).empty());

    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(World(100, 1000), 5000));
    Assert::IsTrue(policy.Decide(store, Outpost::NO_PLAYER, 5000).empty(), L"ordered before it was seated");
  }

  /// **ITS NUMBERING STARTS WHERE THE HOST LEFT OFF**, as `ClientFrame`'s does for a person: a counter at one
  /// would be refused until it counted past what the host last applied.
  TEST_METHOD(ItAdoptsItsSequenceFromTheHost)
  {
    Outpost::BotPolicy policy{SEED, 0};
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(World(100, 1000, 0, 41), 5000));
    const std::vector<Outpost::Command> orders = policy.Decide(store, ME, 5000);

    Assert::IsFalse(orders.empty());
    Assert::AreEqual(std::uint16_t{42}, orders.front().sequence);
    for (std::size_t index = 1; index < orders.size(); ++index)
    {
      Assert::AreEqual(static_cast<std::uint16_t>(orders[index - 1].sequence + 1), orders[index].sequence, L"a gap in the sequence");
    }
  }

  /// **IT KEEPS ITS ORDERS UNTIL THE HOST HAS APPLIED THEM** (ADR-003), and says how long each waited.
  TEST_METHOD(AnAcknowledgmentRetiresWhatItCoversAndTimesIt)
  {
    Outpost::BotPolicy policy{SEED, 0};
    Outpost::ReplicaStore store;
    static_cast<void>(store.Accept(World(100, 1000), 5000));
    const std::vector<Outpost::Command> orders = policy.Decide(store, ME, 5000);
    Assert::IsFalse(orders.empty());
    Assert::AreEqual(orders.size(), policy.Outstanding().size());

    std::vector<std::uint64_t> waits;
    Assert::AreEqual(static_cast<std::size_t>(0), policy.Acknowledge(0, 5100, waits), L"retired an order the host had not applied");

    Assert::AreEqual(static_cast<std::size_t>(1), policy.Acknowledge(orders.front().sequence, 5120, waits));
    Assert::AreEqual(static_cast<std::size_t>(1), waits.size());
    Assert::AreEqual(static_cast<std::uint64_t>(120), waits.front());
    Assert::AreEqual(orders.size() - 1, policy.Outstanding().size());
  }
};

} // namespace GameClientTests
