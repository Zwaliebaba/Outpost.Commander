#pragma once

namespace NeuronClientTests
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
} // namespace NeuronClientTests
