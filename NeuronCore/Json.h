#pragma once

namespace Neuron
{
struct JsonMember;

// One value of a JSON document (RFC 8259). A number is held as a double, as JSON defines it, so a reader that needs an
// integer checks that the value is one. Object members keep the order of the text, and a name appears at most once.
class JsonValue
{
public:
  using Array = std::vector<JsonValue>;
  using Object = std::vector<JsonMember>;

  JsonValue() noexcept = default;
  // A string literal would otherwise convert to bool.
  explicit JsonValue(const char* _value) = delete;
  // Defined in Json.cpp, where JsonMember is complete.
  explicit JsonValue(bool _value) noexcept;
  explicit JsonValue(double _value) noexcept;
  explicit JsonValue(std::string _value) noexcept;
  explicit JsonValue(Array _value) noexcept;
  explicit JsonValue(Object _value) noexcept;

  [[nodiscard]] bool IsNull() const noexcept
  {
    return std::holds_alternative<std::monostate>(m_value);
  }

  [[nodiscard]] bool IsBool() const noexcept
  {
    return std::holds_alternative<bool>(m_value);
  }

  [[nodiscard]] bool IsNumber() const noexcept
  {
    return std::holds_alternative<double>(m_value);
  }

  [[nodiscard]] bool IsString() const noexcept
  {
    return std::holds_alternative<std::string>(m_value);
  }

  [[nodiscard]] bool IsArray() const noexcept
  {
    return std::holds_alternative<Array>(m_value);
  }

  [[nodiscard]] bool IsObject() const noexcept
  {
    return std::holds_alternative<Object>(m_value);
  }

  // Each throws Neuron::Exception when the value is of another type. A reader that can say where the value came from
  // checks the type first and throws its own, better message.
  [[nodiscard]] bool AsBool() const;
  [[nodiscard]] double AsNumber() const;
  [[nodiscard]] const std::string& AsString() const;
  [[nodiscard]] const Array& AsArray() const;
  [[nodiscard]] const Object& AsObject() const;

  // The member with this name, or nullptr when there is none. Throws when the value is not an object.
  [[nodiscard]] const JsonValue* Find(std::string_view _name) const;

private:
  std::variant<std::monostate, bool, double, std::string, Array, Object> m_value;
};

struct JsonMember
{
  std::string name;
  JsonValue value;
};

// Parses a whole document. Throws Neuron::Exception naming the line and column of the first error. Strict RFC 8259: no
// comments, no trailing commas, no NaN or infinity, and nothing after the value but whitespace. A leading UTF-8 byte
// order mark is skipped. The bytes of a string are kept as they are, apart from escapes, which become UTF-8.
[[nodiscard]] JsonValue ParseJson(std::string_view _text);

// _text as a JSON string, quoted, with its quotes, backslashes and control characters escaped, which ParseJson reads back
// as _text. Its other bytes are written as they are, so UTF-8 stays UTF-8.
[[nodiscard]] std::string QuoteJson(std::string_view _text);
} // namespace Neuron