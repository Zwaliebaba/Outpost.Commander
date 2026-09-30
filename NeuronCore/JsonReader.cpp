#include "pch.h"
#include "JsonReader.h"

#include <algorithm>
#include <cmath>
#include <limits>

void Neuron::JsonFail(std::string_view _path, std::string_view _message)
{
  throw Exception(std::format("{}: {}", _path, _message));
}

std::string Neuron::JsonElementPath(std::string_view _path, size_t _index)
{
  return std::format("{}[{}]", _path, _index);
}

std::int32_t Neuron::ReadJsonInteger(const JsonValue& _value, std::string_view _path, std::int32_t _minimum)
{
  if (!_value.IsNumber())
    JsonFail(_path, "expected a number");
  const double number = _value.AsNumber();
  if (number != std::trunc(number))
    JsonFail(_path, std::format("expected a whole number, found {}", number));
  if (number < _minimum)
    JsonFail(_path, std::format("expected at least {}, found {}", _minimum, number));
  if (number > std::numeric_limits<std::int32_t>::max())
    JsonFail(_path, std::format("{} is too large", number));
  return static_cast<std::int32_t>(number);
}

double Neuron::ReadJsonNumber(const JsonValue& _value, std::string_view _path, JsonBound _bound)
{
  if (!_value.IsNumber())
    JsonFail(_path, "expected a number");
  const double number = _value.AsNumber();
  if (_bound == JsonBound::Positive && number <= 0.0)
    JsonFail(_path, std::format("expected more than 0, found {}", number));
  if (_bound == JsonBound::NotNegative && number < 0.0)
    JsonFail(_path, std::format("expected at least 0, found {}", number));
  return number;
}

const Neuron::JsonValue::Array& Neuron::ReadJsonArray(const JsonValue& _value, std::string_view _path)
{
  if (!_value.IsArray())
    JsonFail(_path, "expected an array");
  return _value.AsArray();
}

Neuron::JsonObjectReader::JsonObjectReader(const JsonValue& _value, std::string _path)
  : m_path(std::move(_path))
{
  if (!_value.IsObject())
    JsonFail(Where(), "expected an object");
  m_value = &_value;
}

std::string Neuron::JsonObjectReader::Where() const
{
  return m_path.empty() ? std::string("the file") : m_path;
}

std::string Neuron::JsonObjectReader::PathOf(std::string_view _name) const
{
  return m_path.empty() ? std::string(_name) : std::format("{}.{}", m_path, _name);
}

const Neuron::JsonValue* Neuron::JsonObjectReader::Optional(std::string_view _name)
{
  m_read.push_back(_name);
  return m_value->Find(_name);
}

const Neuron::JsonValue& Neuron::JsonObjectReader::Required(std::string_view _name)
{
  const JsonValue* value = Optional(_name);
  if (value == nullptr)
    JsonFail(Where(), std::format("has no \"{}\"", _name));
  return *value;
}

std::int32_t Neuron::JsonObjectReader::Integer(std::string_view _name, std::int32_t _minimum)
{
  return ReadJsonInteger(Required(_name), PathOf(_name), _minimum);
}

double Neuron::JsonObjectReader::Number(std::string_view _name, JsonBound _bound)
{
  return ReadJsonNumber(Required(_name), PathOf(_name), _bound);
}

std::string Neuron::JsonObjectReader::String(std::string_view _name)
{
  const JsonValue& value = Required(_name);
  if (!value.IsString())
    JsonFail(PathOf(_name), "expected a string");
  return value.AsString();
}

void Neuron::JsonObjectReader::Finish() const
{
  for (const JsonMember& member : m_value->AsObject())
  {
    if (std::ranges::find(m_read, member.name) == m_read.end())
      JsonFail(PathOf(member.name), "is not a member the game knows");
  }
}
