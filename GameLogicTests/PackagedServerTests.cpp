#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};

// The server behind the interface CreateInProcessServer and the factory return, so that a test can count its world.
Outpost::InProcessServer& Concrete(Outpost::Server& _server)
{
  auto* server = dynamic_cast<Outpost::InProcessServer*>(&_server);
  Assert::IsNotNull(server);
  return *server;
}

// Both players' last snapshots after _ticks steps of _server.
std::array<Outpost::Snapshot, 2> StepBoth(Outpost::Server& _server, int _ticks)
{
  const std::unique_ptr<Outpost::Transport> blue = _server.Connect(BLUE);
  const std::unique_ptr<Outpost::Transport> red = _server.Connect(RED);
  for (int i = 0; i < _ticks; ++i)
    _server.Step();
  std::vector<Outpost::Snapshot> blueSnapshots = blue->Receive();
  std::vector<Outpost::Snapshot> redSnapshots = red->Receive();
  Assert::AreEqual(static_cast<size_t>(_ticks), blueSnapshots.size());
  Assert::AreEqual(static_cast<size_t>(_ticks), redSnapshots.size());
  return {std::move(blueSnapshots.back()), std::move(redSnapshots.back())};
}

struct Census
{
  size_t ships = 0;
  size_t warships = 0;
  size_t structures = 0;
};

Census Count(const Outpost::Simulation& _simulation, std::optional<Outpost::PlayerId> _owner = std::nullopt)
{
  Census census;
  for (const Outpost::Entity& entity : _simulation.Entities())
  {
    if (_owner.has_value() && entity.owner != *_owner)
      continue;
    if (entity.kind == Outpost::EntityKind::Ship)
    {
      ++census.ships;
      if (entity.role == Outpost::ShipRole::Warship)
        ++census.warships;
    }
    else if (entity.kind == Outpost::EntityKind::Structure)
    {
      ++census.structures;
    }
  }
  return census;
}

template <typename Fn> std::string FailureOf(Fn _make)
{
  std::string message;
  try
  {
    _make();
  }
  catch (const Neuron::Exception& error)
  {
    message = error.what();
  }
  Assert::IsFalse(message.empty(), L"the server was made");
  return message;
}

void Create()
{
  (void)Outpost::CreateInProcessServer({.seed = 1});
}

void MakeFactory()
{
  (void)Outpost::InProcessServerFactory();
}
} // namespace

// The executable starts its server with CreateInProcessServer, and the AI-against-AI matches make theirs with
// InProcessServerFactory (Server.h): both read Tuning.json and Map.json from the package's Assets folder, through FileSys,
// and set the match up. These tests point FileSys at the repository's OutpostCommander folder, as WinMain points it at the
// package's.
TEST_CLASS(PackagedServerTests)
{
public:
  // A match starts from the packaged data, at its tuned rate, with each player's starting base placed (ADR-016).
  TEST_METHOD(StartsAMatchFromThePackagedData)
  {
    const ScopedHomeDirectory home(RepositoryHome());
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const std::unique_ptr<Outpost::Server> server = Outpost::CreateInProcessServer({.seed = 1});
    Assert::AreEqual(static_cast<std::uint32_t>(tuning.rules.tickHz), server->TicksPerSecond());

    for (const Outpost::Snapshot& snapshot : StepBoth(*server, 1))
    {
      Assert::AreEqual(std::uint64_t{1}, snapshot.tick);
      size_t stations = 0;
      size_t constructors = 0;
      for (const Outpost::EntityView& entity : snapshot.entities)
      {
        if (entity.owner != snapshot.player)
          continue;
        if (entity.kind == Outpost::EntityKind::Structure && entity.structure == Outpost::StructureKind::CommandStation &&
            entity.builtPermille == Outpost::PERMILLE)
          ++stations;
        else if (entity.kind == Outpost::EntityKind::Ship && entity.role == Outpost::ShipRole::Constructor)
          ++constructors;
      }
      Assert::AreEqual(size_t{1}, stations);
      Assert::AreEqual(static_cast<size_t>(tuning.rules.startingConstructors), constructors);
    }
  }

  // The factory's servers start the same match from the same seed as CreateInProcessServer's.
  TEST_METHOD(TheFactoryMakesServersAsCreateInProcessServerDoes)
  {
    const ScopedHomeDirectory home(RepositoryHome());
    const Outpost::ServerFactory factory = Outpost::InProcessServerFactory();
    const std::unique_ptr<Outpost::Server> made = factory({.seed = 7});
    const std::unique_ptr<Outpost::Server> created = Outpost::CreateInProcessServer({.seed = 7});
    Assert::AreEqual(created->TicksPerSecond(), made->TicksPerSecond());

    const std::array<Outpost::Snapshot, 2> fromFactory = StepBoth(*made, 3);
    const std::array<Outpost::Snapshot, 2> fromCreate = StepBoth(*created, 3);
    for (size_t seat = 0; seat < fromFactory.size(); ++seat)
    {
      Assert::AreEqual(fromCreate[seat].tick, fromFactory[seat].tick);
      Assert::IsFalse(fromFactory[seat].entities.empty());
      Assert::IsTrue(fromCreate[seat].entities == fromFactory[seat].entities);
    }
  }

  // The factory reads and checks the data once, when it is made: its servers need no files, and each has a match of its
  // own.
  TEST_METHOD(TheFactoryReadsTheDataOnce)
  {
    Outpost::ServerFactory factory;
    {
      const ScopedHomeDirectory home(RepositoryHome());
      factory = Outpost::InProcessServerFactory();
    }
    const TemporaryHomeDirectory empty(L"PackagedServerTests");
    (void)FailureOf(Create);

    const std::unique_ptr<Outpost::Server> first = factory({.seed = 1});
    const std::unique_ptr<Outpost::Server> second = factory({.seed = 1});
    const std::array<Outpost::Snapshot, 2> firstSnapshots = StepBoth(*first, 4);
    const std::array<Outpost::Snapshot, 2> secondSnapshots = StepBoth(*second, 1);
    Assert::AreEqual(std::uint64_t{4}, firstSnapshots[0].tick);
    Assert::AreEqual(std::uint64_t{1}, secondSnapshots[0].tick, L"stepping one server does not step another");
  }

  // A file that is missing is named, whichever of the two it is. Only one is missing at a time: the two are read in an
  // order the language leaves open.
  TEST_METHOD(RefusesDataThatIsMissing)
  {
    const TemporaryHomeDirectory home(L"PackagedServerTests");
    home.WriteAsset(L"Map.json", ReadRepositoryMap());
    const std::string noTuning("The game data file Assets\\Tuning.json is missing or cannot be read.");
    Assert::AreEqual(noTuning, FailureOf(Create));
    Assert::AreEqual(noTuning, FailureOf(MakeFactory));

    std::filesystem::remove(home.Root() / L"Assets" / L"Map.json");
    home.WriteAsset(L"Tuning.json", ReadRepositoryTuning());
    const std::string noMap("The game data file Assets\\Map.json is missing or cannot be read.");
    Assert::AreEqual(noMap, FailureOf(Create));
    Assert::AreEqual(noMap, FailureOf(MakeFactory));
  }

  // Data that is there but invalid fails as its loader says, naming which of the two it is.
  TEST_METHOD(RefusesDataThatIsInvalid)
  {
    const TemporaryHomeDirectory home(L"PackagedServerTests");
    home.WriteAsset(L"Tuning.json", "{}");
    home.WriteAsset(L"Map.json", ReadRepositoryMap());
    Assert::IsTrue(FailureOf(Create).starts_with("Tuning: "));
    Assert::IsTrue(FailureOf(MakeFactory).starts_with("Tuning: "));

    home.WriteAsset(L"Tuning.json", ReadRepositoryTuning());
    home.WriteAsset(L"Map.json", "[]");
    Assert::IsTrue(FailureOf(Create).starts_with("Map: "));
    Assert::IsTrue(FailureOf(MakeFactory).starts_with("Map: "));
  }

  // A measurement run's server holds task 2.7's load, and a match's does not.
  TEST_METHOD(PlacesTheMeasurementLoadOnlyWhenAsked)
  {
    const ScopedHomeDirectory home(RepositoryHome());
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const std::unique_ptr<Outpost::Server> match = Outpost::CreateInProcessServer({.seed = 1});
    const Census plain = Count(Concrete(*match).World());
    Assert::AreEqual(size_t{2} * static_cast<size_t>(tuning.rules.startingConstructors),
                     Count(Concrete(*match).World(), BLUE).ships + Count(Concrete(*match).World(), RED).ships);
    Assert::IsTrue(plain.ships < Outpost::MEASUREMENT_SHIPS);

    const std::unique_ptr<Outpost::Server> measured = Outpost::CreateInProcessServer({.seed = 1, .measurementLoad = true});
    const Census load = Count(Concrete(*measured).World());
    Assert::AreEqual(Outpost::MEASUREMENT_SHIPS, load.ships);
    Assert::AreEqual(Outpost::MEASUREMENT_STRUCTURES, load.structures);
  }

  // A stress run's server sets task 3.7's scene up, and fills both fleets as its first tick starts.
  TEST_METHOD(StartsTheStressLoadOnlyWhenAsked)
  {
    const ScopedHomeDirectory home(RepositoryHome());
    const std::unique_ptr<Outpost::Server> match = Outpost::CreateInProcessServer({.seed = 3});
    (void)StepBoth(*match, 1);
    Assert::AreEqual(size_t{0}, Count(Concrete(*match).World(), BLUE).warships);

    const std::unique_ptr<Outpost::Server> stressed = Outpost::CreateInProcessServer({.seed = 3, .stressLoad = true});
    for (const Outpost::PlayerId player : {BLUE, RED})
      Assert::AreEqual(Outpost::STRESS_STRUCTURES_PER_PLAYER, Count(Concrete(*stressed).World(), player).structures);
    (void)StepBoth(*stressed, 1);
    for (const Outpost::PlayerId player : {BLUE, RED})
      Assert::AreEqual(Outpost::STRESS_SHIPS_PER_PLAYER, Count(Concrete(*stressed).World(), player).warships);
  }

  // ADR-032: match setup builds the path graphs, so that the first order of the match does not pay for them. A server set
  // up without that builds them in the order's tick, which shows that the tick's timing would see it.
  TEST_METHOD(TheFirstOrderBuildsNoPathGraph)
  {
    const ScopedHomeDirectory home(RepositoryHome());
    const auto graphBuildOfFirstOrder = [](Outpost::Server& _server)
    {
      const std::unique_ptr<Outpost::Transport> blue = _server.Connect(BLUE);
      _server.Step();
      Outpost::MoveCommand move{.destination = {.xMeters = 600.0f, .zMeters = 600.0f}};
      for (const Outpost::Snapshot& snapshot : blue->Receive())
      {
        for (const Outpost::EntityView& entity : snapshot.entities)
        {
          if (entity.kind == Outpost::EntityKind::Ship && entity.owner == BLUE)
            move.ships.push_back(entity.id);
        }
      }
      Assert::IsFalse(move.ships.empty());
      (void)_server.TakeTickTimings();
      blue->Send({.order = move});
      _server.Step();
      const std::vector<Outpost::TickTiming> timings = _server.TakeTickTimings();
      Assert::AreEqual(size_t{1}, timings.size());
      Assert::IsTrue(timings.front().Part(Outpost::TickPart::ShipPaths).count() > 0, L"the order found paths");
      return timings.front().Part(Outpost::TickPart::GraphBuild);
    };

    const std::unique_ptr<Outpost::Server> prepared = Outpost::CreateInProcessServer({.seed = 1});
    Assert::AreEqual(std::int64_t{0}, static_cast<std::int64_t>(graphBuildOfFirstOrder(*prepared).count()));

    Outpost::InProcessServer unprepared(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()), {.seed = 1});
    unprepared.World().PlaceStartingBases(unprepared.MapData());
    Assert::IsTrue(graphBuildOfFirstOrder(unprepared).count() > 0);
  }
};
} // namespace GameLogicTests
