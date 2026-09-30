#pragma once

namespace GameLogicTests
{
// The text of a file under Data/, the files the game ships (ADR-008). UseFullPaths makes __FILE__ absolute, so the
// repository is two levels up from this header.
inline std::string ReadRepositoryData(std::string_view _fileName)
{
  const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path().parent_path() / "Data" / _fileName;
  std::ifstream file(path, std::ios::binary);
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(file.is_open(), path.wstring().c_str());
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

// Data/Tuning.json, which Tools/BattleModel.py reads too.
inline std::string ReadRepositoryTuning()
{
  return ReadRepositoryData("Tuning.json");
}

// Data/Map.json.
inline std::string ReadRepositoryMap()
{
  return ReadRepositoryData("Map.json");
}
} // namespace GameLogicTests
