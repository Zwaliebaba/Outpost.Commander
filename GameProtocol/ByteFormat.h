#pragma once

// How the game writes its values as bytes: the wire format's messages (ADR-060) and a world's saves (ADR-077) alike.
// Included where it is needed, not by GameProtocol.h. A record is a type whose fields a Fields function lists, found by
// argument-dependent lookup: WireFields.h lists the protocol's, and the server lists those of its state.

#include <bit>
#include <concepts>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace Outpost
{
// A type whose fields a Fields function lists.
template <typename T>
concept Record = requires(T& _value) { Fields(_value); };

// Writes values as bytes: integers little-endian in their own width, floats as their bits, a count before the elements of
// a string or a vector, a flag before an optional value, the alternative's index before a variant's, and a record's fields
// in the order its Fields lists them (ADR-060 decision 4).
class ByteWriter
{
public:
  [[nodiscard]] std::vector<std::byte> Take() noexcept
  {
    return std::move(m_bytes);
  }

  [[nodiscard]] std::size_t Size() const noexcept
  {
    return m_bytes.size();
  }

  void Put(bool _value)
  {
    PutBits(static_cast<std::uint8_t>(_value ? 1 : 0));
  }

  template <std::integral T> void Put(T _value)
  {
    PutBits(static_cast<std::make_unsigned_t<T>>(_value));
  }

  void Put(float _value)
  {
    PutBits(std::bit_cast<std::uint32_t>(_value));
  }

  void Put(double _value)
  {
    PutBits(std::bit_cast<std::uint64_t>(_value));
  }

  template <typename E>
    requires std::is_enum_v<E>
  void Put(E _value)
  {
    Put(std::to_underlying(_value));
  }

  template <typename Tag> void Put(Id<Tag> _value)
  {
    Put(_value.value);
  }

  void Put(const std::string& _value)
  {
    PutCount(_value.size());
    for (const char character : _value)
      PutBits(static_cast<std::uint8_t>(character));
  }

  template <typename T> void Put(const std::vector<T>& _values)
  {
    PutCount(_values.size());
    for (const T& value : _values)
      Put(value);
  }

  template <typename T, std::size_t N> void Put(const std::array<T, N>& _values)
  {
    for (const T& value : _values)
      Put(value);
  }

  template <typename A, typename B> void Put(const std::pair<A, B>& _value)
  {
    Put(_value.first);
    Put(_value.second);
  }

  template <typename T> void Put(const std::optional<T>& _value)
  {
    Put(_value.has_value());
    if (_value)
      Put(*_value);
  }

  template <typename... Ts> void Put(const std::variant<Ts...>& _value)
  {
    Put(static_cast<std::uint8_t>(_value.index()));
    std::visit([&writer = *this](const auto& _alternative) { writer.Put(_alternative); }, _value);
  }

  template <Record T> void Put(const T& _value)
  {
    std::apply([&writer = *this](const auto&... _fields) { (writer.Put(_fields), ...); }, Fields(_value));
  }

private:
  template <std::unsigned_integral T> void PutBits(T _bits)
  {
    for (std::size_t i = 0; i < sizeof(T); ++i)
      m_bytes.push_back(static_cast<std::byte>(_bits >> (8 * i)));
  }

  void PutCount(std::size_t _count)
  {
    if (_count > std::numeric_limits<std::uint32_t>::max())
      throw Neuron::Exception(std::format("One field cannot hold {} elements.", _count));
    PutBits(static_cast<std::uint32_t>(_count));
  }

  std::vector<std::byte> m_bytes;
};

// Reads what ByteWriter wrote, and refuses anything it could not have written: bytes that end early, a flag that is
// neither 0 nor 1, an enumerator past its enumeration's last (LastOf), a variant's index past its last alternative, and a
// count larger than the bytes left.
class ByteReader
{
public:
  // _source names what the bytes are in a refusal, such as "A message from the network".
  ByteReader(std::span<const std::byte> _bytes, std::string_view _source) noexcept
    : m_bytes(_bytes),
      m_source(_source)
  {
  }

  void Get(bool& _value)
  {
    std::uint8_t bits = 0;
    GetBits(bits);
    if (bits > 1)
      Malformed("a flag is neither 0 nor 1");
    _value = bits == 1;
  }

  template <std::integral T> void Get(T& _value)
  {
    std::make_unsigned_t<T> bits = 0;
    GetBits(bits);
    _value = static_cast<T>(bits);
  }

  void Get(float& _value)
  {
    std::uint32_t bits = 0;
    GetBits(bits);
    _value = std::bit_cast<float>(bits);
  }

  void Get(double& _value)
  {
    std::uint64_t bits = 0;
    GetBits(bits);
    _value = std::bit_cast<double>(bits);
  }

  template <typename E>
    requires std::is_enum_v<E>
  void Get(E& _value)
  {
    std::underlying_type_t<E> raw{};
    Get(raw);
    if (raw > std::to_underlying(LastOf(E{})))
      Malformed(std::format("{} is not a value of its enumeration", raw));
    _value = static_cast<E>(raw);
  }

  template <typename Tag> void Get(Id<Tag>& _value)
  {
    Get(_value.value);
  }

  void Get(std::string& _value)
  {
    const std::span<const std::byte> bytes = Take(GetCount());
    _value.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  }

  // The elements are added as they are read, so a count the bytes cannot back runs out of bytes before it runs out of
  // memory.
  template <typename T> void Get(std::vector<T>& _values)
  {
    const std::size_t count = GetCount();
    _values.clear();
    for (std::size_t i = 0; i < count; ++i)
      Get(_values.emplace_back());
  }

  template <typename T, std::size_t N> void Get(std::array<T, N>& _values)
  {
    for (T& value : _values)
      Get(value);
  }

  template <typename A, typename B> void Get(std::pair<A, B>& _value)
  {
    Get(_value.first);
    Get(_value.second);
  }

  template <typename T> void Get(std::optional<T>& _value)
  {
    bool present = false;
    Get(present);
    if (present)
      Get(_value.emplace());
    else
      _value.reset();
  }

  template <typename... Ts> void Get(std::variant<Ts...>& _value)
  {
    std::uint8_t index = 0;
    Get(index);
    if (index >= sizeof...(Ts))
      Malformed(std::format("{} is not an alternative of its variant", index));
    GetAlternative(_value, index, std::index_sequence_for<Ts...>{});
  }

  template <Record T> void Get(T& _value)
  {
    std::apply([&reader = *this](auto&... _fields) { (reader.Get(_fields), ...); }, Fields(_value));
  }

  // The bytes not read yet.
  [[nodiscard]] std::size_t Left() const noexcept
  {
    return m_bytes.size();
  }

  // Refuses bytes that run on after what was read.
  void Finish() const
  {
    if (!m_bytes.empty())
      Malformed(std::format("{} bytes run on after it", m_bytes.size()));
  }

  [[noreturn]] void Malformed(std::string_view _what) const
  {
    throw Neuron::Exception(std::format("{} is malformed: {}.", m_source, _what));
  }

private:
  template <typename... Ts, std::size_t... Indices>
  void GetAlternative(std::variant<Ts...>& _value, std::size_t _index, std::index_sequence<Indices...>)
  {
    (void)((Indices == _index && (Get(_value.template emplace<Indices>()), true)) || ...);
  }

  template <std::unsigned_integral T> void GetBits(T& _bits)
  {
    const std::span<const std::byte> bytes = Take(sizeof(T));
    _bits = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i)
      _bits = static_cast<T>(_bits | (static_cast<T>(bytes[i]) << (8 * i)));
  }

  // A count of elements, each of which takes a byte at least, so it is never more than the bytes left.
  [[nodiscard]] std::size_t GetCount()
  {
    std::uint32_t count = 0;
    GetBits(count);
    if (count > m_bytes.size())
      Malformed(std::format("it counts {} elements with {} bytes left", count, m_bytes.size()));
    return count;
  }

  [[nodiscard]] std::span<const std::byte> Take(std::size_t _count)
  {
    if (_count > m_bytes.size())
      Malformed("it ends early");
    const std::span<const std::byte> taken = m_bytes.first(_count);
    m_bytes = m_bytes.subspan(_count);
    return taken;
  }

  std::span<const std::byte> m_bytes;
  std::string_view m_source;
};

// Describes how ByteWriter lays a type out, as text: what each value is and in what order, but not the values. Two types
// laid out alike describe alike, and a field added, taken away, reordered or given another type changes the description.
// A world's save pins its layout's hash to its state version with it (ADR-077, AGENTS.md R18).
class ByteLayout
{
public:
  template <typename T> static void Describe(std::string& _out)
  {
    Of(std::type_identity<std::remove_cvref_t<T>>{}, _out);
  }

private:
  static void Of(std::type_identity<bool>, std::string& _out)
  {
    _out += 'b';
  }

  template <std::integral T> static void Of(std::type_identity<T>, std::string& _out)
  {
    _out += std::format("{}{}", std::is_signed_v<T> ? 'i' : 'u', 8 * sizeof(T));
  }

  static void Of(std::type_identity<float>, std::string& _out)
  {
    _out += "f32";
  }

  static void Of(std::type_identity<double>, std::string& _out)
  {
    _out += "f64";
  }

  template <typename E>
    requires std::is_enum_v<E>
  static void Of(std::type_identity<E>, std::string& _out)
  {
    _out += std::format("e{}", 8 * sizeof(E));
  }

  template <typename Tag> static void Of(std::type_identity<Id<Tag>>, std::string& _out)
  {
    _out += "id";
  }

  static void Of(std::type_identity<std::string>, std::string& _out)
  {
    _out += 's';
  }

  template <typename T> static void Of(std::type_identity<std::vector<T>>, std::string& _out)
  {
    _out += "v(";
    Describe<T>(_out);
    _out += ')';
  }

  template <typename T, std::size_t N> static void Of(std::type_identity<std::array<T, N>>, std::string& _out)
  {
    _out += std::format("[{}](", N);
    Describe<T>(_out);
    _out += ')';
  }

  template <typename A, typename B> static void Of(std::type_identity<std::pair<A, B>>, std::string& _out)
  {
    _out += "p(";
    Describe<A>(_out);
    _out += ',';
    Describe<B>(_out);
    _out += ')';
  }

  template <typename T> static void Of(std::type_identity<std::optional<T>>, std::string& _out)
  {
    _out += "o(";
    Describe<T>(_out);
    _out += ')';
  }

  template <typename... Ts> static void Of(std::type_identity<std::variant<Ts...>>, std::string& _out)
  {
    _out += "a(";
    ((Describe<Ts>(_out), _out += ','), ...);
    _out += ')';
  }

  template <Record T> static void Of(std::type_identity<T>, std::string& _out)
  {
    using Tied = decltype(Fields(std::declval<T&>()));
    _out += std::format("r{}{{", std::tuple_size_v<Tied>);
    [&_out]<std::size_t... Indices>(std::index_sequence<Indices...>)
    { ((Describe<std::tuple_element_t<Indices, Tied>>(_out), _out += ','), ...); }(std::make_index_sequence<std::tuple_size_v<Tied>>{});
    _out += '}';
  }
};
} // namespace Outpost
