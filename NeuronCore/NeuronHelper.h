#pragma once

#include <type_traits>

namespace Neuron
{
#define ENUM_HELPER(T, S, E)                                                                                                                         \
   T inline operator ++(T& _value) noexcept { return _value = static_cast<T>(static_cast<std::underlying_type_t<T>>(_value) + 1); }                  \
   T inline operator ++(T& _value, int) noexcept { T old = _value; _value = static_cast<T>(static_cast<std::underlying_type_t<T>>(_value) + 1); return old; }             \
   T inline operator --(T& _value) noexcept { return _value = static_cast<T>(static_cast<std::underlying_type_t<T>>(_value) - 1); }                  \
   T inline operator --(T& _value, int) noexcept { T old = _value; _value = static_cast<T>(static_cast<std::underlying_type_t<T>>(_value) - 1); return old; }             \
   inline T operator*(T _type) noexcept { return _type; }                                                                                            \
   constexpr size_t SizeOf##T() noexcept { return static_cast<size_t>(T::E) + 1; }                                                                   \
   class It##T                                                                                                                                 \
   {                                                                                                                                                 \
    int m_value;                                                                                                                                       \
    public:                                                                                                                                          \
      explicit It##T(int _value) : m_value(_value) {}                                                                                                      \
      ##T operator*() const { return static_cast<##T>(m_value); }                                                                                      \
      bool operator!=(const It##T& _other) const { return m_value != _other.m_value; }                                                               \
      It##T& operator++() { ++m_value; return *this; }                                                                                           \
   };                                                                                                                                                \
   class Range##T {                                                                                                                                 \
     public:                                                                                                                                         \
      It##T begin() const { return It##T(static_cast<std::underlying_type_t<T>>(T::S)); }                                                 \
      It##T end() const { return It##T(static_cast<std::underlying_type_t<T>>(T::E) + 1); }                                                \
   };                                                                                                                                                \
   constexpr T operator | (T _a, T _b) noexcept { return T(((_ENUM_FLAG_SIZED_INTEGER<T>::type)_a) | ((_ENUM_FLAG_SIZED_INTEGER<T>::type)_b)); }         \
   inline T &operator |= (T &_a, T _b) noexcept { return (T &)(((_ENUM_FLAG_SIZED_INTEGER<T>::type &)_a) |= ((_ENUM_FLAG_SIZED_INTEGER<T>::type)_b)); }  \
   constexpr T operator & (T _a, T _b) noexcept { return T(((_ENUM_FLAG_SIZED_INTEGER<T>::type)_a) & ((_ENUM_FLAG_SIZED_INTEGER<T>::type)_b)); }         \
   inline T &operator &= (T &_a, T _b) noexcept { return (T &)(((_ENUM_FLAG_SIZED_INTEGER<T>::type &)_a) &= ((_ENUM_FLAG_SIZED_INTEGER<T>::type)_b)); }  \
   constexpr T operator ~ (T _a) noexcept { return T(~((_ENUM_FLAG_SIZED_INTEGER<T>::type)_a)); }                                                      \
   constexpr T operator ^ (T _a, T _b) noexcept { return T(((_ENUM_FLAG_SIZED_INTEGER<T>::type)_a) ^ ((_ENUM_FLAG_SIZED_INTEGER<T>::type)_b)); }         \
   inline T &operator ^= (T &_a, T _b) noexcept { return (T &)(((_ENUM_FLAG_SIZED_INTEGER<T>::type &)_a) ^= ((_ENUM_FLAG_SIZED_INTEGER<T>::type)_b)); }  \
   constexpr bool operator ! (T _a) noexcept { return !((_ENUM_FLAG_SIZED_INTEGER<T>::type)_a); }
  //template <> struct std::formatter<T> : std::formatter<int> {                                                                                     \
  //  auto format(const T& id, std::format_context& ctx) const {return std::formatter<int>::format(static_cast<int>(id), ctx); }                     \
  //}

  template <typename T>
  constexpr bool IsValidEnum(T _value) noexcept { return (_value >= begin(_value) && _value < end(_value)); }

  template <typename T>
  constexpr size_t I(T _value) noexcept { return static_cast<size_t>(_value); }

  class Exception : public std::exception
  {
  public:
    Exception(std::string _message) noexcept : m_message(std::move(_message)) {}
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
  inline HANDLE SafeHandle(HANDLE _handle) noexcept { return (_handle == INVALID_HANDLE_VALUE) ? nullptr : _handle; }
}
