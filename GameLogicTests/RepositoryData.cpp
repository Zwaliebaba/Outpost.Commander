#include "pch.h"

#include "RepositoryData.h"

namespace GameLogicTests
{
std::string ReadRepositoryData(std::string_view _fileName)
{
  const std::filesystem::path path =
    std::filesystem::path(__FILE__).parent_path().parent_path() / "OutpostCommander" / "Assets" / _fileName;
  std::ifstream file(path, std::ios::binary);
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(file.is_open(), path.wstring().c_str());
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

std::string ReadRepositoryTuning()
{
  return ReadRepositoryData("Tuning.json");
}

std::string ReadRepositoryMap()
{
  return ReadRepositoryData("Map.json");
}

std::filesystem::path RepositoryHome()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "OutpostCommander";
}

ScopedHomeDirectory::ScopedHomeDirectory(const std::filesystem::path& _home)
  : m_previous(sm_homeDir)
{
  SetHomeDirectory(_home.wstring());
}

ScopedHomeDirectory::~ScopedHomeDirectory()
{
  sm_homeDir.swap(m_previous);
}

TemporaryHomeDirectory::TemporaryHomeDirectory(std::wstring_view _name)
  : m_root(std::filesystem::temp_directory_path() / std::format(L"OutpostCommander-{}-{}", _name, GetCurrentProcessId())),
    m_home(m_root)
{
  std::filesystem::remove_all(m_root);
  std::filesystem::create_directories(m_root / L"Assets");
}

TemporaryHomeDirectory::~TemporaryHomeDirectory()
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

const std::filesystem::path& TemporaryHomeDirectory::Root() const noexcept
{
  return m_root;
}

void TemporaryHomeDirectory::WriteAsset(const std::filesystem::path& _name, std::string_view _contents) const
{
  const std::filesystem::path path = m_root / L"Assets" / _name;
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(_contents.data(), static_cast<std::streamsize>(_contents.size()));
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(file.good(), path.wstring().c_str());
}
} // namespace GameLogicTests
