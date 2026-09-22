#include "pch.h"

#include "HostAddress.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <filesystem>
#include <fstream>
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
  // === IT ASKS THE PROJECTION FOR THE PATH AND THEN READS THE FILE WITH AN ifstream. ===
  //
  // THIS IS NOT A STYLE PREFERENCE AND THE OBVIOUS ALTERNATIVE CRASHES THE CLIENT. The tidy-looking
  // version is `LocalFolder().GetFileAsync(...).get()` and `FileIO::ReadTextAsync(file).get()`, and
  // it was written that way first. **A packaged application's view runs on a single-threaded
  // apartment, and blocking it on an asynchronous operation is not allowed**: C++/WinRT's `get()`
  // asserts on the UI thread rather than returning, so the client died between opening its log and
  // writing the first line into it -- a zero-byte log file and no window, with nothing in it to say
  // why.
  //
  // It was found by deploying to the device, which is the only place it can be found: no suite in
  // this tree can construct a `CoreWindow`, so no test can put this call on the thread that breaks
  // it. `App.cpp` had it right before M0.22 moved the body down here, and moving it did not make
  // the mechanism wrong -- it made the SPLIT right and then changed the mechanism as well, which is
  // one change too many for one commit.
  //
  // `ApplicationData::Current().LocalFolder().Path()` is a property rather than an async operation,
  // so it is safe anywhere; the read after it is ordinary file I/O on a path the process owns.
  //
  // EVERY FAILURE PATH ENDS AT THE DEFAULT. There is no first run on which this file exists, so
  // "not there" is the ordinary case rather than the exceptional one.
  try
  {
    const std::filesystem::path localState{std::wstring{winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()}};
    std::ifstream file{localState / HOST_ADDRESS_FILE_NAME};
    if (!file)
    {
      return std::string{DEFAULT_HOST_ADDRESS};
    }

    std::string firstLine;
    std::getline(file, firstLine);
    return HostAddressFromFileContents(firstLine);
  }
  catch (...)
  {
    return std::string{DEFAULT_HOST_ADDRESS};
  }
}

} // namespace Neuron
