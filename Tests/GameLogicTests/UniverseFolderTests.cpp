#include "pch.h"

#include "UniverseMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr std::uint32_t TICKS_PER_SAVE = Outpost::UNIVERSE_SAVE_SECONDS * 20;

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

// Runs a universe in _folder for _seconds, two AIs playing, with an order of blue's on its last tick, so that the log
// reaches the end of what it ran; and returns the universe as it stood when it was left. The server is gone on return, its
// saves written.
Outpost::Simulation RunUniverse(const std::filesystem::path& _folder, std::uint64_t _seed, double _seconds)
{
  UniverseMatch match(_seed, _folder);
  match.Run(_seconds - 0.05);
  match.Send(UniverseMatch::BLUE, Outpost::StopCommand{});
  match.Run(0.05);
  return match.Server().SimulationState();
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

// ADR-077 decision 7: a universe's folder keeps its saves and its command log, and a universe comes back from them.
TEST_CLASS(UniverseFolderTests)
{
public:
  // A new universe is saved at once, so that it comes back even if it stops before its first minute.
  TEST_METHOD(SavesANewUniverseAtOnce)
  {
    const TemporaryFolder folder;
    {
      UniverseMatch match(6, folder.Path());
    }
    const std::optional<Outpost::UniverseFolder::Recovery> found = Outpost::UniverseFolder::Recover(folder.Path());
    Assert::IsTrue(found.has_value());
    const Outpost::UniverseFolder::Recovery recovery = found.value_or(Outpost::UniverseFolder::Recovery{});
    Assert::AreEqual(std::uint64_t{0}, recovery.header.tick);
    Assert::AreEqual(std::uint64_t{6}, recovery.header.identity.seed);
    Assert::IsTrue(recovery.commands.empty());
    Assert::IsFalse(Outpost::UniverseFolder::Recover(folder.Path() / "none").has_value(),
                    L"a folder that does not exist holds no universe");
  }

  // A universe left and opened again is the universe that ran, to the bit, on the same build, whatever seed it is opened with:
  // the save it made and the commands it logged since bring it back to its last tick.
  TEST_METHOD(ComesBackAsTheUniverseThatRan)
  {
    const TemporaryFolder folder;
    const Outpost::Simulation ran = RunUniverse(folder.Path(), 4, 5.5 * 60.0);
    UniverseMatch opened(999, folder.Path());
    Assert::AreEqual(ran.CurrentTick(), opened.Server().SimulationState().CurrentTick());
    Assert::IsTrue(opened.Server().SimulationState() == ran);
    Assert::AreEqual(std::uint64_t{4}, opened.Identity().seed);
    // And it plays on.
    opened.Run(30.0);
  }

  // The newest three saves are kept, a save every minute, and the logs from the oldest of them on.
  TEST_METHOD(KeepsTheNewestThreeSaves)
  {
    const TemporaryFolder folder;
    (void)RunUniverse(folder.Path(), 4, 5.5 * 60.0);
    const std::vector<std::filesystem::path> saves = FilesOf(folder.Path(), "Universe-");
    const std::vector<std::filesystem::path> logs = FilesOf(folder.Path(), "Commands-");
    Assert::AreEqual(Outpost::UniverseFolder::SAVES_KEPT, saves.size());
    Assert::AreEqual(saves.size(), logs.size());
    for (std::size_t i = 0; i < saves.size(); ++i)
    {
      const std::uint64_t tick = TICKS_PER_SAVE * (3 + i);
      Assert::AreEqual(std::format("Universe-{:020}.save", tick), saves[i].filename().string());
      Assert::AreEqual(std::format("Commands-{:020}.log", tick), logs[i].filename().string());
    }
  }

  // ADR-082 decision 1: the folder's thread times each save it writes, for the universe's log, and hands each over once.
  TEST_METHOD(TimesEachWrite)
  {
    const TemporaryFolder folder;
    Outpost::UniverseFolder universe(folder.Path());
    universe.Save(0, std::vector<std::byte>(1'000, std::byte{1}));
    universe.Save(TICKS_PER_SAVE, std::vector<std::byte>(2'000, std::byte{2}));
    universe.Flush();
    const std::vector<Outpost::WrittenSave> written = universe.TakeWritten();
    Assert::AreEqual(std::size_t{2}, written.size());
    Assert::AreEqual(std::uint64_t{0}, written[0].tick);
    Assert::AreEqual(std::uint64_t{TICKS_PER_SAVE}, written[1].tick);
    Assert::IsTrue(written[0].write.count() >= 0 && written[1].write.count() >= 0, L"a time each");
    Assert::IsTrue(universe.TakeWritten().empty(), L"each handed over once");
  }

  // A save that does not read whole is passed over for the one before it, whose log then brings the universe back all the same.
  TEST_METHOD(ComesBackPastABrokenSave)
  {
    const TemporaryFolder folder;
    const Outpost::Simulation ran = RunUniverse(folder.Path(), 8, 5.5 * 60.0);
    CutShort(FilesOf(folder.Path(), "Universe-").back(), 100);
    const std::optional<Outpost::UniverseFolder::Recovery> recovery = Outpost::UniverseFolder::Recover(folder.Path());
    Assert::IsTrue(recovery.has_value());
    Assert::AreEqual(std::uint64_t{4} * TICKS_PER_SAVE, recovery.value_or(Outpost::UniverseFolder::Recovery{}).header.tick);
    UniverseMatch opened(8, folder.Path());
    Assert::IsTrue(opened.Server().SimulationState() == ran);
  }

  // A process killed while it wrote a log record leaves the record cut short at the end of the newest log, which is
  // passed over. Cut short anywhere else, the log is broken, and the universe does not come back without saying why.
  TEST_METHOD(PassesOverALastRecordCutShort)
  {
    const TemporaryFolder folder;
    const Outpost::Simulation ran = RunUniverse(folder.Path(), 4, 2.5 * 60.0);
    const std::vector<std::filesystem::path> logs = FilesOf(folder.Path(), "Commands-");
    Append(logs.back(), 5);
    {
      UniverseMatch opened(4, folder.Path());
      Assert::IsTrue(opened.Server().SimulationState() == ran, L"a header cut short");
    }

    const TemporaryFolder second;
    const Outpost::Simulation secondRan = RunUniverse(second.Path(), 4, 2.5 * 60.0);
    Append(FilesOf(second.Path(), "Commands-").back(), 30);
    {
      UniverseMatch opened(4, second.Path());
      Assert::IsTrue(opened.Server().SimulationState() == secondRan, L"a command cut short");
    }

    // A log is read only from the save the universe comes back from: here the newest save is broken, so the one before it is
    // read with its log, which is cut short before the newest log begins.
    const TemporaryFolder third;
    (void)RunUniverse(third.Path(), 4, 3.5 * 60.0);
    const std::vector<std::filesystem::path> thirdLogs = FilesOf(third.Path(), "Commands-");
    Assert::IsTrue(std::filesystem::file_size(thirdLogs[1]) > 20, L"the AIs gave orders in the middle log's minute");
    CutShort(thirdLogs[1], 3);
    (void)Outpost::UniverseFolder::Recover(third.Path());
    CutShort(FilesOf(third.Path(), "Universe-").back(), 1);
    Assert::ExpectException<Neuron::Exception>([&]() { (void)Outpost::UniverseFolder::Recover(third.Path()); }, L"an older log cut short");
  }

  // A folder of another universe's data, or with saves none of which reads whole, is refused rather than started anew.
  TEST_METHOD(RefusesWhatItCannotComeBackFrom)
  {
    const TemporaryFolder folder;
    (void)RunUniverse(folder.Path(), 3, 1.5 * 60.0);
    Outpost::InProcessServer server(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()), {.seed = 3});
    Assert::ExpectException<Neuron::Exception>(
      [&]()
      { server.UseUniverse(folder.Path(), UniverseMatch::RepositoryDataHash() + 1, Outpost::UniverseFolder::Recover(folder.Path())); },
      L"other data");

    for (const std::filesystem::path& save : FilesOf(folder.Path(), "Universe-"))
      CutShort(save, 1);
    Assert::ExpectException<Neuron::Exception>([&]() { (void)Outpost::UniverseFolder::Recover(folder.Path()); }, L"no save reads whole");
  }

  // What a save of a late universe costs the server's thread, and its size. Run on its own, with
  // OUTPOST_UNIVERSE_MEASURE set, since it plays an hour of a universe.
  TEST_METHOD(MeasuresALateSave)
  {
    if (GetEnvironmentVariableW(L"OUTPOST_UNIVERSE_MEASURE", nullptr, 0) == 0)
    {
      Logger::WriteMessage("Not run here: OUTPOST_UNIVERSE_MEASURE runs it.");
      return;
    }
    for (const std::uint64_t seed : {1, 2, 3})
    {
      UniverseMatch match(seed);
      match.Run(60.0 * 60.0);
      constexpr int SAVES = 20;
      std::size_t bytes = 0;
      const auto started = std::chrono::steady_clock::now();
      for (int i = 0; i < SAVES; ++i)
        bytes = Outpost::EncodeUniverse(match.Server().SimulationState(), match.Identity()).size();
      const std::chrono::duration<double, std::milli> each = (std::chrono::steady_clock::now() - started) / SAVES;
      Logger::WriteMessage(std::format("seed {}, minute 60: {} entities, a save of {} bytes encoded in {:.2f} ms", seed,
                                       match.Server().SimulationState().Entities().size(), bytes, each.count())
                             .c_str());
    }
  }
};
} // namespace GameLogicTests
