#pragma once

namespace GameAppTests
{
// The bytes of a file the game ships, from OutpostCommander/Assets in the repository, such as "Models\\Human\\Small.nmf".
// UseFullPaths makes __FILE__ absolute, so the repository is two levels up from this header.
inline Neuron::ByteBuffer ReadRepositoryAsset(std::string_view _fileName)
{
  const std::filesystem::path path =
    std::filesystem::path(__FILE__).parent_path().parent_path() / "OutpostCommander" / "Assets" / _fileName;
  std::ifstream file(path, std::ios::binary);
  Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue(file.is_open(), path.wstring().c_str());
  Neuron::ByteBuffer bytes((std::filesystem::file_size(path)));
  file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return bytes;
}

inline std::string ReadRepositoryAssetText(std::string_view _fileName)
{
  const Neuron::ByteBuffer bytes = ReadRepositoryAsset(_fileName);
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

// A model's converted mesh, as the game builds it from the repository's copy: its first level, or another level of a
// model that grows, fitted within _firstLevelWidestMeters (ADR-064).
inline Neuron::MeshData ReadRepositoryModel(const Outpost::ModelSet& _set, const Outpost::ModelEntry& _model,
                                            int _level = Outpost::FIRST_MODEL_LEVEL,
                                            std::optional<float> _firstLevelWidestMeters = std::nullopt)
{
  // ModelFileName's names are ASCII, so narrowing them character by character is exact.
  const std::wstring wideName = Outpost::ModelFileName(_set, _model, _level);
  std::string fileName;
  for (const wchar_t c : wideName)
    fileName.push_back(static_cast<char>(c));
  return Outpost::BuildModelMesh(ReadRepositoryAsset(fileName), _model, fileName, _firstLevelWidestMeters);
}
} // namespace GameAppTests