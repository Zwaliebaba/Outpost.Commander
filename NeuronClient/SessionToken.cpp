#include "pch.h"

#include "SessionToken.h"
#include "InstanceSlot.h"

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
inline constexpr std::size_t TOKEN_DIGITS = 16;

[[nodiscard]] constexpr bool IsBlank(char _character) noexcept
{
  return (_character == ' ') || (_character == '\t') || (_character == '\r') || (_character == '\n');
}

/// The digit's value, or 16 for anything that is not one. **NOT `std::isxdigit`**, which is
/// locale-dependent and takes an `int` that must not be negative -- a signed `char` above 127
/// passed to it is undefined behavior, and the bytes in this file came off a disk.
[[nodiscard]] constexpr std::uint8_t HexValue(char _character) noexcept
{
  if ((_character >= '0') && (_character <= '9'))
  {
    return static_cast<std::uint8_t>(_character - '0');
  }
  if ((_character >= 'a') && (_character <= 'f'))
  {
    return static_cast<std::uint8_t>((_character - 'a') + 10);
  }
  if ((_character >= 'A') && (_character <= 'F'))
  {
    return static_cast<std::uint8_t>((_character - 'A') + 10);
  }
  return 16;
}
} // namespace

std::uint64_t SessionTokenFromFileContents(std::string_view _contents)
{
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

  // EXACTLY SIXTEEN, so a truncated file is refused by its length before a digit is looked at.
  // Anything shorter would otherwise parse into a small number that names nobody's session, and a
  // client presenting one would be seated as new -- which is the same outcome, reached less
  // clearly.
  if (line.size() != TOKEN_DIGITS)
  {
    return NO_TOKEN;
  }

  std::uint64_t token = 0;
  for (const char digit : line)
  {
    const std::uint8_t value = HexValue(digit);
    if (value > 15)
    {
      return NO_TOKEN;
    }
    token = (token << 4) | value;
  }
  return token;
}

std::string SessionTokenToFileContents(std::uint64_t _token)
{
  // Written by hand rather than by a stream: this has to be the exact shape the reader above
  // demands, and a formatting flag left set somewhere else is not a thing to discover on a device.
  constexpr char DIGITS[] = "0123456789abcdef";
  std::string text(TOKEN_DIGITS, '0');
  for (std::size_t position = 0; position < TOKEN_DIGITS; ++position)
  {
    const std::size_t shift = (TOKEN_DIGITS - 1 - position) * 4;
    text[position] = DIGITS[(_token >> shift) & 0xF];
  }
  return text;
}

std::uint64_t ReadSessionToken(std::uint32_t _instanceSlot) noexcept
{
  // The path is a property and the read is ordinary file I/O -- see `HostAddress.cpp` for why this
  // is not `GetFileAsync(...).get()`, and for what happened the day it was.
  try
  {
    const std::filesystem::path localState{std::wstring{winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()}};
    std::ifstream file{localState / InstanceFileName(SESSION_TOKEN_FILE_NAME, _instanceSlot)};
    if (!file)
    {
      return NO_TOKEN;
    }

    std::string firstLine;
    std::getline(file, firstLine);
    return SessionTokenFromFileContents(firstLine);
  }
  catch (...)
  {
    return NO_TOKEN;
  }
}

bool WriteSessionToken(std::uint64_t _token, std::uint32_t _instanceSlot) noexcept
{
  try
  {
    const std::filesystem::path localState{std::wstring{winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()}};
    const std::filesystem::path path = localState / InstanceFileName(SESSION_TOKEN_FILE_NAME, _instanceSlot);

    if (_token == NO_TOKEN)
    {
      std::error_code ignored;
      static_cast<void>(std::filesystem::remove(path, ignored));
      return true;
    }

    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    if (!file)
    {
      return false;
    }
    file << SessionTokenToFileContents(_token);
    return file.good();
  }
  catch (...)
  {
    return false;
  }
}

} // namespace Neuron
