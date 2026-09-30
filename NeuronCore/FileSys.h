#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Neuron
{
using ByteBuffer = std::vector<std::uint8_t>;

class FileSys
{
public:
  // Set once at startup, before any other thread exists, and only read after that.
  static void SetHomeDirectory(const std::wstring& _path)
  {
    sm_homeDir = _path + L"\\Assets\\";
  }
  [[nodiscard]] static std::wstring GetHomeDirectory()
  {
    return sm_homeDir;
  }

protected:
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
