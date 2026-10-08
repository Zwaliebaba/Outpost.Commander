#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};

Outpost::AiSettings RepositorySettings()
{
  return Outpost::LoadAiSettings(ReadRepositoryData("Opponent.json"));
}

// Phase 5 design §2 W3: how often a player's built Shipyards and its Research Lab stand idle with Ore to spend, as shares
// of the seconds each stood.
struct Idleness
{
  std::uint64_t yardSeconds = 0;
  std::uint64_t idleYardSeconds = 0;
  std::uint64_t labSeconds = 0;
  std::uint64_t idleLabSeconds = 0;

  void Sample(const Outpost::Snapshot& _snapshot)
  {
    for (const Outpost::EntityView& entity : _snapshot.entities)
    {
      if (entity.owner != _snapshot.player || entity.kind != Outpost::EntityKind::Structure || entity.builtPermille < Outpost::PERMILLE)
        continue;
      if (entity.structure == Outpost::StructureKind::Shipyard)
      {
        ++yardSeconds;
        // A ship it could pay for and the cap has room for.
        const bool affordable =
          std::ranges::any_of(_snapshot.designs,
                              [&](const Outpost::DesignView& _design)
                              {
                                const auto hull = std::ranges::find(_snapshot.hulls, _design.hull, &Outpost::HullView::id);
                                return hull != _snapshot.hulls.end() && hull->shipyardLevel <= entity.level &&
                                       _design.cost <= _snapshot.ore &&
                                       (_snapshot.fleetCap <= 0 || _snapshot.commandPoints + hull->commandPoints <= _snapshot.fleetCap);
                              });
        idleYardSeconds += entity.queue.empty() && affordable ? 1 : 0;
      }
      else if (entity.structure == Outpost::StructureKind::ResearchLab)
      {
        ++labSeconds;
        const auto researched = [&_snapshot](Outpost::ResearchTopicId _id)
        {
          return std::ranges::any_of(_snapshot.research,
                                     [_id](const Outpost::ResearchTopicView& _topic) { return _topic.id == _id && _topic.researched; });
        };
        const bool open = std::ranges::any_of(_snapshot.research,
                                              [&](const Outpost::ResearchTopicView& _topic)
                                              {
                                                return !_topic.researched && _topic.tier <= _snapshot.researchTier &&
                                                       _topic.cost <= _snapshot.ore &&
                                                       std::ranges::all_of(_topic.prerequisites, researched);
                                              });
        idleLabSeconds += entity.research.empty() && open ? 1 : 0;
      }
    }
  }

  [[nodiscard]] double YardShare() const noexcept
  {
    return yardSeconds > 0 ? static_cast<double>(idleYardSeconds) / static_cast<double>(yardSeconds) : 0.0;
  }

  [[nodiscard]] double LabShare() const noexcept
  {
    return labSeconds > 0 ? static_cast<double>(idleLabSeconds) / static_cast<double>(labSeconds) : 0.0;
  }
};
} // namespace

// Phase 5 design §2 W3 (ADR-079): the deputy keeps its player's empire running on the real server as the AI keeps its own.
TEST_CLASS(DeputyPlayTests)
{
public:
  // Phase 5 design §2 W3: two AIs play for ten minutes, and then blue's seat is its deputy's for ten more. The deputy keeps
  // blue's Shipyards and Lab busy at least as well as the AI keeps red's, and it only keeps and defends: it attacks
  // nothing, claims nothing, salvages nothing and builds nothing but a lost rig.
  TEST_METHOD(KeepsAnEmpireRunningAsTheAiKeepsItsOwn)
  {
    Outpost::InProcessServer server(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()), {.seed = 2});
    server.World().PlaceStartingBases(server.MapData());
    server.PreparePathfinding();
    const std::unique_ptr<Outpost::Transport> blue = server.Connect(BLUE);
    const std::unique_ptr<Outpost::Transport> red = server.Connect(RED);
    const std::uint32_t tps = server.TicksPerSecond();
    Outpost::AiPlayer blueAi(RepositorySettings(), tps);
    Outpost::AiPlayer redAi(RepositorySettings(), tps);
    Outpost::Deputy deputy(RepositorySettings(), tps);
    Idleness deputyIdleness;
    Idleness aiIdleness;
    const std::uint64_t handOver = std::uint64_t{10} * 60 * tps;
    const std::uint64_t end = 2 * handOver;
    while (server.World().CurrentTick() < end)
    {
      server.Step();
      for (const Outpost::Snapshot& snapshot : blue->Receive())
      {
        const bool away = snapshot.tick >= handOver;
        std::vector<Outpost::Command> orders = away ? deputy.Play(snapshot) : blueAi.Update(snapshot);
        if (away && snapshot.tick % tps == 0)
          deputyIdleness.Sample(snapshot);
        for (Outpost::Command& order : orders)
        {
          if (away)
          {
            Assert::IsFalse(std::holds_alternative<Outpost::AttackCommand>(order.order), L"it attacked");
            Assert::IsFalse(std::holds_alternative<Outpost::SalvageCommand>(order.order), L"it salvaged");
            const auto* build = std::get_if<Outpost::BuildStructureCommand>(&order.order);
            Assert::IsTrue(build == nullptr || build->structure == Outpost::StructureKind::MiningRig, L"it built more than a lost rig");
            if (const auto* attackMove = std::get_if<Outpost::AttackMoveCommand>(&order.order))
            {
              const Outpost::SectorView* sector = Outpost::FindSector(snapshot.sectors, attackMove->destination);
              Assert::IsTrue(sector != nullptr && sector->holder == BLUE, L"it went beyond the sectors its player holds");
            }
          }
          blue->Send(std::move(order));
        }
      }
      for (const Outpost::Snapshot& snapshot : red->Receive())
      {
        if (snapshot.tick >= handOver && snapshot.tick % tps == 0)
          aiIdleness.Sample(snapshot);
        for (Outpost::Command& order : redAi.Update(snapshot))
          red->Send(std::move(order));
      }
    }
    Logger::WriteMessage(
      std::format("Idle with Ore to spend, minutes 10 to 20: the deputy's Shipyards {:.1f}% of {} s, its Lab {:.1f}% of {} s; "
                  "the AI's Shipyards {:.1f}% of {} s, its Lab {:.1f}% of {} s.\n",
                  100.0 * deputyIdleness.YardShare(), deputyIdleness.yardSeconds, 100.0 * deputyIdleness.LabShare(),
                  deputyIdleness.labSeconds, 100.0 * aiIdleness.YardShare(), aiIdleness.yardSeconds, 100.0 * aiIdleness.LabShare(),
                  aiIdleness.labSeconds)
        .c_str());
    Assert::IsTrue(deputyIdleness.yardSeconds > 0 && deputyIdleness.labSeconds > 0, L"blue had a Shipyard and a Lab to keep");
    Assert::IsTrue(deputyIdleness.YardShare() <= aiIdleness.YardShare() + 0.05, L"its Shipyards stood idle more than the AI's");
    Assert::IsTrue(deputyIdleness.LabShare() <= aiIdleness.LabShare() + 0.05, L"its Lab stood idle more than the AI's");
  }
};
} // namespace GameLogicTests
