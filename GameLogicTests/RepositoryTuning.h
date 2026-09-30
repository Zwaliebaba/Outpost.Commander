#pragma once

namespace GameLogicTests
{
// The text of Data/Tuning.json, the file the game ships and Tools/BattleModel.py reads (ADR-008). UseFullPaths makes
// __FILE__ absolute, so the repository is two levels up from this header.
inline std::string ReadRepositoryTuning()
{
  const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path().parent_path() / "Data" / "Tuning.json";
  std::ifstream file(path, std::ios::binary);
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(file.is_open(), L"cannot open Data/Tuning.json");
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}
} // namespace GameLogicTests
