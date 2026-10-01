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
  Neuron::ByteBuffer bytes(static_cast<size_t>(std::filesystem::file_size(path)));
  file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return bytes;
}

inline std::string ReadRepositoryAssetText(std::string_view _fileName)
{
  const Neuron::ByteBuffer bytes = ReadRepositoryAsset(_fileName);
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

// A model's converted mesh, as the game builds it from the repository's copy.
inline Neuron::MeshData ReadRepositoryModel(const Outpost::ModelSet& _set, const Outpost::ModelEntry& _model)
{
  const std::string fileName = std::format("Models\\{}\\{}.nmf", _set.name, _model.name);
  return Outpost::BuildModelMesh(ReadRepositoryAsset(fileName), _model, fileName);
}
} // namespace GameAppTests
