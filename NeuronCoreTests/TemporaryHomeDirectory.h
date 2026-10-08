#pragma once

namespace NeuronCoreTests
{
// FileSys pointed at _home for one test, as WinMain points it at the package's folder, and put back exactly as it was
// afterwards. It reaches FileSys's state as a derived class can.
class ScopedHomeDirectory : Neuron::FileSys
{
public:
  explicit ScopedHomeDirectory(const std::filesystem::path& _home)
    : m_previous(sm_homeDir)
  {
    SetHomeDirectory(_home.wstring());
  }

  ScopedHomeDirectory(const ScopedHomeDirectory&) = delete;
  ScopedHomeDirectory& operator=(const ScopedHomeDirectory&) = delete;
  ScopedHomeDirectory(ScopedHomeDirectory&&) = delete;
  ScopedHomeDirectory& operator=(ScopedHomeDirectory&&) = delete;

  ~ScopedHomeDirectory()
  {
    sm_homeDir.swap(m_previous);
  }

private:
  std::wstring m_previous;
};

// A home directory of its own in the temporary folder, with an empty Assets folder, that FileSys reads from for one test,
// and that is deleted afterwards. _name keeps two that are alive at once apart.
class TemporaryHomeDirectory
{
public:
  explicit TemporaryHomeDirectory(std::wstring_view _name)
    : m_root(std::filesystem::temp_directory_path() / std::format(L"OutpostCommander-{}-{}", _name, GetCurrentProcessId())),
      m_home(m_root)
  {
    std::filesystem::remove_all(m_root);
    std::filesystem::create_directories(m_root / L"Assets");
  }

  TemporaryHomeDirectory(const TemporaryHomeDirectory&) = delete;
  TemporaryHomeDirectory& operator=(const TemporaryHomeDirectory&) = delete;
  TemporaryHomeDirectory(TemporaryHomeDirectory&&) = delete;
  TemporaryHomeDirectory& operator=(TemporaryHomeDirectory&&) = delete;

  // A folder that cannot be deleted is left in the temporary folder, and the log says why, rather than failing the test
  // after it has passed.
  ~TemporaryHomeDirectory()
  {
    try
    {
      std::error_code ignored;
      std::filesystem::remove_all(m_root, ignored);
    }
    catch (const std::exception& error)
    {
      Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage(error.what());
    }
  }

  [[nodiscard]] const std::filesystem::path& Root() const noexcept
  {
    return m_root;
  }

  // Writes _contents to _name under the Assets folder as they are, making the folders it names.
  void WriteAsset(const std::filesystem::path& _name, std::string_view _contents) const
  {
    const std::filesystem::path path = m_root / L"Assets" / _name;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(_contents.data(), static_cast<std::streamsize>(_contents.size()));
    Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(file.good(), path.wstring().c_str());
  }

private:
  std::filesystem::path m_root;
  ScopedHomeDirectory m_home;
};
} // namespace NeuronCoreTests
