#include "pch.h"
#include "TemporaryHomeDirectory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
namespace
{
std::string_view AsText(const Neuron::ByteBuffer& _bytes) noexcept
{
  return {reinterpret_cast<const char*>(_bytes.data()), _bytes.size()};
}
} // namespace

// The packaged game reads its data and its assets from the Assets folder beside the executable (ADR-008). Every caller
// takes an empty buffer as a file that is missing or cannot be read, and says so naming the file.
TEST_CLASS(FileSysTests)
{
public:
  TEST_METHOD(TheHomeDirectoryIsTheAssetsFolder)
  {
    const TemporaryHomeDirectory home(L"FileSysTests");
    Assert::AreEqual(home.Root().wstring() + L"\\Assets\\", Neuron::FileSys::GetHomeDirectory());
  }

  // Every byte comes back as it was written, a zero and a CR LF pair among them: nothing is read as text.
  TEST_METHOD(ReadsAWholeFileUnderTheAssetsFolder)
  {
    const TemporaryHomeDirectory home(L"FileSysTests");
    Neuron::ByteBuffer bytes{0x00, 0xFF, 0x0D, 0x0A, 0x1A, 0x7F, 0x80};
    for (int i = 0; i < 70'000; ++i)
      bytes.push_back(static_cast<std::uint8_t>(i * 31));
    home.WriteAsset(L"Data.bin", AsText(bytes));
    Assert::IsTrue(bytes == Neuron::BinaryFile::ReadFile(L"Data.bin"));
  }

  // A model's mesh is named by its folder under Assets.
  TEST_METHOD(ReadsAFileInAFolderUnderTheAssetsFolder)
  {
    const TemporaryHomeDirectory home(L"FileSysTests");
    const Neuron::ByteBuffer bytes{1, 2, 3};
    home.WriteAsset(L"Meshes\\Hull.nmf", AsText(bytes));
    Assert::IsTrue(bytes == Neuron::BinaryFile::ReadFile(L"Meshes\\Hull.nmf"));
  }

  TEST_METHOD(ReadsNothingFromAFileThatIsMissing)
  {
    const TemporaryHomeDirectory home(L"FileSysTests");
    Assert::IsTrue(Neuron::BinaryFile::ReadFile(L"Missing.json").empty());
    Assert::IsTrue(Neuron::BinaryFile::ReadFile(L"NoSuchFolder\\Missing.json").empty());
  }

  // A folder where a file is expected cannot be read as one.
  TEST_METHOD(ReadsNothingFromAFolder)
  {
    const TemporaryHomeDirectory home(L"FileSysTests");
    std::filesystem::create_directories(home.Root() / L"Assets" / L"Meshes");
    Assert::IsTrue(Neuron::BinaryFile::ReadFile(L"Meshes").empty());
  }

  // An empty file reads as nothing too, so a caller refuses it as it refuses a missing one rather than parsing nothing.
  TEST_METHOD(ReadsNothingFromAnEmptyFile)
  {
    const TemporaryHomeDirectory home(L"FileSysTests");
    home.WriteAsset(L"Empty.json", "");
    Assert::IsTrue(Neuron::BinaryFile::ReadFile(L"Empty.json").empty());
  }

  // The file is opened to be shared with other readers, so a file another reader holds open is still read.
  TEST_METHOD(ReadsAFileAnotherReaderHoldsOpen)
  {
    const TemporaryHomeDirectory home(L"FileSysTests");
    const Neuron::ByteBuffer bytes{9, 8, 7};
    home.WriteAsset(L"Shared.json", AsText(bytes));
    std::ifstream other(home.Root() / L"Assets" / L"Shared.json", std::ios::binary);
    Assert::IsTrue(other.is_open());
    Assert::IsTrue(bytes == Neuron::BinaryFile::ReadFile(L"Shared.json"));
  }
};
} // namespace NeuronCoreTests
