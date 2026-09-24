#include "pch.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId AI = 2;
constexpr Outpost::PlayerId ENEMY = 1;

[[nodiscard]] Outpost::EntityRecord RecordAt(std::uint16_t _index, std::int32_t _x, std::int32_t _y, Outpost::PlayerId _owner,
                                             Outpost::DesignId _design) noexcept
{
  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(_index, 1);
  record.owner = _owner;
  record.positionX = Outpost::QuantizePosition(_x * Neuron::FIXED_ONE);
  record.positionY = Outpost::QuantizePosition(_y * Neuron::FIXED_ONE);
  record.designIdentity = static_cast<std::uint8_t>(_design);
  record.hullPercentRemaining = 100;
  return record;
}

[[nodiscard]] Outpost::Placement RockAt(std::int32_t _x, std::int32_t _y) noexcept
{
  return Outpost::Placement{.kind = Outpost::PlacedKind::Asteroid,
                            .field = Outpost::FieldKind::Home,
                            .position = Neuron::Vec2{.x = _x * Neuron::FIXED_ONE, .y = _y * Neuron::FIXED_ONE}};
}

[[nodiscard]] const Outpost::Command* FirstOf(const std::vector<Outpost::Command>& _commands, Outpost::CommandType _type) noexcept
{
  for (const Outpost::Command& command : _commands)
  {
    if (command.type == _type)
    {
      return &command;
    }
  }
  return nullptr;
}
} // namespace

/// M3.10, `OpenQuestions.md` Q48. **The stub decides over what a player is sent, and nothing else.**
TEST_CLASS(TheStubAi)
{
public:
  /// With nothing building and the credits for it, it builds a Miner first.
  TEST_METHOD(ItBuildsAMinerFirst)
  {
    const std::vector<Outpost::EntityRecord> entities{RecordAt(0, 0, 0, AI, Outpost::DesignId::Station)};
    Outpost::StubAi ai;
    std::vector<Outpost::Command> commands;
    ai.Decide(Outpost::AiView{.player = AI, .entities = entities, .own = Outpost::PlayerBlock{.credits = 1000}}, commands);
    const Outpost::Command* build = FirstOf(commands, Outpost::CommandType::Build);
    Assert::IsNotNull(build);
    Assert::AreEqual(static_cast<int>(Outpost::DesignId::Miner), static_cast<int>(build->targetX));
  }

  /// A new Miner is sent to the nearest rock once, and not again.
  TEST_METHOD(ItSendsEachNewMinerToTheNearestRockOnce)
  {
    const std::vector<Outpost::EntityRecord> entities{RecordAt(0, 0, 0, AI, Outpost::DesignId::Station),
                                                      RecordAt(1, 300, 0, AI, Outpost::DesignId::Miner)};
    const std::vector<Outpost::Placement> field{RockAt(5000, 0), RockAt(1300, 0), RockAt(-3000, 0)};
    Outpost::StubAi ai;
    std::vector<Outpost::Command> commands;
    const Outpost::AiView view{.player = AI, .entities = entities, .own = Outpost::PlayerBlock{}, .field = field};
    ai.Decide(view, commands);
    const Outpost::Command* mine = FirstOf(commands, Outpost::CommandType::Mine);
    Assert::IsNotNull(mine);
    Assert::AreEqual(std::int16_t{1}, mine->targetX, L"it did not pick the nearest rock");

    ai.Decide(view, commands);
    Assert::IsNull(FirstOf(commands, Outpost::CommandType::Mine), L"it ordered the same miner twice");
  }

  /// **A RAID ON THE HOME FIELD IS ANSWERED BEFORE ANY STRIKE**, and eight fighters with nothing to defend strike.
  TEST_METHOD(ItDefendsBeforeItStrikes)
  {
    std::vector<Outpost::EntityRecord> entities{RecordAt(0, 0, 0, AI, Outpost::DesignId::Station),
                                                RecordAt(1, 12000, 0, ENEMY, Outpost::DesignId::Station)};
    for (std::uint16_t index = 0; index < Outpost::AI_STRIKE_FIGHTERS; ++index)
    {
      entities.push_back(RecordAt(static_cast<std::uint16_t>(10 + index), 500, index * 80, AI, Outpost::DesignId::Fighter));
    }

    Outpost::StubAi striker;
    std::vector<Outpost::Command> commands;
    striker.Decide(Outpost::AiView{.player = AI, .entities = entities}, commands);
    const Outpost::Command* strike = FirstOf(commands, Outpost::CommandType::Attack);
    Assert::IsNotNull(strike);
    Assert::IsTrue(strike->TargetEntity() == Outpost::PackIdentity(1, 1), L"it did not strike the enemy station");
    Assert::AreEqual(Outpost::AI_STRIKE_FIGHTERS, strike->selection.size());

    entities.push_back(RecordAt(40, 1500, 0, ENEMY, Outpost::DesignId::Fighter));
    Outpost::StubAi defender;
    defender.Decide(Outpost::AiView{.player = AI, .entities = entities}, commands);
    const Outpost::Command* defend = FirstOf(commands, Outpost::CommandType::Attack);
    Assert::IsNotNull(defend);
    Assert::IsTrue(defend->TargetEntity() == Outpost::PackIdentity(40, 1), L"it ignored a raider in its home field");
  }

  /// **A MATCH AGAINST IT REACHES AN END UNATTENDED**: two AI seats, no client, and the host ends the match.
  TEST_METHOD(TwoStubsPlayAMatchToItsEnd)
  {
    Outpost::Host host;
    host.BeginMatch(Outpost::DEFAULT_MATCH_SEED, 2);
    host.SetAiSeats(2);

    std::uint32_t ticks = 0;
    std::size_t mostMiners = 0;
    std::size_t mostFighters = 0;
    for (; (host.LastEnded().matchNumber == 0) && (ticks < Outpost::MATCH_CLOCK_TICKS + 20); ++ticks)
    {
      host.RunOneTick();
      std::size_t miners = 0;
      std::size_t fighters = 0;
      for (std::size_t slot = 0; slot < host.CurrentWorld().SlotCount(); ++slot)
      {
        if (host.CurrentWorld().IsSlotAlive(slot))
        {
          const Outpost::DesignId design = host.CurrentWorld().EntityInSlot(slot).design;
          miners += (design == Outpost::DesignId::Miner) ? 1 : 0;
          fighters += (design == Outpost::DesignId::Fighter) ? 1 : 0;
        }
      }
      mostMiners = std::max(mostMiners, miners);
      mostFighters = std::max(mostFighters, fighters);
    }

    const Outpost::MatchEnded& ended = host.LastEnded();
    Logger::WriteMessage(
      (std::wstring{L"STUB AI MATCH ended after "} + std::to_wstring(ticks) + L" ticks, " +
       ((ended.winner == Outpost::NO_PLAYER) ? std::wstring{L"a draw"} : L"won by player " + std::to_wstring(ended.winner)) +
       (ended.onClock ? L" on the clock" : L" by elimination") + L"; at most " + std::to_wstring(mostMiners) + L" miners and " +
       std::to_wstring(mostFighters) + L" fighters on the field\n")
        .c_str());
    Assert::AreEqual(std::uint16_t{1}, ended.matchNumber, L"the match never ended");
    Assert::IsTrue(mostMiners >= 2 * Outpost::AI_MINERS, L"the stubs did not build their miners");
    Assert::IsTrue(mostFighters >= Outpost::AI_STRIKE_FIGHTERS, L"the stubs never built a strike force");
  }
};

/// M3.10. **AN AI SEAT IS NEVER A CLIENT'S.**
TEST_CLASS(TheReservedSeats)
{
public:
  TEST_METHOD(AClientIsNeverGivenAnAiSeat)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, 1);
    sessions.ReserveSeats(1);
    const Outpost::JoinReply first = sessions.Admit(Outpost::Join{}, Neuron::Endpoint{.addressV4 = 1, .port = 100});
    Assert::AreEqual(Outpost::PlayerId{1}, first.player);
    const Outpost::JoinReply second = sessions.Admit(Outpost::Join{}, Neuron::Endpoint{.addressV4 = 1, .port = 101});
    Assert::IsTrue(second.result == Outpost::JoinResult::MatchFull, L"a client took the AI's seat");
  }
};

} // namespace GameLogicTests
