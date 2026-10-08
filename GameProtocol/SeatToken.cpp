#include "pch.h"
#include "SeatToken.h"

#include <random>

namespace
{
// The value of one hexadecimal digit, or -1 for any other character.
int DigitValue(char _digit) noexcept
{
  if (_digit >= '0' && _digit <= '9')
    return _digit - '0';
  if (_digit >= 'a' && _digit <= 'f')
    return _digit - 'a' + 10;
  if (_digit >= 'A' && _digit <= 'F')
    return _digit - 'A' + 10;
  return -1;
}
} // namespace

Outpost::SeatToken Outpost::NewSeatToken()
{
  // MSVC's random_device draws from the operating system's cryptographic source, as Linux's does.
  std::random_device source;
  SeatToken token{};
  for (std::size_t i = 0; i < token.size(); i += sizeof(std::uint32_t))
  {
    const std::uint32_t bits = source();
    for (std::size_t j = 0; j < sizeof(std::uint32_t); ++j)
      token[i + j] = static_cast<std::uint8_t>(bits >> (8 * j));
  }
  return token;
}

std::string Outpost::ToHex(std::span<const std::uint8_t> _bytes)
{
  constexpr std::string_view DIGITS = "0123456789abcdef";
  std::string text;
  text.reserve(2 * _bytes.size());
  for (const std::uint8_t byte : _bytes)
  {
    text += DIGITS[byte >> 4];
    text += DIGITS[byte & 0x0F];
  }
  return text;
}

bool Outpost::FromHex(std::string_view _text, std::span<std::uint8_t> _bytes) noexcept
{
  if (_text.size() != 2 * _bytes.size())
    return false;
  for (std::size_t i = 0; i < _bytes.size(); ++i)
  {
    const int high = DigitValue(_text[2 * i]);
    const int low = DigitValue(_text[(2 * i) + 1]);
    if (high < 0 || low < 0)
      return false;
    _bytes[i] = static_cast<std::uint8_t>((high << 4) | low);
  }
  return true;
}
