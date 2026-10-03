#include "pch.h"
#include "FileSys.h"

Neuron::ByteBuffer Neuron::BinaryFile::ReadFile(const std::wstring& _fileName)
{
  ByteBuffer data;

  std::wstring fullName = GetHomeDirectory() + _fileName;
  ScopedHandle file(SafeHandle(CreateFile2(fullName.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
  if (!file)
    return {};

  // Get the file size.
  FILE_STANDARD_INFO fileInfo;
  if (!GetFileInformationByHandleEx(file.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
    return {};

  // File is too big for 32-bit allocation, so reject read.
  if (fileInfo.EndOfFile.HighPart > 0)
    return {};

  // Create enough space for the file data.
  data.resize(fileInfo.EndOfFile.LowPart);

  // Read the data in.
  DWORD bytesRead = 0;

  if (!::ReadFile(file.get(), data.data(), fileInfo.EndOfFile.LowPart, &bytesRead, nullptr))
    return {};

  if (bytesRead < fileInfo.EndOfFile.LowPart)
    return {};

  return data;
}
