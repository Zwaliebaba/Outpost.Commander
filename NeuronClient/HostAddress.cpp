#include "pch.h"

#include "HostAddress.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <string>
#include <string_view>

namespace Neuron
{

namespace
{
[[nodiscard]] constexpr bool IsBlank(char _character) noexcept
{
  return (_character == ' ') || (_character == '\t') || (_character == '\r') || (_character == '\n');
}
} // namespace

std::string HostAddressFromFileContents(std::string_view _contents)
{
  // The first line, however the file was saved. A file written on this machine ends its lines with
  // a carriage return and a newline, and one dropped from anywhere else may not.
  const std::size_t lineEnd = _contents.find_first_of("\r\n");
  std::string_view line = (lineEnd == std::string_view::npos) ? _contents : _contents.substr(0, lineEnd);

  while (!line.empty() && IsBlank(line.front()))
  {
    line.remove_prefix(1);
  }
  while (!line.empty() && IsBlank(line.back()))
  {
    line.remove_suffix(1);
  }

  if (line.empty())
  {
    return std::string{DEFAULT_HOST_ADDRESS};
  }
  return std::string{line};
}

std::string ReadHostAddress() noexcept
{
  // EVERY FAILURE PATH ENDS AT THE DEFAULT, which is why the whole body is inside one catch. There
  // is no first run on which this file exists, so "not there" is the ordinary case rather than the
  // exceptional one, and it must cost nothing but the exception the projection throws for it.
  try
  {
    const winrt::Windows::Storage::StorageFolder folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
    const winrt::Windows::Storage::StorageFile file = folder.GetFileAsync(winrt::hstring{HOST_ADDRESS_FILE_NAME}).get();
    const winrt::hstring contents = winrt::Windows::Storage::FileIO::ReadTextAsync(file).get();

    return HostAddressFromFileContents(winrt::to_string(contents));
  }
  catch (...)
  {
    return std::string{DEFAULT_HOST_ADDRESS};
  }
}

} // namespace Neuron
