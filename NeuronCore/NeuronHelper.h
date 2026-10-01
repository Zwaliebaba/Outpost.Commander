#pragma once

#include <type_traits>

namespace Neuron
{
template <typename T> constexpr bool IsValidEnum(T _value) noexcept
{
  return (_value >= begin(_value) && _value < end(_value));
}

template <typename T> constexpr size_t I(T _value) noexcept
{
  return static_cast<size_t>(_value);
}

class Exception : public std::exception
{
public:
  Exception(std::string _message) noexcept
    : m_message(std::move(_message)) {}

  ~Exception() noexcept override = default;

  [[nodiscard]] const char* what() const noexcept override
  {
    return m_message.c_str();
  }

protected:
  std::string m_message;
};

struct NonCopyable
{
  NonCopyable() = default;
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};

struct HandleCloser
{
  void operator()(HANDLE _handle) const noexcept
  {
    if (_handle)
      CloseHandle(_handle);
  }
};

using ScopedHandle = std::unique_ptr<void, HandleCloser>;

inline HANDLE SafeHandle(HANDLE _handle) noexcept
{
  return (_handle == INVALID_HANDLE_VALUE) ? nullptr : _handle;
}
} // namespace Neuron