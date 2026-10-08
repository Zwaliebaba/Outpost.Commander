#include "pch.h"

#include "WorldMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr std::uint32_t TICKS_PER_SAVE = Outpost::WORLD_SAVE_SECONDS * 20;

// The files in _folder whose names start with _prefix, by name, which is by tick.
std::vector<std::filesystem::path> FilesOf(const std::filesystem::path& _folder, std::string_view _prefix)
{
  std::vector<std::filesystem::path> files;
  for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(_folder))
  {
    if (entry.path().filename().string().starts_with(_prefix))
      files.push_back(entry.path());
  }
  std::ranges::sort(files);
  return files;
}

// Runs a world in _folder for _seconds, two AIs playing, with an order of blue's on its last tick, so that the log
// reaches the end of what it ran; and returns the world as it stood when it was left. The server is gone on return, its
// saves written.
Outpost::Simulation RunWorld(const std::filesystem::path& _folder, std::uint64_t _seed, double _seconds)
{
  WorldMatch match(_seed, _folder);
  match.Run(_seconds - 0.05);
  match.Send(WorldMatch::BLUE, Outpost::StopCommand{});
  match.Run(0.05);
  return match.Server().World();
}

void Append(const std::filesystem::path& _file, std::size_t _bytes)
{
  std::ofstream file(_file, std::ios::binary | std::ios::app);
  const std::string tail(_bytes, '\x07');
  file.write(tail.data(), static_cast<std::streamsize>(tail.size()));
}

void CutShort(const std::filesystem::path& _file, std::uintmax_t _bytes)
{
  std::filesystem::resize_file(_file, std::filesystem::file_size(_file) - _bytes);
}
} // namespace

// Phase 5 design §5, ADR-077: a world's folder keeps its saves and its command log, and a world comes back from them.
TEST_CLASS(WorldFolderTests)
{
public:
  // A new world is saved at once, so that it comes back even if it stops before its first minute.
  TEST_METHOD(SavesANewWorldAtOnce)
  {
    const TemporaryFolder folder;
    {
      WorldMatch match(6, folder.Path());
    }
    const std::optional<Outpost::WorldFolder::Recovery> found = Outpost::WorldFolder::Recover(folder.Path());
    Assert::IsTrue(found.has_value());
    const Outpost::WorldFolder::Recovery recovery = found.value_or(Outpost::WorldFolder::Recovery{});
    Assert::AreEqual(std::uint64_t{0}, recovery.header.tick);
    Assert::AreEqual(std::uint64_t{6}, recovery.header.identity.seed);
    Assert::IsTrue(recovery.commands.empty());
    Assert::IsFalse(Outpost::WorldFolder::Recover(folder.Path() / "none").has_value(), L"a folder that does not exist holds no world");
  }

  // A world left and opened again is the world that ran, to the bit, on the same build, whatever seed it is opened with:
  // the save it made and the commands it logged since bring it back to its last tick.
  TEST_METHOD(ComesBackAsTheWorldThatRan)
  {
    const TemporaryFolder folder;
    const Outpost::Simulation ran = RunWorld(folder.Path(), 4, 5.5 * 60.0);
    WorldMatch opened(999, folder.Path());
    Assert::AreEqual(ran.CurrentTick(), opened.Server().World().CurrentTick());
    Assert::IsTrue(opened.Server().World() == ran);
    Assert::AreEqual(std::uint64_t{4}, opened.Identity().seed);
    // And it plays on.
    opened.Run(30.0);
  }

  // The newest three saves are kept, a save every minute, and the logs from the oldest of them on.
  TEST_METHOD(KeepsTheNewestThreeSaves)
  {
    const TemporaryFolder folder;
    (void)RunWorld(folder.Path(), 4, 5.5 * 60.0);
    const std::vector<std::filesystem::path> saves = FilesOf(folder.Path(), "World-");
    const std::vector<std::filesystem::path> logs = FilesOf(folder.Path(), "Commands-");
    Assert::AreEqual(Outpost::WorldFolder::SAVES_KEPT, saves.size());
    Assert::AreEqual(saves.size(), logs.size());
    for (std::size_t i = 0; i < saves.size(); ++i)
    {
      const std::uint64_t tick = TICKS_PER_SAVE * (3 + i);
      Assert::AreEqual(std::format("World-{:020}.save", tick), saves[i].filename().string());
      Assert::AreEqual(std::format("Commands-{:020}.log", tick), logs[i].filename().string());
    }
  }

  // A save that does not read whole is passed over for the one before it, whose log then brings the world back all the same.
  TEST_METHOD(ComesBackPastABrokenSave)
  {
    const TemporaryFolder folder;
    const Outpost::Simulation ran = RunWorld(folder.Path(), 8, 5.5 * 60.0);
    CutShort(FilesOf(folder.Path(), "World-").back(), 100);
    const std::optional<Outpost::WorldFolder::Recovery> recovery = Outpost::WorldFolder::Recover(folder.Path());
    Assert::IsTrue(recovery.has_value());
    Assert::AreEqual(std::uint64_t{4} * TICKS_PER_SAVE, recovery.value_or(Outpost::WorldFolder::Recovery{}).header.tick);
    WorldMatch opened(8, folder.Path());
    Assert::IsTrue(opened.Server().World() == ran);
  }

  // A process killed while it wrote a log record leaves the record cut short at the end of the newest log, which is
  // passed over. Cut short anywhere else, the log is broken, and the world does not come back without saying why.
  TEST_METHOD(PassesOverALastRecordCutShort)
  {
    const TemporaryFolder folder;
    const Outpost::Simulation ran = RunWorld(folder.Path(), 4, 2.5 * 60.0);
    const std::vector<std::filesystem::path> logs = FilesOf(folder.Path(), "Commands-");
    Append(logs.back(), 5);
    {
      WorldMatch opened(4, folder.Path());
      Assert::IsTrue(opened.Server().World() == ran, L"a header cut short");
    }

    const TemporaryFolder second;
    const Outpost::Simulation secondRan = RunWorld(second.Path(), 4, 2.5 * 60.0);
    Append(FilesOf(second.Path(), "Commands-").back(), 30);
    {
      WorldMatch opened(4, second.Path());
      Assert::IsTrue(opened.Server().World() == secondRan, L"a command cut short");
    }

    // A log is read only from the save the world comes back from: here the newest save is broken, so the one before it is
    // read with its log, which is cut short before the newest log begins.
    const TemporaryFolder third;
    (void)RunWorld(third.Path(), 4, 3.5 * 60.0);
    const std::vector<std::filesystem::path> thirdLogs = FilesOf(third.Path(), "Commands-");
    Assert::IsTrue(std::filesystem::file_size(thirdLogs[1]) > 20, L"the AIs gave orders in the middle log's minute");
    CutShort(thirdLogs[1], 3);
    (void)Outpost::WorldFolder::Recover(third.Path());
    CutShort(FilesOf(third.Path(), "World-").back(), 1);
    Assert::ExpectException<Neuron::Exception>([&]() { (void)Outpost::WorldFolder::Recover(third.Path()); }, L"an older log cut short");
  }

  // A folder of another world's data, or with saves none of which reads whole, is refused rather than started anew.
  TEST_METHOD(RefusesWhatItCannotComeBackFrom)
  {
    const TemporaryFolder folder;
    (void)RunWorld(folder.Path(), 3, 1.5 * 60.0);
    Outpost::InProcessServer server(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()), {.seed = 3});
    Assert::ExpectException<Neuron::Exception>(
      [&]() { server.UseWorld(folder.Path(), WorldMatch::RepositoryDataHash() + 1, Outpost::WorldFolder::Recover(folder.Path())); },
      L"other data");

    for (const std::filesystem::path& save : FilesOf(folder.Path(), "World-"))
      CutShort(save, 1);
    Assert::ExpectException<Neuron::Exception>([&]() { (void)Outpost::WorldFolder::Recover(folder.Path()); }, L"no save reads whole");
  }

  // W2 (Phase 5 design §2): what a save of a late world costs the server's thread, and its size. Run on its own, with
  // OUTPOST_WORLD_MEASURE set, since it plays an hour of world.
  TEST_METHOD(MeasuresALateSave)
  {
    if (GetEnvironmentVariableW(L"OUTPOST_WORLD_MEASURE", nullptr, 0) == 0)
    {
      Logger::WriteMessage("Not run here: OUTPOST_WORLD_MEASURE runs it.");
      return;
    }
    for (const std::uint64_t seed : {1, 2, 3})
    {
      WorldMatch match(seed);
      match.Run(60.0 * 60.0);
      constexpr int SAVES = 20;
      std::size_t bytes = 0;
      const auto started = std::chrono::steady_clock::now();
      for (int i = 0; i < SAVES; ++i)
        bytes = Outpost::EncodeWorld(match.Server().World(), match.Identity()).size();
      const std::chrono::duration<double, std::milli> each = (std::chrono::steady_clock::now() - started) / SAVES;
      Logger::WriteMessage(std::format("seed {}, minute 60: {} entities, a save of {} bytes encoded in {:.2f} ms", seed,
                                       match.Server().World().Entities().size(), bytes, each.count())
                             .c_str());
    }
  }
};
} // namespace GameLogicTests
