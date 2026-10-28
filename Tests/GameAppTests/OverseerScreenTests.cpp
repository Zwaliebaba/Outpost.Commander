#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Clock = std::chrono::steady_clock;

const Clock::time_point START{std::chrono::seconds{1'000}};
const Outpost::OverseerLink LINK{.server = "127.0.0.1:45001", .connected = true, .roundTrip = std::chrono::microseconds{800}};

// The interface's fonts as the overseer rasterizes them for a window of this size, kept for the run, since a TextMetrics reads
// them where they are.
Outpost::Hud::TextMetrics MetricsFor(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  static std::map<float, std::vector<Neuron::GlyphAtlas::Font>> g_fontsByScale;
  const float scale = Outpost::Hud::Scale(_widthPixels, _heightPixels);
  auto found = g_fontsByScale.find(scale);
  if (found == g_fontsByScale.end())
    found =
      g_fontsByScale.emplace(scale, Neuron::RasterizeUiAtlas(Outpost::Hud::Typefaces(), Outpost::Hud::Sprites(), scale).glyphs.fonts).first;
  return {found->second, scale};
}

Outpost::OverseerWelcomeMessage Welcome()
{
  return {.ticksPerSecond = 20,
          .folderUtf8 = "C:\\Users\\Owner\\Documents\\Outpost Commander\\Universe",
          .seats = {{.player = Outpost::PlayerId{1}, .ai = {}}, {.player = Outpost::PlayerId{2}, .ai = "Normal"}}};
}

// A model welcomed at START, told a Health of _tick at _heard.
Outpost::OverseerModel ModelWithHealth(std::uint64_t _tick, Clock::time_point _heard)
{
  Outpost::OverseerModel model;
  model.Take(Outpost::EncodeOverseerMessage(Welcome()), START);
  model.Take(Outpost::EncodeOverseerMessage(Outpost::HealthMessage{.tick = _tick}), _heard);
  return model;
}

// The overview laid out at 1920×1080 at _now.
Neuron::DrawList Lay(const Outpost::OverseerModel& _model, const Outpost::OverseerLink& _link, Clock::time_point _now)
{
  Outpost::OverseerScreen screen({});
  return screen.Lay(_model, _link, _now, MetricsFor(1920, 1080), 1920, 1080);
}

bool Shows(const Neuron::DrawList& _layout, std::string_view _text)
{
  return std::ranges::any_of(_layout.texts, [_text](const Neuron::CanvasText& _drawn) { return _drawn.text == _text; });
}

std::wstring Texts(const Neuron::DrawList& _layout)
{
  std::wstring all;
  for (const Neuron::CanvasText& text : _layout.texts)
    all += std::wstring(winrt::to_hstring(text.text)) + L" | ";
  return all;
}
} // namespace

TEST_CLASS(OverseerScreenTests)
{
public:
  // ADR-102: the header names the universe's folder and its server, says Running while Health comes, and tells universe time
  // and its tick.
  TEST_METHOD(SaysTheServerRunsWhileHealthComes)
  {
    const Neuron::DrawList layout = Lay(ModelWithHealth(std::uint64_t{20} * 3'725, START), LINK, START + std::chrono::seconds{2});
    Assert::IsTrue(Shows(layout, "Running"), Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "127.0.0.1:45001"), L"the server");
    Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Neuron::CanvasText& _text) { return _text.text.starts_with("C:\\Users"); }),
                   L"the universe's folder, cut short as it fits");
    Assert::IsTrue(Shows(layout, "1:02:05"), L"universe time");
    Assert::IsTrue(Shows(layout, "tick 74,500"), L"and its tick");
  }

  // ADR-102: No word once Health has not come for NO_WORD_AFTER, saying since when and at what tick.
  TEST_METHOD(SaysNoWordAfterThreeSecondsWithoutHealth)
  {
    const Outpost::OverseerModel model = ModelWithHealth(400, START);
    const Neuron::DrawList quiet = Lay(model, LINK, START + Outpost::OverseerScreen::NO_WORD_AFTER);
    Assert::IsTrue(Shows(quiet, "No word"), Texts(quiet).c_str());
    Assert::IsTrue(Shows(quiet, "Connected; no Health for 3 s."), Texts(quiet).c_str());
    Assert::IsTrue(Shows(quiet, "The last word was 0:03 ago, at tick 400."), Texts(quiet).c_str());
    Assert::IsTrue(Shows(Lay(model, LINK, START + std::chrono::milliseconds{2'900}), "Running"), L"until then");
  }

  // ADR-102: a connection gone says why, and that the overseer joins again; what the server said as it ended says how.
  TEST_METHOD(SaysWhyTheConnectionWent)
  {
    Outpost::OverseerModel model = ModelWithHealth(400, START);
    model.Lost("The connection timed out.", START + std::chrono::seconds{1});
    Outpost::OverseerLink gone = LINK;
    gone.connected = false;
    const Neuron::DrawList lost = Lay(model, gone, START + std::chrono::seconds{1});
    Assert::IsTrue(Shows(lost, "No word"), L"no word at once, without a connection");
    Assert::IsTrue(Shows(lost, "The connection went: The connection timed out."), Texts(lost).c_str());
    Assert::IsTrue(Shows(lost, "Joining 127.0.0.1:45001 every 2 s."), Texts(lost).c_str());

    model.Take(
      Outpost::EncodeOverseerMessage(Outpost::ClosingMessage{.ending = Outpost::ServerEnding::StoppedByOwner, .lastSaveTick = 380}), START);
    const Neuron::DrawList stopped = Lay(model, gone, START + std::chrono::seconds{1});
    Assert::IsTrue(Shows(stopped, "The server was stopped by its owner; its last save is of tick 380."), Texts(stopped).c_str());
  }

  // ADR-102: before any connection, the overseer says whom it joins.
  TEST_METHOD(SaysWhomItJoinsBeforeAnyConnection)
  {
    Outpost::OverseerLink joining = LINK;
    joining.connected = false;
    const Neuron::DrawList layout = Lay(Outpost::OverseerModel(), joining, START);
    Assert::IsTrue(Shows(layout, "No word"));
    Assert::IsTrue(Shows(layout, "Joining 127.0.0.1:45001 every 2 s."), Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "Not yet told"), L"no folder before a welcome");
  }

  TEST_METHOD(WritesUniverseTimeInDays)
  {
    Assert::AreEqual(std::string("0:00:00"), Outpost::UniverseTime(0, 20));
    Assert::AreEqual(std::string("5:02:03"), Outpost::UniverseTime(std::uint64_t{20} * ((5 * 3'600) + (2 * 60) + 3), 20));
    Assert::AreEqual(std::string("3 d 04:05:06"),
                     Outpost::UniverseTime(std::uint64_t{20} * ((3 * 86'400) + (4 * 3'600) + (5 * 60) + 6), 20));
  }
};
} // namespace GameAppTests
