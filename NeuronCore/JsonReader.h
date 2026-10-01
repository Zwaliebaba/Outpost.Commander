#pragma once

namespace Neuron
{
// Reading a parsed JSON document into typed data, where every error names the place it is about, such as
// "hulls[1].armor: expected a whole number, found 2.5". Each failure throws Neuron::Exception; a loader that wants its
// file named in the message catches and prefixes it.

enum class JsonBound : std::uint8_t
{
  Any,
  NotNegative,
  Positive
};

[[noreturn]] void JsonFail(std::string_view _path, std::string_view _message);

// "_path[_index]".
[[nodiscard]] std::string JsonElementPath(std::string_view _path, size_t _index);

// A number that must be whole and at least _minimum.
[[nodiscard]] std::int32_t ReadJsonInteger(const JsonValue& _value, std::string_view _path, std::int32_t _minimum);
[[nodiscard]] double ReadJsonNumber(const JsonValue& _value, std::string_view _path, JsonBound _bound);
[[nodiscard]] const JsonValue::Array& ReadJsonArray(const JsonValue& _value, std::string_view _path);

// The members of one object, read by name. Finish() rejects any member nothing asked for, so that a misspelled optional
// member is an error rather than silently ignored.
class JsonObjectReader
{
public:
  // An empty _path is the document itself.
  JsonObjectReader(const JsonValue& _value, std::string _path);

  [[nodiscard]] const std::string& Path() const noexcept
  {
    return m_path;
  }

  // The path, or "the file" for the document itself.
  [[nodiscard]] std::string Where() const;
  [[nodiscard]] std::string PathOf(std::string_view _name) const;

  [[nodiscard]] const JsonValue* Optional(std::string_view _name);
  [[nodiscard]] const JsonValue& Required(std::string_view _name);
  [[nodiscard]] std::int32_t Integer(std::string_view _name, std::int32_t _minimum);
  [[nodiscard]] double Number(std::string_view _name, JsonBound _bound);
  [[nodiscard]] std::string String(std::string_view _name);

  // An identifier: a whole number of 1 or more, as the type T built from it.
  template <typename T> [[nodiscard]] T Identifier(std::string_view _name)
  {
    return T{static_cast<std::uint32_t>(Integer(_name, 1))};
  }

  void Finish() const;

private:
  const JsonValue* m_value = nullptr;
  std::string m_path;
  std::vector<std::string_view> m_read;
};

// Reads each element of _parent's array _name with _readElement, which is handed a JsonObjectReader for the element and
// returns an Element. Each element is finished after it is read.
template <typename Element, typename Fn>
[[nodiscard]] std::vector<Element> ReadJsonList(JsonObjectReader& _parent, std::string_view _name, Fn _readElement)
{
  const std::string path = _parent.PathOf(_name);
  const JsonValue::Array& elements = ReadJsonArray(_parent.Required(_name), path);
  std::vector<Element> list;
  list.reserve(elements.size());
  for (size_t i = 0; i < elements.size(); ++i)
  {
    JsonObjectReader reader(elements[i], JsonElementPath(path, i));
    list.push_back(_readElement(reader));
    reader.Finish();
  }
  return list;
}
} // namespace Neuron