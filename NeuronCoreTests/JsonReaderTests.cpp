#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
namespace
{
using Neuron::JsonBound;
using Neuron::JsonObjectReader;

// An identifier type of the kind JsonObjectReader::Identifier builds, with no game concept behind it.
struct WidgetId
{
  std::uint32_t value = 0;
};

struct Widget
{
  WidgetId id;
  std::string name;
};

// The message _read fails with.
template <typename Fn> std::string FailureOf(Fn _read)
{
  std::string message;
  try
  {
    _read();
  }
  catch (const Neuron::Exception& error)
  {
    message = error.what();
  }
  Assert::IsFalse(message.empty(), L"it was read");
  return message;
}

std::string ReadIntegerFailure(std::string_view _text, std::int32_t _minimum)
{
  const Neuron::JsonValue value = Neuron::ParseJson(_text);
  return FailureOf([&] { (void)Neuron::ReadJsonInteger(value, "count", _minimum); });
}

std::string ReadNumberFailure(std::string_view _text, JsonBound _bound)
{
  const Neuron::JsonValue value = Neuron::ParseJson(_text);
  return FailureOf([&] { (void)Neuron::ReadJsonNumber(value, "ratio", _bound); });
}
} // namespace

// JsonReader.h: reading a parsed document into typed data, where every error names the place it is about, as
// "<path>: <what is wrong>".
TEST_CLASS(JsonReaderTests)
{
public:
  TEST_METHOD(ReadsEachKindOfMember)
  {
    const Neuron::JsonValue document = Neuron::ParseJson(R"({"count": 3, "ratio": 0.25, "name": "Ion", "id": 7, "offset": -4})");
    JsonObjectReader reader(document, "");
    Assert::AreEqual(3, reader.Integer("count", 0));
    Assert::AreEqual(0.25, reader.Number("ratio", JsonBound::Positive));
    Assert::AreEqual(std::string("Ion"), reader.String("name"));
    Assert::AreEqual(7u, reader.Identifier<WidgetId>("id").value);
    Assert::AreEqual(-4, reader.Integer("offset", std::numeric_limits<std::int32_t>::min()));
    reader.Finish();
  }

  // The header's own example, read through a list as a loader reads one.
  TEST_METHOD(NamesThePlaceOfAnErrorInsideAList)
  {
    const Neuron::JsonValue document = Neuron::ParseJson(R"({"hulls": [{"armor": 1}, {"armor": 2.5}]})");
    Assert::AreEqual(std::string("hulls[1].armor: expected a whole number, found 2.5"),
                     FailureOf(
                       [&]
                       {
                         JsonObjectReader reader(document, "");
                         (void)Neuron::ReadJsonList<std::int32_t>(reader, "hulls",
                                                                  [](JsonObjectReader& _hull) { return _hull.Integer("armor", 0); });
                       }));
  }

  // The document itself is "the file"; a member of it is named alone, and a member of a member after a dot.
  TEST_METHOD(NamesTheDocumentAndItsMembers)
  {
    const Neuron::JsonValue document = Neuron::ParseJson(R"({"rules": {"tickHz": 20}})");
    JsonObjectReader top(document, "");
    Assert::AreEqual(std::string("the file"), top.Where());
    Assert::AreEqual(std::string("rules"), top.PathOf("rules"));
    Assert::AreEqual(std::string("the file: has no \"sight\""), FailureOf([&] { (void)top.Required("sight"); }));

    JsonObjectReader rules(top.Required("rules"), top.PathOf("rules"));
    Assert::AreEqual(std::string("rules"), rules.Where());
    Assert::AreEqual(std::string("rules.tickHz"), rules.PathOf("tickHz"));
    Assert::AreEqual(std::string("rules: has no \"startingOre\""), FailureOf([&] { (void)rules.Integer("startingOre", 0); }));
    Assert::AreEqual(std::string("hulls[3]"), Neuron::JsonElementPath("hulls", 3));
  }

  TEST_METHOD(RefusesAValueOfTheWrongType)
  {
    const Neuron::JsonValue document = Neuron::ParseJson(R"({"name": 5, "count": "five", "list": {}, "inner": []})");
    JsonObjectReader reader(document, "");
    Assert::AreEqual(std::string("name: expected a string"), FailureOf([&] { (void)reader.String("name"); }));
    Assert::AreEqual(std::string("count: expected a number"), FailureOf([&] { (void)reader.Integer("count", 0); }));
    Assert::AreEqual(std::string("count: expected a number"), FailureOf([&] { (void)reader.Number("count", JsonBound::Any); }));
    Assert::AreEqual(std::string("list: expected an array"),
                     FailureOf([&] { (void)Neuron::ReadJsonArray(reader.Required("list"), "list"); }));
    Assert::AreEqual(std::string("inner: expected an object"),
                     FailureOf([&] { (void)JsonObjectReader(reader.Required("inner"), "inner"); }));

    const Neuron::JsonValue array = Neuron::ParseJson("[1, 2]");
    Assert::AreEqual(std::string("the file: expected an object"), FailureOf([&] { (void)JsonObjectReader(array, ""); }));
  }

  // A whole number at least the minimum, and no larger than an int32_t holds.
  TEST_METHOD(ReadsAnIntegerWithinItsBounds)
  {
    Assert::AreEqual(std::numeric_limits<std::int32_t>::max(), Neuron::ReadJsonInteger(Neuron::ParseJson("2147483647"), "count", 0));
    Assert::AreEqual(0, Neuron::ReadJsonInteger(Neuron::ParseJson("0"), "count", 0));
    // A number written with a fraction or an exponent is still whole if its value is.
    Assert::AreEqual(12, Neuron::ReadJsonInteger(Neuron::ParseJson("12.0"), "count", 0));
    Assert::AreEqual(1200, Neuron::ReadJsonInteger(Neuron::ParseJson("1.2e3"), "count", 0));

    Assert::AreEqual(std::string("count: expected a whole number, found 2.5"), ReadIntegerFailure("2.5", 0));
    Assert::AreEqual(std::string("count: expected at least 1, found 0"), ReadIntegerFailure("0", 1));
    Assert::AreEqual(std::string("count: expected at least 0, found -3"), ReadIntegerFailure("-3", 0));
    Assert::AreEqual(std::string("count: 2147483648 is too large"), ReadIntegerFailure("2147483648", 0));
  }

  TEST_METHOD(ReadsANumberWithinItsBound)
  {
    Assert::AreEqual(-1.5, Neuron::ReadJsonNumber(Neuron::ParseJson("-1.5"), "ratio", JsonBound::Any));
    Assert::AreEqual(0.0, Neuron::ReadJsonNumber(Neuron::ParseJson("0"), "ratio", JsonBound::NotNegative));
    Assert::AreEqual(1e-9, Neuron::ReadJsonNumber(Neuron::ParseJson("1e-9"), "ratio", JsonBound::Positive));

    Assert::AreEqual(std::string("ratio: expected at least 0, found -0.5"), ReadNumberFailure("-0.5", JsonBound::NotNegative));
    Assert::AreEqual(std::string("ratio: expected more than 0, found 0"), ReadNumberFailure("0", JsonBound::Positive));
    Assert::AreEqual(std::string("ratio: expected more than 0, found -2"), ReadNumberFailure("-2", JsonBound::Positive));
  }

  // Finish rejects a member nothing asked for, so that a misspelled optional member is an error rather than ignored. A
  // member asked for with Optional counts as read, and an Optional member that is absent is nothing.
  TEST_METHOD(FinishRejectsAMemberNothingAskedFor)
  {
    const Neuron::JsonValue document = Neuron::ParseJson(R"({"speed": 3, "sped": 2})");
    JsonObjectReader reader(document, "ship");
    Assert::IsNotNull(reader.Optional("speed"));
    Assert::IsNull(reader.Optional("range"));
    Assert::AreEqual(std::string("ship.sped: is not a member the game knows"), FailureOf([&] { reader.Finish(); }));

    Assert::IsNotNull(reader.Optional("sped"));
    reader.Finish();
  }

  // Each element of a list is read in order and then finished, and its place is its index.
  TEST_METHOD(ReadsAndFinishesEachElementOfAList)
  {
    const Neuron::JsonValue document = Neuron::ParseJson(R"({"widgets": [{"id": 4, "name": "a"}, {"id": 9, "name": "b"}]})");
    JsonObjectReader reader(document, "");
    const std::vector<Widget> widgets =
      Neuron::ReadJsonList<Widget>(reader, "widgets", [](JsonObjectReader& _widget)
                                   { return Widget{.id = _widget.Identifier<WidgetId>("id"), .name = _widget.String("name")}; });
    Assert::AreEqual(size_t{2}, widgets.size());
    Assert::AreEqual(4u, widgets[0].id.value);
    Assert::AreEqual(std::string("a"), widgets[0].name);
    Assert::AreEqual(9u, widgets[1].id.value);
    Assert::AreEqual(std::string("b"), widgets[1].name);
    reader.Finish();

    const auto readIds = [](const Neuron::JsonValue& _document)
    {
      JsonObjectReader top(_document, "");
      return Neuron::ReadJsonList<WidgetId>(top, "widgets", [](JsonObjectReader& _widget) { return _widget.Identifier<WidgetId>("id"); });
    };
    const Neuron::JsonValue extra = Neuron::ParseJson(R"({"widgets": [{"id": 1}, {"id": 2, "size": 3}]})");
    Assert::AreEqual(std::string("widgets[1].size: is not a member the game knows"), FailureOf([&] { (void)readIds(extra); }));
    const Neuron::JsonValue notObjects = Neuron::ParseJson(R"({"widgets": [7]})");
    Assert::AreEqual(std::string("widgets[0]: expected an object"), FailureOf([&] { (void)readIds(notObjects); }));
    const Neuron::JsonValue notAList = Neuron::ParseJson(R"({"widgets": {"id": 1}})");
    Assert::AreEqual(std::string("widgets: expected an array"), FailureOf([&] { (void)readIds(notAList); }));
    // An identifier is 1 or more.
    const Neuron::JsonValue zero = Neuron::ParseJson(R"({"widgets": [{"id": 0}]})");
    Assert::AreEqual(std::string("widgets[0].id: expected at least 1, found 0"), FailureOf([&] { (void)readIds(zero); }));
  }
};
} // namespace NeuronCoreTests
