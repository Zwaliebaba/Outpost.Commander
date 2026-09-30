#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
void ExpectParseError(std::string_view _text)
{
  Assert::ExpectException<Neuron::Exception>([_text] { (void)Neuron::ParseJson(_text); });
}

std::string ParseErrorMessage(std::string_view _text)
{
  try
  {
    (void)Neuron::ParseJson(_text);
  }
  catch (const Neuron::Exception& error)
  {
    return error.what();
  }
  Assert::Fail(L"the text parsed");
  return {};
}
} // namespace

TEST_CLASS(JsonTests)
{
public:
  TEST_METHOD(ReadsLiterals)
  {
    Assert::IsTrue(Neuron::ParseJson("true").AsBool());
    Assert::IsFalse(Neuron::ParseJson("false").AsBool());
    Assert::IsTrue(Neuron::ParseJson("null").IsNull());
  }

  TEST_METHOD(ReadsNumbers)
  {
    Assert::AreEqual(0.0, Neuron::ParseJson("0").AsNumber());
    Assert::AreEqual(-125.0, Neuron::ParseJson("-12.5e1").AsNumber());
    Assert::AreEqual(100.0, Neuron::ParseJson("1E+2").AsNumber());
    Assert::AreEqual(0.4, Neuron::ParseJson("0.4").AsNumber());
    Assert::AreEqual(0.015, Neuron::ParseJson("1.5e-2").AsNumber());
  }

  TEST_METHOD(ReadsStringEscapes)
  {
    Assert::AreEqual(std::string("a\"b\\c/d\b\f\n\r\t"), Neuron::ParseJson(R"("a\"b\\c\/d\b\f\n\r\t")").AsString());
    Assert::AreEqual(std::string("Defence gun"), Neuron::ParseJson(R"("Defence gun")").AsString());
  }

  TEST_METHOD(ConvertsUnicodeEscapesToUtf8)
  {
    Assert::AreEqual(std::string("\xC3\xA9"), Neuron::ParseJson(R"("\u00e9")").AsString());
    Assert::AreEqual(std::string("\xE2\x82\xAC"), Neuron::ParseJson(R"("\u20AC")").AsString());
    // A surrogate pair is one code point outside the Basic Multilingual Plane.
    Assert::AreEqual(std::string("\xF0\x9F\x98\x80"), Neuron::ParseJson(R"("\ud83d\ude00")").AsString());
  }

  TEST_METHOD(ReadsNestedValuesInOrder)
  {
    const Neuron::JsonValue value = Neuron::ParseJson("{ \"b\": [1, {\"c\": true}], \"a\": \"x\", \"e\": {} }");
    const Neuron::JsonValue::Object& members = value.AsObject();
    Assert::AreEqual(size_t{3}, members.size());
    Assert::AreEqual(std::string("b"), members[0].name);
    Assert::AreEqual(std::string("a"), members[1].name);
    Assert::AreEqual(std::string("e"), members[2].name);

    const Neuron::JsonValue::Array& list = value.Find("b")->AsArray();
    Assert::AreEqual(size_t{2}, list.size());
    Assert::AreEqual(1.0, list[0].AsNumber());
    Assert::IsTrue(list[1].Find("c")->AsBool());
    Assert::IsTrue(value.Find("e")->AsObject().empty());
    Assert::IsNull(value.Find("d"));
  }

  TEST_METHOD(AcceptsWhitespaceAndByteOrderMark)
  {
    Assert::AreEqual(2.0, Neuron::ParseJson("\xEF\xBB\xBF\r\n\t [ 1 ,\r\n 2 ] \r\n").AsArray()[1].AsNumber());
  }

  TEST_METHOD(RejectsWhatJsonDoesNotAllow)
  {
    // No value, a misspelled literal, or more than one value.
    for (const std::string_view text : {"", " ", "tru", "nul", "1 2", "[1] x", "// comment\n1"})
      ExpectParseError(text);
    // Numbers outside JSON's grammar or out of range.
    for (const std::string_view text : {"01", "1.", ".5", "+1", "-", "1e", "1e+", "NaN", "Infinity", "1e400"})
      ExpectParseError(text);
    // Broken arrays and objects.
    for (const std::string_view text : {"[1,]", "[1 2]", "[1", "{\"a\":1,}", "{\"a\" 1}", "{a:1}", "{\"a\":1"})
      ExpectParseError(text);
    // Broken strings: unclosed, a raw control character, an unknown or short escape, single quotes.
    for (const std::string_view text : {"\"abc", "\"a\tb\"", "\"\\x\"", "\"\\u12\"", "\"\\", "'a'"})
      ExpectParseError(text);
  }

  TEST_METHOD(RejectsLoneSurrogates)
  {
    ExpectParseError(R"("\ud800")");
    ExpectParseError(R"("\ud800\u0041")");
    ExpectParseError(R"("\udc00")");
  }

  TEST_METHOD(RejectsADuplicateMemberName)
  {
    ExpectParseError(R"({"a": 1, "a": 2})");
  }

  TEST_METHOD(LimitsNesting)
  {
    Assert::IsTrue(Neuron::ParseJson(std::string(64, '[') + std::string(64, ']')).IsArray());
    ExpectParseError(std::string(1000, '[') + std::string(1000, ']'));
  }

  TEST_METHOD(NamesTheLineAndColumnOfAnError)
  {
    const std::string message = ParseErrorMessage("{\r\n  \"a\": x\r\n}");
    Assert::IsTrue(message.find("line 2, column 8") != std::string::npos);
  }

  TEST_METHOD(ThrowsOnTheWrongType)
  {
    const Neuron::JsonValue value = Neuron::ParseJson("[1]");
    Assert::ExpectException<Neuron::Exception>([&value] { (void)value.AsObject(); });
    Assert::ExpectException<Neuron::Exception>([&value] { (void)value.AsArray()[0].AsString(); });
    Assert::ExpectException<Neuron::Exception>([&value] { (void)value.Find("a"); });
  }
};
} // namespace GameLogicTests
