#include "pch.h"
#include "AiMatchesOptions.h"

#include <algorithm>
#include <charconv>
#include <limits>

namespace
{
// The log when the options name none: in the temporary folder, replaced by each run (ADR-038).
constexpr auto DEFAULT_LOG = L"OutpostCommander-ai-matches.log";
// The switch's options (ADR-063). Each takes a value.
constexpr std::wstring_view FIRST_SEED_OPTION = L"--first-seed";
constexpr std::wstring_view MATCHES_OPTION = L"--matches";
constexpr std::wstring_view LIMIT_MINUTES_OPTION = L"--limit-minutes";
constexpr std::wstring_view AI1_OPTION = L"--ai1";
constexpr std::wstring_view AI2_OPTION = L"--ai2";
constexpr std::wstring_view LOG_OPTION = L"--log";
constexpr std::array<std::wstring_view, 6> OPTIONS{FIRST_SEED_OPTION, MATCHES_OPTION, LIMIT_MINUTES_OPTION,
                                                   AI1_OPTION,        AI2_OPTION,     LOG_OPTION};

// A whole number at least _minimum and at most _maximum, written in decimal digits and nothing else.
std::uint64_t ReadNumber(std::wstring_view _option, std::wstring_view _value, std::uint64_t _minimum, std::uint64_t _maximum)
{
  // Every digit is ASCII, so narrowing them one by one is exact; anything else is refused below.
  std::string digits;
  for (const wchar_t character : _value)
    digits.push_back(character >= L'0' && character <= L'9' ? static_cast<char>(character) : '?');
  std::uint64_t number = 0;
  const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
  if (digits.empty() || error != std::errc{} || end != digits.data() + digits.size() || number < _minimum || number > _maximum)
  {
    throw Neuron::Exception(std::format("--ai-matches: {} takes a whole number from {} to {}, not '{}'.", winrt::to_string(_option),
                                        _minimum, _maximum, winrt::to_string(_value)));
  }
  return number;
}
} // namespace

Outpost::AiMatchesOptions Outpost::ReadAiMatchesOptions(std::span<const std::wstring> _arguments)
{
  AiMatchesOptions options;
  std::vector<std::wstring_view> given;
  for (size_t i = 0; i < _arguments.size(); i += 2)
  {
    const std::wstring_view option = _arguments[i];
    if (std::ranges::find(OPTIONS, option) == OPTIONS.end())
      throw Neuron::Exception(std::format("--ai-matches: {} is not an option it takes.", winrt::to_string(option)));
    if (std::ranges::find(given, option) != given.end())
      throw Neuron::Exception(std::format("--ai-matches: {} is given twice.", winrt::to_string(option)));
    given.push_back(option);
    // The value follows its option, and is neither empty nor another option.
    if (i + 1 == _arguments.size() || _arguments[i + 1].empty() || _arguments[i + 1].starts_with(L"--"))
      throw Neuron::Exception(std::format("--ai-matches: {} needs a value.", winrt::to_string(option)));
    const std::wstring_view value = _arguments[i + 1];
    if (option == FIRST_SEED_OPTION)
      options.desc.firstSeed = ReadNumber(option, value, 0, std::numeric_limits<std::uint64_t>::max());
    else if (option == MATCHES_OPTION)
      options.desc.matches = static_cast<std::uint32_t>(ReadNumber(option, value, 1, std::numeric_limits<std::uint32_t>::max()));
    else if (option == LIMIT_MINUTES_OPTION)
      options.desc.limitMinutes = static_cast<std::uint32_t>(ReadNumber(option, value, 1, std::numeric_limits<std::uint32_t>::max()));
    else if (option == AI1_OPTION)
      options.settings[0] = value;
    else if (option == AI2_OPTION)
      options.settings[1] = value;
    else if (option == LOG_OPTION)
      options.log = value;
  }
  if (options.log.empty())
    options.log = std::filesystem::temp_directory_path() / DEFAULT_LOG;
  return options;
}
