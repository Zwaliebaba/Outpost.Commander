#include "pch.h"
#include "Json.h"

#include <charconv>

namespace
{
// Deep enough for any data file, shallow enough that a hostile file cannot exhaust the stack.
constexpr int MAX_DEPTH = 64;

class Parser
{
public:
  explicit Parser(std::string_view _text) noexcept
    : m_text(_text)
  {
  }

  Neuron::JsonValue ParseDocument()
  {
    if (m_text.starts_with("\xEF\xBB\xBF"))
      m_position = 3;
    SkipWhitespace();
    Neuron::JsonValue value = ParseValue(0);
    SkipWhitespace();
    if (m_position != m_text.size())
      Fail("unexpected text after the value");
    return value;
  }

private:
  [[noreturn]] void Fail(std::string_view _message) const
  {
    int line = 1;
    int column = 1;
    for (size_t i = 0; i < m_position && i < m_text.size(); ++i)
    {
      if (m_text[i] == '\n')
      {
        ++line;
        column = 1;
      }
      else
        ++column;
    }
    throw Neuron::Exception(std::format("JSON line {}, column {}: {}", line, column, _message));
  }

  [[nodiscard]] bool AtEnd() const noexcept
  {
    return m_position >= m_text.size();
  }

  [[nodiscard]] char Peek() const noexcept
  {
    return AtEnd() ? '\0' : m_text[m_position];
  }

  void SkipWhitespace() noexcept
  {
    while (!AtEnd() && (Peek() == ' ' || Peek() == '\t' || Peek() == '\n' || Peek() == '\r'))
    {
      ++m_position;
    }
  }

  void Expect(char _character)
  {
    if (AtEnd() || Peek() != _character)
      Fail(std::format("expected '{}'", _character));
    ++m_position;
  }

  void ExpectWord(std::string_view _word)
  {
    if (!m_text.substr(m_position).starts_with(_word))
      Fail("expected a value");
    m_position += _word.size();
  }

  Neuron::JsonValue ParseValue(int _depth)
  {
    if (_depth > MAX_DEPTH)
      Fail(std::format("nested deeper than {}", MAX_DEPTH));
    switch (Peek())
    {
    case '{':
      return ParseObject(_depth);
    case '[':
      return ParseArray(_depth);
    case '"':
      return Neuron::JsonValue(ParseString());
    case 't':
      ExpectWord("true");
      return Neuron::JsonValue(true);
    case 'f':
      ExpectWord("false");
      return Neuron::JsonValue(false);
    case 'n':
      ExpectWord("null");
      return {};
    default:
      if (Peek() == '-' || (Peek() >= '0' && Peek() <= '9'))
        return Neuron::JsonValue(ParseNumber());
      Fail(AtEnd() ? "expected a value, found the end of the text" : "expected a value");
    }
  }

  Neuron::JsonValue ParseObject(int _depth)
  {
    Expect('{');
    Neuron::JsonValue::Object members;
    SkipWhitespace();
    if (Peek() == '}')
    {
      ++m_position;
      return Neuron::JsonValue(std::move(members));
    }
    for (;;)
    {
      SkipWhitespace();
      if (Peek() != '"')
        Fail("expected a member name");
      const size_t nameAt = m_position;
      std::string name = ParseString();
      for (const Neuron::JsonMember& member : members)
      {
        if (member.name == name)
        {
          m_position = nameAt;
          Fail(std::format("the member \"{}\" appears twice", name));
        }
      }
      SkipWhitespace();
      Expect(':');
      SkipWhitespace();
      members.push_back({std::move(name), ParseValue(_depth + 1)});
      SkipWhitespace();
      if (Peek() == ',')
      {
        ++m_position;
        continue;
      }
      Expect('}');
      return Neuron::JsonValue(std::move(members));
    }
  }

  Neuron::JsonValue ParseArray(int _depth)
  {
    Expect('[');
    Neuron::JsonValue::Array elements;
    SkipWhitespace();
    if (Peek() == ']')
    {
      ++m_position;
      return Neuron::JsonValue(std::move(elements));
    }
    for (;;)
    {
      SkipWhitespace();
      elements.push_back(ParseValue(_depth + 1));
      SkipWhitespace();
      if (Peek() == ',')
      {
        ++m_position;
        continue;
      }
      Expect(']');
      return Neuron::JsonValue(std::move(elements));
    }
  }

  // RFC 8259 §6: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?. The grammar is checked here, because from_chars accepts
  // forms JSON does not, such as a leading zero, "1." or "inf"; from_chars then does the correctly rounded conversion.
  double ParseNumber()
  {
    const size_t start = m_position;
    auto digits = [this]
    {
      const size_t first = m_position;
      while (!AtEnd() && Peek() >= '0' && Peek() <= '9')
      {
        ++m_position;
      }
      return m_position - first;
    };

    if (Peek() == '-')
      ++m_position;
    if (Peek() == '0')
      ++m_position;
    else if (digits() == 0)
      Fail("expected a digit");
    if (Peek() == '.')
    {
      ++m_position;
      if (digits() == 0)
        Fail("expected a digit after the decimal point");
    }
    if (Peek() == 'e' || Peek() == 'E')
    {
      ++m_position;
      if (Peek() == '+' || Peek() == '-')
        ++m_position;
      if (digits() == 0)
        Fail("expected a digit in the exponent");
    }

    const char* first = m_text.data() + start;
    const char* last = m_text.data() + m_position;
    double value = 0.0;
    const auto [end, error] = std::from_chars(first, last, value);
    if (error == std::errc::result_out_of_range)
    {
      m_position = start;
      Fail("the number is out of range");
    }
    if (error != std::errc() || end != last)
    {
      m_position = start;
      Fail("the number cannot be read");
    }
    return value;
  }

  unsigned ParseHexQuad()
  {
    unsigned value = 0;
    for (int i = 0; i < 4; ++i)
    {
      const char digit = Peek();
      unsigned nibble = 0;
      if (digit >= '0' && digit <= '9')
        nibble = static_cast<unsigned>(digit - '0');
      else if (digit >= 'a' && digit <= 'f')
        nibble = static_cast<unsigned>(digit - 'a' + 10);
      else if (digit >= 'A' && digit <= 'F')
        nibble = static_cast<unsigned>(digit - 'A' + 10);
      else
        Fail("expected four hexadecimal digits after \\u");
      value = (value << 4) | nibble;
      ++m_position;
    }
    return value;
  }

  static void AppendUtf8(std::string& _text, unsigned _codePoint)
  {
    if (_codePoint < 0x80)
      _text += static_cast<char>(_codePoint);
    else if (_codePoint < 0x800)
    {
      _text += static_cast<char>(0xC0 | (_codePoint >> 6));
      _text += static_cast<char>(0x80 | (_codePoint & 0x3F));
    }
    else if (_codePoint < 0x10000)
    {
      _text += static_cast<char>(0xE0 | (_codePoint >> 12));
      _text += static_cast<char>(0x80 | ((_codePoint >> 6) & 0x3F));
      _text += static_cast<char>(0x80 | (_codePoint & 0x3F));
    }
    else
    {
      _text += static_cast<char>(0xF0 | (_codePoint >> 18));
      _text += static_cast<char>(0x80 | ((_codePoint >> 12) & 0x3F));
      _text += static_cast<char>(0x80 | ((_codePoint >> 6) & 0x3F));
      _text += static_cast<char>(0x80 | (_codePoint & 0x3F));
    }
  }

  unsigned ParseEscapedCodePoint()
  {
    const size_t escapeAt = m_position - 2;
    const unsigned high = ParseHexQuad();
    if (high >= 0xDC00 && high <= 0xDFFF)
    {
      m_position = escapeAt;
      Fail("a low surrogate without a high surrogate before it");
    }
    if (high < 0xD800 || high > 0xDBFF)
      return high;
    if (!m_text.substr(m_position).starts_with("\\u"))
    {
      m_position = escapeAt;
      Fail("a high surrogate without a low surrogate after it");
    }
    m_position += 2;
    const unsigned low = ParseHexQuad();
    if (low < 0xDC00 || low > 0xDFFF)
    {
      m_position = escapeAt;
      Fail("a high surrogate without a low surrogate after it");
    }
    return 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00);
  }

  std::string ParseString()
  {
    Expect('"');
    std::string text;
    for (;;)
    {
      if (AtEnd())
        Fail("the string is not closed");
      const char character = m_text[m_position];
      if (character == '"')
      {
        ++m_position;
        return text;
      }
      if (static_cast<unsigned char>(character) < 0x20)
        Fail("a control character in a string must be escaped");
      ++m_position;
      if (character != '\\')
      {
        text += character;
        continue;
      }
      if (AtEnd())
        Fail("the string is not closed");
      const char escape = Peek();
      ++m_position;
      switch (escape)
      {
      case '"':
      case '\\':
      case '/':
        text += escape;
        break;
      case 'b':
        text += '\b';
        break;
      case 'f':
        text += '\f';
        break;
      case 'n':
        text += '\n';
        break;
      case 'r':
        text += '\r';
        break;
      case 't':
        text += '\t';
        break;
      case 'u':
        AppendUtf8(text, ParseEscapedCodePoint());
        break;
      default:
        m_position -= 2;
        Fail("an unknown escape");
      }
    }
  }

  std::string_view m_text;
  size_t m_position = 0;
};

[[noreturn]] void WrongType(std::string_view _expected)
{
  throw Neuron::Exception(std::format("the JSON value is not {}", _expected));
}
} // namespace

Neuron::JsonValue::JsonValue(bool _value) noexcept
  : m_value(_value)
{
}

Neuron::JsonValue::JsonValue(double _value) noexcept
  : m_value(_value)
{
}

Neuron::JsonValue::JsonValue(std::string _value) noexcept
  : m_value(std::move(_value))
{
}

Neuron::JsonValue::JsonValue(Array _value) noexcept
  : m_value(std::move(_value))
{
}

Neuron::JsonValue::JsonValue(Object _value) noexcept
  : m_value(std::move(_value))
{
}

bool Neuron::JsonValue::AsBool() const
{
  if (!IsBool())
    WrongType("true or false");
  return std::get<bool>(m_value);
}

double Neuron::JsonValue::AsNumber() const
{
  if (!IsNumber())
    WrongType("a number");
  return std::get<double>(m_value);
}

const std::string& Neuron::JsonValue::AsString() const
{
  if (!IsString())
    WrongType("a string");
  return std::get<std::string>(m_value);
}

const Neuron::JsonValue::Array& Neuron::JsonValue::AsArray() const
{
  if (!IsArray())
    WrongType("an array");
  return std::get<Array>(m_value);
}

const Neuron::JsonValue::Object& Neuron::JsonValue::AsObject() const
{
  if (!IsObject())
    WrongType("an object");
  return std::get<Object>(m_value);
}

const Neuron::JsonValue* Neuron::JsonValue::Find(std::string_view _name) const
{
  for (const JsonMember& member : AsObject())
  {
    if (member.name == _name)
      return &member.value;
  }
  return nullptr;
}

Neuron::JsonValue Neuron::ParseJson(std::string_view _text)
{
  return Parser(_text).ParseDocument();
}

std::string Neuron::QuoteJson(std::string_view _text)
{
  std::string quoted = "\"";
  for (const char character : _text)
  {
    switch (character)
    {
    case '"':
      quoted += "\\\"";
      break;
    case '\\':
      quoted += "\\\\";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20)
        quoted += std::format("\\u{:04x}", static_cast<unsigned char>(character));
      else
        quoted += character;
      break;
    }
  }
  quoted += '"';
  return quoted;
}
