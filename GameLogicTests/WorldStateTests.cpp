#include "pch.h"

#include "WorldMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
using Bytes = std::vector<std::byte>;

// The layout of each state version's save, as the hash of SaveLayout (AGENTS.md R18): the simulation's state, and from
// version 4 the seats' reports beside it. Version 5 adds when a remembered structure was last seen, and version 6 a world's
// rules and each player's loss (Phase 5 design §8). A change to either changes the layout: raise WORLD_STATE_VERSION and
// record the new version's hash here, below the old.
constexpr std::array<std::pair<std::uint32_t, std::uint64_t>, 6> LAYOUTS{{{1, 0x5EB85327A281B46Cull},
                                                                          {2, 0x8F33507C8E312761ull},
                                                                          {3, 0x4D954F637F96B5C2ull},
                                                                          {4, 0xA1F1A0DC52EC7069ull},
                                                                          {5, 0x2EC92634EE22BAC5ull},
                                                                          {6, 0xC7C5E5F3DF7A3234ull}}};

std::uint64_t LayoutHash()
{
  const std::string layout = Outpost::SaveLayout();
  const std::array<std::string_view, 1> texts{layout};
  return Outpost::DataHash(texts);
}

// _bytes with their checksum made again, as a save altered on purpose would have it.
Bytes Rechecked(Bytes _bytes)
{
  _bytes.resize(_bytes.size() - sizeof(std::uint64_t));
  std::uint64_t hash = 0xCBF29CE484222325ull;
  for (const std::byte byte : _bytes)
    hash = (hash ^ static_cast<std::uint64_t>(byte)) * 0x100000001B3ull;
  for (std::size_t i = 0; i < sizeof(hash); ++i)
    _bytes.push_back(static_cast<std::byte>(hash >> (8 * i)));
  return _bytes;
}

// What both players are shown after one more tick of _simulation with no commands, as the wire would carry it.
std::array<Bytes, 2> NextSnapshots(Outpost::Simulation& _simulation)
{
  (void)_simulation.Tick({});
  return {Outpost::EncodeMessage(_simulation.BuildSnapshot(WorldMatch::BLUE)),
          Outpost::EncodeMessage(_simulation.BuildSnapshot(WorldMatch::RED))};
}

void ExpectRefused(std::span<const std::byte> _bytes, const Outpost::WorldIdentity& _identity, const WorldMatch& _match,
                   const wchar_t* _why)
{
  Outpost::Simulation blank = _match.Blank();
  Assert::ExpectException<Neuron::Exception>([&]() { Outpost::DecodeWorld(_bytes, _identity, blank); }, _why);
}
} // namespace

// ADR-077: a world's state is saved between two ticks and loaded into a simulation made for the same world.
TEST_CLASS(WorldStateTests)
{
public:
  // A world well into play, with fights, salvage and orders under way, saved and loaded, is the world it was: it compares
  // equal, saves to the same bytes, and shows both players the same as the world that was saved after a tick more.
  TEST_METHOD(LoadsTheWorldItSaved)
  {
    WorldMatch match(5);
    match.Run(15.0 * 60.0);
    Outpost::Simulation& world = match.Server().World();
    const Bytes saved = Outpost::EncodeWorld(world, match.Identity());

    const Outpost::SaveHeader header = Outpost::ReadSaveHeader(saved);
    Assert::IsTrue(header.identity == match.Identity());
    Assert::AreEqual(world.CurrentTick(), header.tick);

    Outpost::Simulation loaded = match.Blank();
    Outpost::DecodeWorld(saved, match.Identity(), loaded);
    Assert::IsTrue(loaded == world, L"the loaded world compares equal");
    Assert::IsTrue(Outpost::EncodeWorld(loaded, match.Identity()) == saved, L"and saves to the same bytes");
    Assert::IsTrue(NextSnapshots(loaded) == NextSnapshots(world), L"and shows the players the same");
  }

  // Design §11 (ADR-080): a save keeps each seat's report of a time away beside the state, and gives them back.
  TEST_METHOD(KeepsTheSeatsReports)
  {
    WorldMatch match(5);
    match.Run(1.0);
    const Outpost::SeatReports reports{
      {Outpost::PlayerId{2},
       {.sinceTick = 7,
        .shipsLost = 3,
        .sectorsGained = {4},
        .ordersFired = {{.kind = Outpost::EventKind::OrderFired, .order = 9, .outcome = Outpost::OrderOutcome::HeldInstead}}}},
      {Outpost::PlayerId{1}, {.sinceTick = 8, .structuresBuilt = 1}}};
    const Bytes saved = Outpost::EncodeWorld(match.Server().World(), match.Identity(), reports);
    Outpost::Simulation loaded = match.Blank();
    Assert::IsTrue(Outpost::DecodeWorld(saved, match.Identity(), loaded) == reports);
    Assert::IsTrue(loaded == match.Server().World());
    Outpost::Simulation again = match.Blank();
    Assert::IsTrue(Outpost::DecodeWorld(Outpost::EncodeWorld(match.Server().World(), match.Identity()), match.Identity(), again).empty(),
                   L"a world with none keeps none");
  }

  // Design §5: a world loaded from a save and given the commands the running world applied after it is, ticks later, the
  // world that never stopped, to the bit, on the same build (ADR-009). The path graphs it builds again are the very graphs
  // the running world kept up to date (ADR-054).
  TEST_METHOD(RunsOnAsIfItNeverStopped)
  {
    WorldMatch match(9);
    std::vector<Bytes> saves;
    for (int save = 0; save < 3; ++save)
    {
      match.Run(6.0 * 60.0);
      saves.push_back(Outpost::EncodeWorld(match.Server().World(), match.Identity()));
    }
    match.Run(6.0 * 60.0);
    const Outpost::Simulation& world = match.Server().World();

    for (const Bytes& save : saves)
    {
      Outpost::Simulation loaded = match.Blank();
      Outpost::DecodeWorld(save, match.Identity(), loaded);
      match.Replay(loaded, world.CurrentTick());
      Assert::IsTrue(loaded == world,
                     std::format(L"the world loaded at tick {} runs on as the world did", Outpost::ReadSaveHeader(save).tick).c_str());
    }
  }

  // A save is refused when it is cut short, altered, of another world or data, or of another state version.
  TEST_METHOD(RefusesWhatItCannotLoad)
  {
    WorldMatch match(2);
    match.Run(60.0);
    const Bytes saved = Outpost::EncodeWorld(match.Server().World(), match.Identity());

    ExpectRefused(std::span(saved).first(saved.size() - 1), match.Identity(), match, L"cut short");
    ExpectRefused(std::span(saved).first(20), match.Identity(), match, L"only its header");
    Bytes altered = saved;
    altered[saved.size() / 2] ^= std::byte{0x01};
    ExpectRefused(altered, match.Identity(), match, L"altered");

    Outpost::WorldIdentity otherData = match.Identity();
    ++otherData.dataHash;
    ExpectRefused(saved, otherData, match, L"other data");
    Outpost::WorldIdentity otherSeed = match.Identity();
    ++otherSeed.seed;
    ExpectRefused(saved, otherSeed, match, L"another world");

    // The version follows the save's kind, eight bytes in.
    Bytes otherVersion = saved;
    otherVersion[8] = static_cast<std::byte>(Outpost::WORLD_STATE_VERSION + 1);
    otherVersion = Rechecked(std::move(otherVersion));
    ExpectRefused(otherVersion, match.Identity(), match, L"another state version");
    Assert::ExpectException<Neuron::Exception>([&]() { (void)Outpost::ReadSaveHeader(otherVersion); });

    // Not a save at all, though its checksum holds.
    Bytes notASave = saved;
    notASave[0] = std::byte{'X'};
    ExpectRefused(Rechecked(std::move(notASave)), match.Identity(), match, L"not a save");
  }

  // AGENTS.md R18: the layout a save of WORLD_STATE_VERSION holds is the one recorded for it. A change to what Simulation
  // holds fails this until the version is raised and its layout recorded.
  TEST_METHOD(TheLayoutIsTheVersions)
  {
    const auto recorded = std::ranges::find(LAYOUTS, Outpost::WORLD_STATE_VERSION, &std::pair<std::uint32_t, std::uint64_t>::first);
    Assert::IsTrue(recorded != LAYOUTS.end(), L"WORLD_STATE_VERSION has a recorded layout");
    Assert::AreEqual(
      recorded->second, LayoutHash(),
      L"what Simulation holds has changed: raise WORLD_STATE_VERSION and record its layout's hash in LAYOUTS (AGENTS.md R18)");
    // Each version's layout is recorded once, and none is reused.
    for (std::size_t i = 1; i < LAYOUTS.size(); ++i)
    {
      Assert::IsTrue(LAYOUTS[i].first > LAYOUTS[i - 1].first);
      Assert::IsTrue(LAYOUTS[i].second != LAYOUTS[i - 1].second);
    }
  }

  // The layout follows the field lists: an entity is saved with every one of its sixty fields, and a player with its
  // thirteen, all but its research effects.
  TEST_METHOD(TheLayoutNamesEveryField)
  {
    const std::string layout = Outpost::Simulation::StateLayout();
    Assert::IsTrue(layout.find("v(r60{") != std::string::npos, L"the entities, sixty fields each");
    Assert::IsTrue(layout.find("v(r13{") != std::string::npos, L"the players, thirteen fields each");
  }
};
} // namespace GameLogicTests
