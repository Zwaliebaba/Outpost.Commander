#include "pch.h"

#include "PackageFile.h"

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace Neuron
{

bool ReadPackageFile(std::wstring_view _relativePath, std::vector<std::byte>& _outBytes) noexcept
{
  try
  {
    // A PROPERTY, NOT AN ASYNCHRONOUS OPERATION. See the header for what happens when this is
    // `GetFileAsync(...).get()` instead, and where that was learned.
    const std::filesystem::path installed{std::wstring{winrt::Windows::ApplicationModel::Package::Current().InstalledLocation().Path()}};

    std::ifstream file{installed / std::wstring{_relativePath}, std::ios::binary | std::ios::ate};
    if (!file)
    {
      return false;
    }

    const std::streamoff size = file.tellg();
    if (size <= 0)
    {
      return false;
    }
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
    {
      return false;
    }

    _outBytes = std::move(bytes);
    return true;
  }
  catch (...)
  {
    // EVERY FAILURE PATH ENDS HERE. A client that will not start is a worse outcome than one that
    // draws an arrow where a hull should be, and the caller is what decides that.
    return false;
  }
}

} // namespace Neuron
