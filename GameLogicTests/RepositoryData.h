#pragma once

namespace GameLogicTests
{
// The text of a data file the game ships, from OutpostCommander/Assets in the repository (ADR-008). UseFullPaths makes
// __FILE__ absolute, so the repository is two levels up from this header.
std::string ReadRepositoryData(std::string_view _fileName);

// OutpostCommander/Assets/Tuning.json, which Tools/BattleModel.py reads too.
std::string ReadRepositoryTuning();

// OutpostCommander/Assets/Map.json.
std::string ReadRepositoryMap();

// The repository's OutpostCommander folder, whose Assets folder the package carries (ADR-008).
std::filesystem::path RepositoryHome();

// FileSys pointed at _home for one test, as WinMain points it at the package's folder, and put back exactly as it was
// afterwards. It reaches FileSys's state as a derived class can.
class ScopedHomeDirectory : Neuron::FileSys
{
public:
  explicit ScopedHomeDirectory(const std::filesystem::path& _home);

  ScopedHomeDirectory(const ScopedHomeDirectory&) = delete;
  ScopedHomeDirectory& operator=(const ScopedHomeDirectory&) = delete;
  ScopedHomeDirectory(ScopedHomeDirectory&&) = delete;
  ScopedHomeDirectory& operator=(ScopedHomeDirectory&&) = delete;

  ~ScopedHomeDirectory();

private:
  std::wstring m_previous;
};

// A home directory of its own in the temporary folder, with an empty Assets folder, that FileSys reads from for one test,
// and that is deleted afterwards. _name keeps two that are alive at once apart.
class TemporaryHomeDirectory
{
public:
  explicit TemporaryHomeDirectory(std::wstring_view _name);

  TemporaryHomeDirectory(const TemporaryHomeDirectory&) = delete;
  TemporaryHomeDirectory& operator=(const TemporaryHomeDirectory&) = delete;
  TemporaryHomeDirectory(TemporaryHomeDirectory&&) = delete;
  TemporaryHomeDirectory& operator=(TemporaryHomeDirectory&&) = delete;

  // A folder that cannot be deleted is left in the temporary folder, and the log says why, rather than failing the test
  // after it has passed.
  ~TemporaryHomeDirectory();

  [[nodiscard]] const std::filesystem::path& Root() const noexcept;

  // Writes _contents to _name under the Assets folder as they are, making the folders it names.
  void WriteAsset(const std::filesystem::path& _name, std::string_view _contents) const;

private:
  std::filesystem::path m_root;
  ScopedHomeDirectory m_home;
};
} // namespace GameLogicTests
