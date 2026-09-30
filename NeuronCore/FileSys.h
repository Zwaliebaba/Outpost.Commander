#pragma once

namespace Neuron
{
using ByteBuffer = std::vector<uint8_t>;

class FileSys
{
public:
  static void SetHomeDirectory(const std::wstring& _path)
  {
    sm_homeDir = _path + L"\\Assets\\";
  }
  [[nodiscard]] static std::wstring GetHomeDirectory()
  {
    return sm_homeDir;
  }

protected:
  // Set once by WinMain before any other thread starts, then only read. Not synchronized.
  inline static std::wstring sm_homeDir;
};

class BinaryFile : public FileSys
{
public:
  [[nodiscard]] static ByteBuffer ReadFile(const std::wstring& _fileName);
};

class TextFile : public FileSys
{
public:
  [[nodiscard]] static std::wstring ReadFile(const std::wstring& _fileName);
};
} // namespace Neuron
