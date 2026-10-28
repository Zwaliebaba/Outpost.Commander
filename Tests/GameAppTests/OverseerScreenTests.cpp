#include "pch.h"
#include "LayoutGuards.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Clock = std::chrono::steady_clock;
using Outpost::OverseerScreen;

const Clock::time_point START{std::chrono::seconds{1'000}};
// The link as the overseer finds it, connected to its server.
Outpost::OverseerLink Link()
{
  return {.server = "127.0.0.1:45001", .connected = true, .roundTrip = std::chrono::microseconds{800}};
}
// Seat 1's set's color, as Models.json gives it; seat 2 has none here, so that the palette's first color is seat 3's.
constexpr DirectX::XMFLOAT4 BLUE{0.18f, 0.42f, 0.95f, 1.0f};
// An hour of universe time at 20 ticks a second.
constexpr std::uint64_t HOUR_TICK = std::uint64_t{20} * 3'600;

Outpost::OverseerWelcomeMessage Welcome()
{
  return {.wireLayoutHash = 0x1234'5678'9abc'def0,
          .protocolVersion = 1,
          .stateVersion = 1,
          .ticksPerSecond = 20,
          .folderUtf8 = "C:\\Users\\Owner\\Documents\\Outpost Commander\\Universe",
          .seats = {{.player = Outpost::PlayerId{1}, .ai = {}},
                    {.player = Outpost::PlayerId{2}, .ai = "Normal"},
                    {.player = Outpost::PlayerId{3}, .ai = {}},
                    {.player = Outpost::PlayerId{4}, .ai = "Hard"}},
          .recoveredSaveTick = 1'200,
          .replayedCommands = 7,
          .dayRecords = {"start 0 new", "seat 0 player 1 deputy", "seat 0 player 3 deputy", "seat 36000 player 1 player",
                         "start 50000 recovered save 49000 replayed 2", "battle 60000 60200 sector 2 player 1 present player 2 ai",
                         "battle 70000 70100 sector 3 player 3 deputy player 2 ai"}};
}

// _model welcomed at START, and told a Health of _tick at _heard.
void TellHealth(Outpost::OverseerModel& _model, std::uint64_t _tick, Clock::time_point _heard)
{
  _model.Take(Outpost::EncodeOverseerMessage(Welcome()), START);
  _model.Take(Outpost::EncodeOverseerMessage(Outpost::HealthMessage{.tick = _tick}), _heard);
}

// The seats as the server's thread kept them in the second of _second, of 61 that end at an hour of universe time: seat 1's
// player online for 2:05 with 10 KB out a second, seat 2 an AI empire, seat 3 away while its deputy plays, and seat 4 an AI
// empire fallen, restarting in 5:00.
std::vector<Outpost::SeatHealth> SeatsAt(std::uint64_t _second)
{
  return {
    {.player = Outpost::PlayerId{1},
     .play = Outpost::SeatPlay::Player,
     .link = Outpost::SeatLink::Online,
     .linkTick = HOUR_TICK - (std::uint64_t{20} * 125),
     .connection =
       Outpost::LinkFigures{
         .roundTripMicroseconds = 38'000, .packetsSent = 1'000, .packetsLost = 2, .backlogBytes = 2'048, .bytesSent = 10'240 * _second},
     .orders = 2,
     .ore = 12'000,
     .incomeHundredthsPerSecond = 4'050,
     .commandPoints = 120,
     .fleetCap = 200,
     .nodes = 6,
     .researched = 9},
    {.player = Outpost::PlayerId{2}, .play = Outpost::SeatPlay::Ai, .ore = 5'000, .commandPoints = 80, .fleetCap = 200, .nodes = 4},
    {.player = Outpost::PlayerId{3},
     .play = Outpost::SeatPlay::Deputy,
     .link = Outpost::SeatLink::Gone,
     .linkTick = HOUR_TICK - (std::uint64_t{20} * 600),
     .deputyTick = HOUR_TICK - (std::uint64_t{20} * 540),
     .ore = 800,
     .nodes = 2},
    {.player = Outpost::PlayerId{4}, .play = Outpost::SeatPlay::Ai, .ore = 300, .restartTick = HOUR_TICK + (std::uint64_t{20} * 300)}};
}

// A universe an hour old, as the overseer knows it after 61 Health, the last heard at START plus 60 s: in the last minute
// 1,180 ticks of 1 ms and 20 of 3 ms, before it one of 4.2 ms and 3 ticks dropped; and the log's records.
void Script(Outpost::OverseerModel& _model)
{
  _model.Take(Outpost::EncodeOverseerMessage(Welcome()), START);
  for (std::uint64_t second = 0; second <= 60; ++second)
  {
    Outpost::HealthMessage health{.tick = HOUR_TICK - ((60 - second) * 20), .seats = SeatsAt(second)};
    health.tickMicroseconds.assign(20, 1'000);
    if (second == 0)
    {
      health.tickMicroseconds.front() = 4'200;
      health.droppedTicks = 3;
    }
    else if (second == 60)
      health.tickMicroseconds.assign(20, 3'000);
    health.process = {.workingSetBytes = 190'000'000,
                      .privateBytes = 150'000'000,
                      .cpuHundredths = 12,
                      .threads = 42,
                      .handles = 310,
                      .uptimeSeconds = 3'600,
                      .diskFreeBytes = 120ull * 1'024 * 1'024 * 1'024};
    _model.Take(Outpost::EncodeOverseerMessage(health), START + std::chrono::seconds{second});
  }
  _model.Take(Outpost::EncodeOverseerMessage(
                Outpost::LogRecordsMessage{.lines = {"start 0 new", "save 70000 bytes 12700 encode_us 310", "seat 71000 player 3 deputy",
                                                     "battle 71500 71900 sector 4 player 1 present player 2 ai"}}),
              START);
}

// A model told what Script tells it.
struct Scripted
{
  Outpost::OverseerModel model;

  Scripted()
  {
    Script(model);
  }
};

const Clock::time_point NOW = START + std::chrono::seconds{60};

// The overview laid out at this size.
const Neuron::DrawList& Lay(OverseerScreen& _screen, const Outpost::OverseerModel& _model, Clock::time_point _now,
                            const Outpost::OverseerLink& _link = Link(), std::uint32_t _widthPixels = 1920,
                            std::uint32_t _heightPixels = 1080)
{
  return _screen.Lay(_model, _link, _now, MetricsFor(_widthPixels, _heightPixels), _widthPixels, _heightPixels);
}

Neuron::DrawList Lay(const Outpost::OverseerModel& _model, const Outpost::OverseerLink& _link, Clock::time_point _now)
{
  OverseerScreen screen({});
  return Lay(screen, _model, _now, _link);
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

// Where _text first stands among the layout's texts, which the order they are drawn in follows.
std::size_t IndexOf(const Neuron::DrawList& _layout, std::string_view _text)
{
  const auto found = std::ranges::find(_layout.texts, _text, &Neuron::CanvasText::text);
  Assert::IsTrue(found != _layout.texts.end(), std::wstring(winrt::to_hstring(_text)).c_str());
  return static_cast<std::size_t>(found - _layout.texts.begin());
}

// The seats the table shows, in the order it shows them: each "Seat n" alone, not a sentence that names one.
std::vector<std::string> SeatsShown(const Neuron::DrawList& _layout)
{
  std::vector<std::string> seats;
  for (const Neuron::CanvasText& text : _layout.texts)
  {
    const std::string_view number = std::string_view(text.text).substr(std::min<std::size_t>(5, text.text.size()));
    if (text.text.starts_with("Seat ") && !number.empty() &&
        std::ranges::all_of(number, [](char _digit) { return _digit >= '0' && _digit <= '9'; }))
      seats.push_back(text.text);
  }
  return seats;
}

// A press of the left button, down and up, at the middle of the first region of the layout that _which picks.
Neuron::InputState PressOn(const Neuron::DrawList& _layout, const std::function<bool(const Neuron::CanvasRegion&)>& _which)
{
  const auto region = std::ranges::find_if(_layout.regions, _which);
  Assert::IsTrue(region != _layout.regions.end(), L"a region to press");
  const auto x = static_cast<std::int32_t>(region->area.left + (region->area.width / 2.0f));
  const auto y = static_cast<std::int32_t>(region->area.top + (region->area.height / 2.0f));
  Neuron::InputState input{.active = true, .cursorXPixels = x, .cursorYPixels = y};
  input.events.push_back({.kind = Neuron::InputEventKind::ButtonDown, .key = VK_LBUTTON, .xPixels = x, .yPixels = y});
  input.events.push_back({.kind = Neuron::InputEventKind::ButtonUp, .key = VK_LBUTTON, .xPixels = x, .yPixels = y});
  return input;
}

// The face of the row that shows _seat: the panel under its "Seat n".
const Neuron::CanvasRect& RowFaceOf(const Neuron::DrawList& _layout, std::uint32_t _seat)
{
  const Neuron::CanvasText& text = _layout.texts[IndexOf(_layout, std::format("Seat {}", _seat))];
  // The last drawn of those under it that are wide, since the panel the table stands in is drawn first.
  const auto face = std::ranges::find_if(_layout.panels.rbegin(), _layout.panels.rend(), [&text](const Neuron::CanvasRect& _panel)
                                         { return _panel.width > 200.0f && _panel.Contains(text.left + 1.0f, text.top + 1.0f); });
  Assert::IsTrue(face != _layout.panels.rend());
  return *face;
}

bool Same(const DirectX::XMFLOAT4& _first, const DirectX::XMFLOAT4& _second) noexcept
{
  return _first.x == _second.x && _first.y == _second.y && _first.z == _second.z && _first.w == _second.w;
}

// _model given a view of a map 4 km across, in full sight or _seat's: the sector at its south-west held by seat 1, seat 1's
// ship in it, and seat 3's structure in the north-east.
void TellView(Outpost::OverseerModel& _model, Outpost::PlayerId _seat)
{
  Outpost::Snapshot view{.tick = HOUR_TICK, .player = _seat};
  view.mapSizeMeters = 4'000.0f;
  view.sectors = {
    {.id = 1, .minXMeters = -2'000.0f, .maxXMeters = 0.0f, .minZMeters = -2'000.0f, .maxZMeters = 0.0f, .holder = Outpost::PlayerId{1}},
    {.id = 2, .minXMeters = 0.0f, .maxXMeters = 2'000.0f, .minZMeters = 0.0f, .maxZMeters = 2'000.0f}};
  view.entities = {{.id = Outpost::EntityId{1},
                    .kind = Outpost::EntityKind::Ship,
                    .owner = Outpost::PlayerId{1},
                    .position = {.xMeters = -1'000.0f, .zMeters = -1'000.0f},
                    .radiusMeters = 10.0f},
                   {.id = Outpost::EntityId{2},
                    .kind = Outpost::EntityKind::Structure,
                    .owner = Outpost::PlayerId{3},
                    .position = {.xMeters = 1'000.0f, .zMeters = 1'000.0f},
                    .radiusMeters = 40.0f}};
  Assert::IsTrue(_model.TakeView(Outpost::EncodeMessage(Outpost::StateMessage{.snapshot = std::move(view)})).has_value());
}

// The map's square at 1920×1080, 306 units across, and how many of the panels drawn in it are of _color.
const Neuron::CanvasRect* MapSquareOf(const Neuron::DrawList& _layout)
{
  const auto square = std::ranges::find_if(_layout.panels, [](const Neuron::CanvasRect& _panel)
                                           { return _panel.width == 306.0f && _panel.height == 306.0f; });
  return square != _layout.panels.end() ? &*square : nullptr;
}

std::ptrdiff_t MarksIn(const Neuron::DrawList& _layout, const Neuron::CanvasRect& _square, const DirectX::XMFLOAT4& _color)
{
  return std::ranges::count_if(_layout.panels,
                               [&](const Neuron::CanvasRect& _panel)
                               {
                                 return Same(_panel.color, _color) && _panel.width < _square.width &&
                                        _square.Contains(_panel.left + (_panel.width / 2.0f), _panel.top + (_panel.height / 2.0f));
                               });
}
} // namespace

TEST_CLASS(OverseerScreenTests)
{
public:
  // ADR-103: the header names the universe's folder, its server and the build it runs, says Running while Health comes, how
  // long the server has run, the overseer's round trip and what the server came back from, and tells universe time and its
  // tick.
  TEST_METHOD(SaysTheServerRunsWhileHealthComes)
  {
    const Scripted scripted;
    const Neuron::DrawList layout = Lay(scripted.model, Link(), NOW + std::chrono::seconds{2});
    Assert::IsTrue(Shows(layout, "Running"), Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "127.0.0.1:45001"), L"the server");
    Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Neuron::CanvasText& _text) { return _text.text.starts_with("C:\\Users"); }),
                   L"the universe's folder, cut short as it fits");
    Assert::IsTrue(Shows(layout, "wire 123456789abcdef0 \xC2\xB7 protocol 1 \xC2\xB7 state 1"), Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "Up 1:00:00; the overseer's round trip 0.80 ms."), Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "It came back from the save of tick 1,200, replaying 7 commands."), Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "1:00:00"), L"universe time");
    Assert::IsTrue(Shows(layout, "tick 72,000"), L"and its tick");
  }

  // ADR-103: No word once Health has not come for NO_WORD_AFTER, saying since when and at what tick.
  TEST_METHOD(SaysNoWordAfterThreeSecondsWithoutHealth)
  {
    Outpost::OverseerModel model;
    TellHealth(model, 400, START);
    const Neuron::DrawList quiet = Lay(model, Link(), START + OverseerScreen::NO_WORD_AFTER);
    Assert::IsTrue(Shows(quiet, "No word"), Texts(quiet).c_str());
    Assert::IsTrue(Shows(quiet, "Connected; no Health for 3 s."), Texts(quiet).c_str());
    Assert::IsTrue(Shows(quiet, "The last word was 0:03 ago, at tick 400."), Texts(quiet).c_str());
    Assert::IsTrue(Shows(Lay(model, Link(), START + std::chrono::milliseconds{2'900}), "Running"), L"until then");
  }

  // ADR-103: a connection gone says why, and that the overseer joins again; once the server said it was stopped, the header says
  // Stopped, when, and the save it comes back from.
  TEST_METHOD(SaysWhyTheConnectionWent)
  {
    Outpost::OverseerModel model;
    TellHealth(model, 400, START);
    model.Lost("The connection timed out.", START + std::chrono::seconds{1});
    Outpost::OverseerLink gone = Link();
    gone.connected = false;
    const Neuron::DrawList lost = Lay(model, gone, START + std::chrono::seconds{1});
    Assert::IsTrue(Shows(lost, "No word"), L"no word at once, without a connection");
    Assert::IsTrue(Shows(lost, "The connection went: The connection timed out."), Texts(lost).c_str());
    Assert::IsTrue(Shows(lost, "Joining 127.0.0.1:45001 every 2 s."), Texts(lost).c_str());

    model.Take(
      Outpost::EncodeOverseerMessage(Outpost::ClosingMessage{.ending = Outpost::ServerEnding::StoppedByOwner, .lastSaveTick = 380}), START);
    const Neuron::DrawList stopped = Lay(model, gone, START + std::chrono::seconds{1});
    Assert::IsTrue(Shows(stopped, "Stopped") && !Shows(stopped, "No word"), Texts(stopped).c_str());
    Assert::IsTrue(Shows(stopped, "Stopped by its owner 0:01 ago.") && Shows(stopped, "Its last save is of tick 380."),
                   Texts(stopped).c_str());
    Assert::IsTrue(Shows(stopped, "Joining 127.0.0.1:45001 every 2 s."), Texts(stopped).c_str());
  }

  // ADR-103 decision 7: Stalled while Health comes and its tick has stood for STALLED_AFTER, which a stuck server's thread does
  // while its main thread still reports; Running again once the tick moves. Failed once the server said so, with its message,
  // and Stopped by Windows; each until a newer connection's welcome.
  TEST_METHOD(SaysStalledFailedAndStoppedByWindows)
  {
    Outpost::OverseerModel model;
    model.Take(Outpost::EncodeOverseerMessage(Welcome()), START);
    for (int second = 0; second <= 1; ++second)
      model.Take(Outpost::EncodeOverseerMessage(Outpost::HealthMessage{.tick = 400}), START + std::chrono::seconds{second});
    Assert::IsTrue(Shows(Lay(model, Link(), START + std::chrono::milliseconds{2'900}), "Running"),
                   L"a tick that stood a second, and no Health since: not yet No word, and not Stalled");
    model.Take(Outpost::EncodeOverseerMessage(Outpost::HealthMessage{.tick = 400}), START + std::chrono::seconds{2});
    const Neuron::DrawList stalled = Lay(model, Link(), START + std::chrono::seconds{2});
    Assert::IsTrue(Shows(stalled, "Stalled"), Texts(stalled).c_str());
    Assert::IsTrue(Shows(stalled, "Tick 400 has stood for 0:02; that time goes unrun."), Texts(stalled).c_str());
    model.Take(Outpost::EncodeOverseerMessage(Outpost::HealthMessage{.tick = 420}), START + std::chrono::seconds{3});
    Assert::IsTrue(Shows(Lay(model, Link(), START + std::chrono::seconds{3}), "Running"), L"the tick moves again");

    Outpost::OverseerLink gone = Link();
    gone.connected = false;
    model.Take(Outpost::EncodeOverseerMessage(Outpost::ClosingMessage{
                 .ending = Outpost::ServerEnding::Failed, .messageUtf8 = "The save could not be written.", .lastSaveTick = 380}),
               START + std::chrono::seconds{4});
    const Neuron::DrawList failed = Lay(model, gone, START + std::chrono::seconds{9});
    Assert::IsTrue(Shows(failed, "Failed"), Texts(failed).c_str());
    Assert::IsTrue(Shows(failed, "The save could not be written."), Texts(failed).c_str());
    Assert::IsTrue(Shows(failed, "Failed 0:05 ago; it comes back from the save of tick 380."), Texts(failed).c_str());

    model.Take(Outpost::EncodeOverseerMessage(Welcome()), START + std::chrono::seconds{10});
    model.Take(Outpost::EncodeOverseerMessage(Outpost::HealthMessage{.tick = 380}), START + std::chrono::seconds{10});
    Assert::IsTrue(Shows(Lay(model, Link(), START + std::chrono::seconds{10}), "Running"), L"a newer connection's welcome");
    model.Take(
      Outpost::EncodeOverseerMessage(Outpost::ClosingMessage{.ending = Outpost::ServerEnding::StoppedByWindows, .lastSaveTick = 1'200}),
      START + std::chrono::seconds{11});
    const Neuron::DrawList windows = Lay(model, Link(), START + std::chrono::seconds{11});
    Assert::IsTrue(Shows(windows, "Stopped"), L"while the connection still stands");
    Assert::IsTrue(Shows(windows, "Stopped by Windows 0:00 ago, as it signed out or shut down."), Texts(windows).c_str());
    Assert::IsTrue(Shows(windows, "Its last save is of tick 1,200."), Texts(windows).c_str());
  }

  // ADR-103: before any connection, the overseer says whom it joins, and shows no seat.
  TEST_METHOD(SaysWhomItJoinsBeforeAnyConnection)
  {
    Outpost::OverseerLink joining = Link();
    joining.connected = false;
    const Neuron::DrawList layout = Lay(Outpost::OverseerModel(), joining, START);
    Assert::IsTrue(Shows(layout, "No word"));
    Assert::IsTrue(Shows(layout, "Joining 127.0.0.1:45001 every 2 s."), Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "Not yet told"), L"no folder before a welcome");
    Assert::IsTrue(SeatsShown(layout).empty());
    Assert::IsTrue(Shows(layout, "no Health yet"));
  }

  // ADR-103: the tiles tell the tick's 99th percentile over the last minute against its budget and the hour's longest; the ticks
  // a second and those dropped in the hour; the last save, its size and encoding time, from the log; the bytes out a second
  // and to how many seats; and the process's working set, its share of a core and the disk free.
  TEST_METHOD(ShowsTheHealthTiles)
  {
    const Scripted scripted;
    const Neuron::DrawList layout = Lay(scripted.model, Link(), NOW);
    for (const std::string_view text :
         {"3.00 ms", "of 5 ms; the hour's longest 4.20 ms", "20", "3 dropped this hour", "1:40 ago",
          "tick 70,000 \xC2\xB7 12.4 KB \xC2\xB7 0.31 ms", "10.0 KB/s", "to 1 seat", "181 MB", "12% of a core \xC2\xB7 120 GB free"})
      Assert::IsTrue(Shows(layout, text), std::format(L"{}: {}", std::wstring(winrt::to_hstring(text)), Texts(layout)).c_str());
  }

  // ADR-103: a row a seat, with who plays it, its status, its link, its orders a minute, its Ore and income, its fleet against
  // its cap, its nodes and its research.
  TEST_METHOD(ListsTheSeats)
  {
    const Scripted scripted;
    const Neuron::DrawList layout = Lay(scripted.model, Link(), NOW);
    Assert::IsTrue(SeatsShown(layout) == std::vector<std::string>{"Seat 1", "Seat 2", "Seat 3", "Seat 4"}, Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "SEATS \xC2\xB7 4 SHOWN"));
    for (const std::string_view text : {"player",       "Online",    "for 2:05",     "38.00 ms",        "loss 0.2% \xC2\xB7 backlog 2.0 KB",
                                        "120 a minute", "0 refused", "12,000",       "+40.50 a second", "120 / 200",
                                        "AI, Normal",   "Playing",   "an AI empire", "no connection",   "deputy",
                                        "Away",         "for 10:00", "AI, Hard",     "Fallen",          "restarts in 5:00"})
      Assert::IsTrue(Shows(layout, text), std::format(L"{}: {}", std::wstring(winrt::to_hstring(text)), Texts(layout)).c_str());
  }

  // ADR-103: the filters keep the players' seats, the AI empires' or the away seats; a heading sorts by its column, a second
  // press the other way; and a row picked asks for its sight, and asks for full sight again when pressed again.
  TEST_METHOD(FiltersSortsAndPicks)
  {
    const Scripted scripted;
    const Outpost::OverseerModel& model = scripted.model;
    OverseerScreen screen({{.player = Outpost::PlayerId{1}, .color = BLUE}});
    screen.Filter(OverseerScreen::SeatFilter::Players);
    Assert::IsTrue(SeatsShown(Lay(screen, model, NOW)) == std::vector<std::string>{"Seat 1", "Seat 3"});
    screen.Filter(OverseerScreen::SeatFilter::Ai);
    Assert::IsTrue(SeatsShown(Lay(screen, model, NOW)) == std::vector<std::string>{"Seat 2", "Seat 4"});
    screen.Filter(OverseerScreen::SeatFilter::Away);
    Assert::IsTrue(SeatsShown(Lay(screen, model, NOW)) == std::vector<std::string>{"Seat 3"});

    // The All cell, pressed.
    screen.Input(PressOn(Lay(screen, model, NOW), [](const Neuron::CanvasRegion& _region) { return _region.payload == ((2u << 16) | 0); }));
    Assert::IsTrue(SeatsShown(Lay(screen, model, NOW)).size() == 4, L"All, pressed");

    // ORE's heading, pressed: by Ore, the least first; pressed again, the most first.
    const auto ore = [](const Neuron::CanvasRegion& _region) { return _region.payload == ((1u << 16) | 4); };
    screen.Input(PressOn(Lay(screen, model, NOW), ore));
    Assert::IsTrue(SeatsShown(Lay(screen, model, NOW)) == std::vector<std::string>{"Seat 4", "Seat 3", "Seat 2", "Seat 1"});
    screen.Input(PressOn(Lay(screen, model, NOW), ore));
    const Neuron::DrawList& sorted = Lay(screen, model, NOW);
    Assert::IsTrue(SeatsShown(sorted) == std::vector<std::string>{"Seat 1", "Seat 2", "Seat 3", "Seat 4"});
    Assert::IsTrue(Shows(sorted, "v"), L"the heading's mark");

    // Seat 2's row, pressed: picked, and its sight asked for once.
    Assert::IsFalse(screen.TakeSight().has_value(), L"nothing picked yet");
    const auto seatTwo = [](const Neuron::CanvasRegion& _region) { return _region.payload == ((3u << 16) | 2); };
    screen.Input(PressOn(Lay(screen, model, NOW), seatTwo));
    Assert::AreEqual(std::uint32_t{2}, screen.TakeSight().value_or(Outpost::PlayerId{}).value);
    Assert::IsFalse(screen.TakeSight().has_value(), L"asked once");
    const Neuron::DrawList& picked = Lay(screen, model, NOW);
    Assert::IsTrue(Same(RowFaceOf(picked, 2).color, Outpost::Hud::Theme().pickedColor), L"in the picked card's face");
    screen.Input(PressOn(picked, seatTwo));
    const std::optional<Outpost::PlayerId> full = screen.TakeSight();
    Assert::IsTrue(full.has_value() && !full.value_or(Outpost::PlayerId{2}).IsValid(), L"full sight again");
    Assert::IsFalse(screen.Picked().has_value());
  }

  // ADR-103: each seat's row is marked in its color, its set's or the overseer's palette's, none the warning's.
  TEST_METHOD(DrawsEachSeatInItsColor)
  {
    OverseerScreen screen({{.player = Outpost::PlayerId{1}, .color = BLUE}});
    Assert::IsTrue(Same(screen.SeatColor(Outpost::PlayerId{1}), BLUE), L"its set's");
    Assert::IsTrue(Same(screen.SeatColor(Outpost::PlayerId{3}), Outpost::OVERSEER_SEAT_PALETTE[0]));
    Assert::IsTrue(Same(screen.SeatColor(Outpost::PlayerId{4}), Outpost::OVERSEER_SEAT_PALETTE[1]));
    for (const DirectX::XMFLOAT4& color : Outpost::OVERSEER_SEAT_PALETTE)
      Assert::IsTrue(Neuron::Canvas::MayFill(Outpost::Hud::Theme(), color), L"never the warning's");
    const Scripted scripted;
    const Neuron::DrawList& layout = Lay(screen, scripted.model, NOW);
    const auto marked = [&layout](const DirectX::XMFLOAT4& _color)
    { return std::ranges::any_of(layout.panels, [&_color](const Neuron::CanvasRect& _panel) { return Same(_panel.color, _color); }); };
    Assert::IsTrue(marked(BLUE) && marked(Outpost::OVERSEER_SEAT_PALETTE[0]) && marked(Outpost::OVERSEER_SEAT_PALETTE[1]),
                   L"each row's mark");
  }

  // ADR-103: the log shows its records the newest first, filtered by kind.
  TEST_METHOD(ShowsTheLogNewestFirst)
  {
    const Scripted scripted;
    const Outpost::OverseerModel& model = scripted.model;
    OverseerScreen screen({});
    const Neuron::DrawList& all = Lay(screen, model, NOW);
    Assert::IsTrue(IndexOf(all, "battle 71500 71900 sector 4 player 1 present player 2 ai") < IndexOf(all, "start 0 new"), L"newest first");
    screen.FilterLog(OverseerScreen::LogFilter::Battles);
    const Neuron::DrawList& battles = Lay(screen, model, NOW);
    Assert::IsTrue(Shows(battles, "battle 71500 71900 sector 4 player 1 present player 2 ai"));
    Assert::IsFalse(Shows(battles, "start 0 new"));
    screen.FilterLog(OverseerScreen::LogFilter::Saves);
    const Neuron::DrawList& saves = Lay(screen, model, NOW);
    Assert::IsTrue(Shows(saves, "start 0 new") && Shows(saves, "save 70000 bytes 12700 encode_us 310"));
    Assert::IsFalse(Shows(saves, "seat 71000 player 3 deputy"));
  }

  // ADR-061 decision 5, ADR-062 decision 3: at 1920×1080 and 1280×720, every text of the overview ends inside its panel, no two
  // meet, and each stands at 4.5:1, with a seat picked.
  TEST_METHOD(HoldsTheLayoutAndContrastGuards)
  {
    Scripted scripted;
    TellView(scripted.model, Outpost::PlayerId{});
    const Outpost::OverseerModel& model = scripted.model;
    std::wstring offenders;
    for (const auto& [width, height] : std::array<std::pair<std::uint32_t, std::uint32_t>, 2>{{{1920, 1080}, {1280, 720}}})
    {
      OverseerScreen screen({{.player = Outpost::PlayerId{1}, .color = BLUE}});
      screen.Pick(Outpost::PlayerId{1});
      const Neuron::DrawList& layout = Lay(screen, model, NOW, Link(), width, height);
      const std::span<const Neuron::GlyphAtlas::Font> fonts = FontsFor(width, height);
      const std::wstring name = std::format(L"the overview at {} wide", width);
      offenders += TextsPastTheirPanels(layout, fonts, name) + TextsThatMeet(layout, fonts, name) + TextsUnderContrast(layout, fonts, name);
    }
    Assert::IsTrue(offenders.empty(), offenders.c_str());
  }

  // ADR-103 decision 7: "All well." over the checks that pass while nothing is to be acted on or known; what is to be acted on
  // as a chip, its words on the warning's tag.
  TEST_METHOD(ShowsWhatNeedsAttention)
  {
    Outpost::OverseerModel model;
    Script(model);
    const Neuron::DrawList quiet = Lay(model, Link(), NOW);
    Assert::IsTrue(Shows(quiet, "All well."), Texts(quiet).c_str());
    Assert::IsTrue(Shows(quiet, "No link wears down."));
    Outpost::HealthMessage low = model.LatestHealth()->health;
    low.tick += 20;
    low.process.diskFreeBytes = std::uint64_t{3} * 1'024 * 1'024 * 1'024;
    model.Take(Outpost::EncodeOverseerMessage(low), NOW);
    const Neuron::DrawList acting = Lay(model, Link(), NOW);
    Assert::IsFalse(Shows(acting, "All well."));
    const Neuron::CanvasText& chip = acting.texts[IndexOf(acting, "The server's disk has 3.0 GB free")];
    Assert::IsTrue(std::ranges::any_of(acting.panels,
                                       [&chip](const Neuron::CanvasRect& _panel) {
                                         return Same(_panel.color, Outpost::Hud::Theme().warningColor) &&
                                                _panel.Contains(chip.left + 1.0f, chip.top + 1.0f);
                                       }),
                   L"on the warning's tag");
  }

  // ADR-103 decision 7: the filter for what needs a look keeps the seats a rule names.
  TEST_METHOD(FiltersTheSeatsThatNeedALook)
  {
    Outpost::OverseerModel model;
    Script(model);
    Outpost::HealthMessage waited = model.LatestHealth()->health;
    waited.tick += 20;
    waited.seats[3].restartTick = waited.tick - (std::uint64_t{20} * 12 * 60);
    model.Take(Outpost::EncodeOverseerMessage(waited), NOW);
    OverseerScreen screen({});
    screen.Filter(OverseerScreen::SeatFilter::NeedsALook);
    const Neuron::DrawList& layout = Lay(screen, model, NOW);
    Assert::IsTrue(SeatsShown(layout) == std::vector<std::string>{"Seat 4"}, Texts(layout).c_str());
    Assert::IsTrue(Shows(layout, "Seat 4 has waited 12:00 for its start to be clear."));
  }

  // ADR-103 decision 7: a lane a player's seat, solid in its color while its player played it and hatched while its deputy did,
  // and the whole day in its present play when no hand-over is told; a recovery's mark across each; a lane of battles, a mark
  // filled with a player present and hollow without; and no lane for an AI empire.
  TEST_METHOD(DrawsWhoPlayed)
  {
    const Scripted scripted;
    OverseerScreen screen({{.player = Outpost::PlayerId{1}, .color = BLUE}});
    const Neuron::DrawList& layout = Lay(screen, scripted.model, NOW);
    Assert::IsTrue(Shows(layout, "SEAT 1") && Shows(layout, "SEAT 3") && Shows(layout, "BATTLES"), Texts(layout).c_str());
    Assert::IsFalse(Shows(layout, "SEAT 2") || Shows(layout, "SEAT 4"), L"no lane for an AI empire");
    // A span of a lane is as tall as the lane, 12 units at this size; the seat table's marks are not.
    const auto drawn = [&layout](const DirectX::XMFLOAT4& _color, Neuron::CanvasFill _fill)
    {
      return std::ranges::count_if(layout.panels, [&](const Neuron::CanvasRect& _panel)
                                   { return Same(_panel.color, _color) && _panel.fill == _fill && _panel.height == 12.0f; });
    };
    Assert::AreEqual(std::ptrdiff_t{1}, drawn(BLUE, Neuron::CanvasFill::Solid), L"seat 1's player, from tick 36,000");
    Assert::AreEqual(std::ptrdiff_t{1}, drawn(BLUE, Neuron::CanvasFill::Hatched), L"seat 1's deputy, before it");
    Assert::AreEqual(std::ptrdiff_t{1}, drawn(Outpost::OVERSEER_SEAT_PALETTE[0], Neuron::CanvasFill::Hatched),
                     L"seat 3's deputy, all day, its hand-over to its deputy at 71,000 going on");
    const Neuron::Theme& theme = Outpost::Hud::Theme();
    const auto marks = [&layout, &theme](float _width, float _height)
    {
      return std::ranges::count_if(layout.panels, [&](const Neuron::CanvasRect& _panel)
                                   { return Same(_panel.color, theme.textColor) && _panel.width == _width && _panel.height == _height; });
    };
    Assert::AreEqual(std::ptrdiff_t{2}, marks(8.0f, 8.0f), L"two battles with a player present, filled");
    Assert::AreEqual(std::ptrdiff_t{2}, marks(8.0f, 1.0f), L"one without, hollow");
    Assert::AreEqual(std::ptrdiff_t{2},
                     std::ranges::count_if(layout.panels, [&theme](const Neuron::CanvasRect& _panel)
                                           { return Same(_panel.color, theme.labelColor) && _panel.width == 2.0f; }),
                     L"the recovery's mark across each player's lane");
  }

  // ADR-103 decision 7: the map draws the view, every seat in the color the seat table marks it with, under a heading that says
  // whose sight it shows; a seat's pick shows once its sight comes, and the All cell gives full sight back.
  TEST_METHOD(DrawsTheMap)
  {
    Scripted scripted;
    OverseerScreen screen({{.player = Outpost::PlayerId{1}, .color = BLUE}});
    const Neuron::DrawList& before = Lay(screen, scripted.model, NOW);
    Assert::IsTrue(Shows(before, "MAP \xC2\xB7 FULL SIGHT") && Shows(before, "No view yet."), Texts(before).c_str());
    Assert::IsNull(MapSquareOf(before), L"no map without a view");

    TellView(scripted.model, Outpost::PlayerId{});
    const Neuron::DrawList& full = Lay(screen, scripted.model, NOW);
    const Neuron::CanvasRect* square = MapSquareOf(full);
    Assert::IsNotNull(square, L"the map's square");
    Assert::IsTrue(MarksIn(full, *square, BLUE) >= 1, L"seat 1's ship in its set's color");
    Assert::IsTrue(MarksIn(full, *square, Outpost::OVERSEER_SEAT_PALETTE[0]) >= 1, L"seat 3's structure in the overseer's");
    Assert::IsFalse(Shows(full, "No view yet."));

    screen.Pick(Outpost::PlayerId{3});
    Assert::IsTrue(screen.TakeSight() == Outpost::PlayerId{3});
    Assert::IsTrue(Shows(Lay(screen, scripted.model, NOW), "MAP \xC2\xB7 FULL SIGHT"), L"what the view shows, until the seat's comes");
    TellView(scripted.model, Outpost::PlayerId{3});
    const Neuron::DrawList& seat = Lay(screen, scripted.model, NOW);
    Assert::IsTrue(Shows(seat, "MAP \xC2\xB7 SEAT 3'S SIGHT"), Texts(seat).c_str());

    constexpr std::uint32_t ALL = 5u << 16;
    screen.Input(PressOn(seat, [](const Neuron::CanvasRegion& _region) { return _region.payload == ALL; }));
    Assert::IsTrue(screen.TakeSight() == Outpost::PlayerId{}, L"full sight again");
    Assert::IsFalse(screen.Picked().has_value());
    screen.Input(PressOn(Lay(screen, scripted.model, NOW), [](const Neuron::CanvasRegion& _region) { return _region.payload == ALL; }));
    Assert::IsFalse(screen.TakeSight().has_value(), L"pressed in full sight, nothing changes");
  }

  TEST_METHOD(WritesUniverseTimeAndBytes)
  {
    Assert::AreEqual(std::string("0:00:00"), Outpost::UniverseTime(0, 20));
    Assert::AreEqual(std::string("5:02:03"), Outpost::UniverseTime(std::uint64_t{20} * ((5 * 3'600) + (2 * 60) + 3), 20));
    Assert::AreEqual(std::string("3 d 04:05:06"),
                     Outpost::UniverseTime(std::uint64_t{20} * ((3 * 86'400) + (4 * 3'600) + (5 * 60) + 6), 20));
    Assert::AreEqual(std::string("512 B"), Outpost::BytesText(512));
    Assert::AreEqual(std::string("12.4 KB"), Outpost::BytesText(12'700));
    Assert::AreEqual(std::string("181 MB"), Outpost::BytesText(190'000'000));
  }
};
} // namespace GameAppTests
