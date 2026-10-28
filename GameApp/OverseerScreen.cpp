#include "pch.h"
#include "OverseerScreen.h"

#include <algorithm>
#include <charconv>
#include <numeric>
#include <ranges>

namespace
{
using Outpost::Hud;
using Outpost::OverseerScreen;

// Everything below in reference units. The screen's margin and the gap between its regions, and a panel's padding.
constexpr float MARGIN = 16.0f;
constexpr float PADDING = 12.0f;
// A line in the title face, in the name face and of a figure, as the HUD's panels set them (ADR-062); a large figure's; a log
// record's line, in the detail face with room between its lines; and a filter's cell.
constexpr float TITLE_LINE_UNITS = 30.0f;
constexpr float NAME_LINE_UNITS = 22.0f;
constexpr float FIGURE_LINE_UNITS = 17.0f;
constexpr float LARGE_FIGURE_LINE_UNITS = 36.0f;
constexpr float DETAIL_LINE_UNITS = 18.0f;
constexpr float CELL_UNITS = 26.0f;
constexpr float LABEL_TRACKING_UNITS = 2.0f;

// The header's columns: the universe and its server at its left, taking what is left; the server's state; and the universe's
// time at its right.
constexpr float STATE_COLUMN_UNITS = 620.0f;
constexpr float TIME_COLUMN_UNITS = 320.0f;
// The tiles, in a row of this many as wide as each other.
constexpr std::size_t TILE_COUNT = 5;
// The column at the overview's right, of who played and the universe log; and the row over the seat table, of what needs
// attention, as tall as leaves the table rows to show.
constexpr float RIGHT_COLUMN_UNITS = 560.0f;
constexpr float TOP_ROW_UNITS = 360.0f;
// Who played: each lane's row, its label's width and the gap after it, the lane's height and how wide it is, what the right
// column leaves it; a battle's mark and a recovery's.
constexpr float LANE_ROW_UNITS = 20.0f;
constexpr float LANE_LABEL_UNITS = 80.0f;
constexpr float LANE_GAP_UNITS = 8.0f;
constexpr float LANE_UNITS = 12.0f;
constexpr float LANE_WIDTH_UNITS = RIGHT_COLUMN_UNITS - (2.0f * PADDING) - LANE_LABEL_UNITS - LANE_GAP_UNITS;
constexpr float BATTLE_MARK_UNITS = 8.0f;
constexpr float START_MARK_UNITS = 2.0f;
// The filters' cells, this far apart, and the gap under them.
constexpr float CELL_GAP_UNITS = 4.0f;
// The map, at the top row's left: its square, as tall as the row leaves under the map's heading, its panel around it, and the
// cell that gives full sight back.
constexpr float MAP_SQUARE_UNITS = TOP_ROW_UNITS - (2.0f * PADDING) - CELL_UNITS - CELL_GAP_UNITS;
constexpr float MAP_PANEL_UNITS = MAP_SQUARE_UNITS + (2.0f * PADDING);
constexpr float ALL_CELL_UNITS = 56.0f;
// The header's buttons.
constexpr float BUTTON_UNITS = 34.0f;
constexpr float HEADER_BUTTON_UNITS = 120.0f;
// The overseer's windows (ADR-103 decision 7): how wide the server's stands, and how far down the screen a window first
// opens, at the screen's right.
constexpr float SERVER_WINDOW_UNITS = 860.0f;
constexpr float SEAT_WINDOW_UNITS = 860.0f;
constexpr float CONFIRM_WINDOW_UNITS = 620.0f;
constexpr float CONFIRM_BUTTON_UNITS = 180.0f;
constexpr float WINDOW_TOP_UNITS = 140.0f;
// The server window's sections: a tick part's name and its figures, either side of its bar; a chart's height and the gap
// between its bars; and how many of the day's long ticks and of the saves it lists.
constexpr float PART_NAME_UNITS = 130.0f;
constexpr float PART_FIGURE_UNITS = 240.0f;
constexpr float PART_BAR_UNITS = 8.0f;
constexpr float CHART_UNITS = 120.0f;
constexpr float CHART_GAP_UNITS = 2.0f;
constexpr std::size_t LONG_TICKS_SHOWN = 10;
constexpr std::size_t SAVES_SHOWN = 10;
// A minute of Health, one a second, which the longest tick of each minute is taken over, and the hour's minutes.
constexpr std::size_t HEALTHS_A_MINUTE = 60;
constexpr std::size_t MINUTES_AN_HOUR = 60;
// The seat window's reach back over its link (ADR-103 decision 7), and the scheduled orders' actions as it names them.
constexpr std::uint64_t LINK_SECONDS = std::uint64_t{4} * 3'600;
constexpr std::array<std::string_view, 6> ACTION_NAMES{"Move", "Attack-move", "Attack", "Hold sector", "Patrol", "Build a rig"};
// What came of the joins, by JoinOutcome, as the server window writes them.
constexpr std::array<std::string_view, Outpost::JOIN_OUTCOME_COUNT> JOIN_OUTCOME_NAMES{
  "welcomed", "another build", "seat refused", "malformed", "silent", "taken", "closed", "lost"};
// How far a notch of the wheel scrolls the table and the log: three name lines.
constexpr float WHEEL_NOTCH_UNITS = 3.0f * NAME_LINE_UNITS;

// The words between two figures of one line.
constexpr std::string_view DOT = " \xC2\xB7 ";

// What a press on the overview names: its kind in the payload's high half, and which in its low half.
enum class PressKind : std::uint32_t
{
  Sort = 1,
  Filter,
  Seat,
  LogFilter,
  FullSight,
  ServerWindow,
  SeatWindow,
  SaveNow,
  Stop,
  Confirm,
  Cancel
};

constexpr std::uint32_t PayloadOf(PressKind _kind, std::uint32_t _which) noexcept
{
  return (static_cast<std::uint32_t>(_kind) << 16) | (_which & 0xFFFF);
}

Neuron::LayoutText Fitted(std::string _text, Hud::Typeface _face, float _lineUnits)
{
  return {.text = std::move(_text), .font = Hud::FontOf(_face), .lineUnits = _lineUnits, .flow = Neuron::TextFlow::Fit};
}

// A label over what follows it, in the label face and tracked, as a window's heading is.
void Heading(Neuron::Canvas& _canvas, std::string _text)
{
  (void)_canvas.Label({.text = std::move(_text),
                       .font = Hud::FontOf(Hud::Typeface::Label),
                       .lineUnits = NAME_LINE_UNITS,
                       .trackingUnits = LABEL_TRACKING_UNITS},
                      Hud::Theme().labelColor);
}

// A panel of the overview, of _style, padded, what it holds stretched across it.
std::size_t BeginPanel(Neuron::Canvas& _canvas, Neuron::LayoutStyle _style = {})
{
  _style.padding = {.left = PADDING, .top = PADDING, .right = PADDING, .bottom = PADDING};
  _style.alignX = Neuron::LayoutAlign::Stretch;
  return _canvas.BeginPanel(_style);
}

std::string MillisecondsText(std::uint32_t _microseconds)
{
  return std::format("{:.2f} ms", static_cast<double>(_microseconds) / 1'000.0);
}

// The universe time between two ticks, as a span: minutes and seconds, with hours once there are any.
std::string SpanText(std::uint64_t _fromTick, std::uint64_t _toTick, std::uint32_t _ticksPerSecond)
{
  const std::uint64_t ticks = _toTick > _fromTick ? _toTick - _fromTick : 0;
  return Outpost::MinutesAndSeconds(_ticksPerSecond > 0 ? ticks / _ticksPerSecond : 0);
}

std::string TickText(std::uint64_t _tick)
{
  return std::format("tick {}", Outpost::WithThousands(static_cast<std::int64_t>(_tick)));
}

// The words of a log record: its kind, then its tick and what follows them.
std::vector<std::string_view> WordsOf(std::string_view _record)
{
  std::vector<std::string_view> words;
  for (const auto word : std::views::split(_record, ' '))
  {
    if (!std::ranges::empty(word))
      words.emplace_back(word.begin(), word.end());
  }
  return words;
}

std::optional<std::uint64_t> NumberOf(std::string_view _word)
{
  std::uint64_t value = 0;
  const auto [end, error] = std::from_chars(_word.data(), _word.data() + _word.size(), value);
  if (_word.empty() || error != std::errc{} || end != _word.data() + _word.size())
    return std::nullopt;
  return value;
}

// The number after _key among _words, if a whole number follows it.
std::optional<std::uint64_t> NumberAfter(std::span<const std::string_view> _words, std::string_view _key)
{
  const auto key = std::ranges::find(_words, _key);
  return key == _words.end() || key + 1 == _words.end() ? std::nullopt : NumberOf(*(key + 1));
}

// The newest save the log tells of (ADR-082 decision 1): its tick, its size and how long encoding it took.
struct SaveRecord
{
  std::uint64_t tick = 0;
  std::uint64_t bytes = 0;
  std::uint64_t encodeMicroseconds = 0;
};

std::optional<SaveRecord> NewestSave(const std::deque<std::string>& _records)
{
  for (auto record = _records.rbegin(); record != _records.rend(); ++record)
  {
    const std::vector<std::string_view> words = WordsOf(*record);
    if (words.size() < 2 || words.front() != "save")
      continue;
    if (const std::optional<std::uint64_t> tick = NumberOf(words[1]))
    {
      return SaveRecord{
        .tick = *tick, .bytes = NumberAfter(words, "bytes").value_or(0), .encodeMicroseconds = NumberAfter(words, "encode_us").value_or(0)};
    }
  }
  return std::nullopt;
}

// Whether a record of kind _kind is one _filter keeps.
bool Keeps(OverseerScreen::LogFilter _filter, std::string_view _kind) noexcept
{
  constexpr std::array<std::string_view, 7> SEATS{"seat", "join", "lost", "restart", "waiting", "hour", "order"};
  constexpr std::array<std::string_view, 3> SAVES{"start", "save", "written"};
  constexpr std::array<std::string_view, 3> SERVER{"tick", "dropped", "overseer"};
  switch (_filter)
  {
  case OverseerScreen::LogFilter::All:
    return true;
  case OverseerScreen::LogFilter::Seats:
    return std::ranges::contains(SEATS, _kind);
  case OverseerScreen::LogFilter::Saves:
    return std::ranges::contains(SAVES, _kind);
  case OverseerScreen::LogFilter::Battles:
    return _kind == "battle";
  case OverseerScreen::LogFilter::Server:
    return std::ranges::contains(SERVER, _kind);
  }
  return false;
}

// The server's state as the header names it (Game §session.overseer), and the lines under it that say why and since when;
// whether the server is up, Running or Stalled, which the latest action's result is told under; and whether it runs, which
// the actions are asked of only then (ADR-104): a stalled server's thread may never carry one out.
struct HeaderState
{
  std::string name;
  std::vector<std::string> lines;
  bool up = false;
  bool running = false;
};

// How long ago _then was at _now, in minutes and seconds.
std::string AgoText(std::chrono::steady_clock::time_point _then, std::chrono::steady_clock::time_point _now)
{
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(_now - _then).count();
  return std::format("{} ago", Outpost::MinutesAndSeconds(static_cast<std::uint64_t>(std::max<std::int64_t>(seconds, 0))));
}

// What the server said as it ended (ADR-101 decision 8): who stopped it, or its failure; when; and the save it comes back from.
std::vector<std::string> EndingOf(const Outpost::ClosingMessage& _closing, std::chrono::steady_clock::time_point _heard,
                                  std::chrono::steady_clock::time_point _now)
{
  const std::string save = Outpost::WithThousands(static_cast<std::int64_t>(_closing.lastSaveTick));
  switch (_closing.ending)
  {
  case Outpost::ServerEnding::StoppedByOwner:
    return {std::format("Stopped by its owner {}.", AgoText(_heard, _now)), std::format("Its last save is of tick {}.", save)};
  case Outpost::ServerEnding::StoppedByWindows:
    return {std::format("Stopped by Windows {}, as it signed out or shut down.", AgoText(_heard, _now)),
            std::format("Its last save is of tick {}.", save)};
  case Outpost::ServerEnding::Failed:
    return {_closing.messageUtf8, std::format("Failed {}; it comes back from the save of tick {}.", AgoText(_heard, _now), save)};
  }
  return {};
}

// What the server came back from as it started (ADR-101 decision 3).
std::string CameBackFrom(const Outpost::OverseerWelcomeMessage& _welcome)
{
  if (!_welcome.recoveredSaveTick.has_value())
    return "It started as a new universe.";
  return std::format("It came back from the save of tick {}, replaying {} commands.",
                     Outpost::WithThousands(static_cast<std::int64_t>(*_welcome.recoveredSaveTick)), _welcome.replayedCommands);
}

HeaderState StateOf(const Outpost::OverseerModel& _model, const Outpost::OverseerLink& _link, std::chrono::steady_clock::time_point _now)
{
  const Outpost::HeardHealth* latest = _model.LatestHealth();
  const std::optional<Outpost::OverseerWelcomeMessage>& welcome = _model.Welcome();
  const std::string joining = std::format("Joining {} every {} s.", _link.server, Outpost::OverseerClient::REJOIN_INTERVAL.count());
  // Stopped or Failed, as the server said, until a newer connection's welcome.
  if (const std::optional<Outpost::ClosingMessage>& closing = _model.Closing())
  {
    HeaderState state{.name = closing->ending == Outpost::ServerEnding::Failed ? "Failed" : "Stopped",
                      .lines = EndingOf(*closing, _model.ClosingAt().value_or(_now), _now)};
    if (!_link.connected)
      state.lines.push_back(joining);
    return state;
  }
  if (_link.connected && welcome.has_value() && latest != nullptr && _now - latest->heard < OverseerScreen::NO_WORD_AFTER)
  {
    // Health comes from the server's main thread and its tick from the server's thread (ADR-025): a tick that stands still
    // while Health keeps coming is a stuck server's thread, from the first Health heard at it to the latest. Health that stops
    // coming is No word's to tell.
    const std::deque<Outpost::HeardHealth>& healths = _model.Healths();
    auto standing = healths.rbegin();
    while (std::next(standing) != healths.rend() && std::next(standing)->health.tick == latest->health.tick)
      ++standing;
    if (latest->heard - standing->heard >= OverseerScreen::STALLED_AFTER)
    {
      const auto stood = std::chrono::duration_cast<std::chrono::seconds>(latest->heard - standing->heard).count();
      return {.name = "Stalled",
              .lines = {std::format("Tick {} has stood for {}; that time goes unrun.",
                                    Outpost::WithThousands(static_cast<std::int64_t>(latest->health.tick)),
                                    Outpost::MinutesAndSeconds(static_cast<std::uint64_t>(stood))),
                        "The server's thread may be stuck; Health still comes."},
              .up = true};
    }
    std::string up = std::format("Up {}", Outpost::MinutesAndSeconds(latest->health.process.uptimeSeconds));
    if (_link.roundTrip.has_value())
      up += std::format("; the overseer's round trip {}", MillisecondsText(static_cast<std::uint32_t>(_link.roundTrip->count())));
    return {.name = "Running", .lines = {up + ".", CameBackFrom(*welcome)}, .up = true, .running = true};
  }
  HeaderState state{.name = "No word", .lines = {}};
  if (const std::optional<std::string>& why = _model.LostWhy())
    state.lines.push_back(std::format("The connection went: {}", *why));
  else if (_link.connected)
    state.lines.push_back(std::format("Connected; no Health for {} s.", OverseerScreen::NO_WORD_AFTER.count()));
  if (!_link.connected)
    state.lines.push_back(joining);
  if (latest != nullptr)
    state.lines.push_back(std::format("The last word was {}, at tick {}.", AgoText(latest->heard, _now), latest->health.tick));
  return state;
}

// An action as the overseer names it (ADR-104).
std::string ActionName(Outpost::OverseerAction _action, Outpost::PlayerId _seat)
{
  switch (_action)
  {
  case Outpost::OverseerAction::SaveNow:
    return "Save now";
  case Outpost::OverseerAction::Stop:
    return "Stop";
  case Outpost::OverseerAction::DropLink:
    return std::format("Drop seat {}'s link", _seat.value);
  case Outpost::OverseerAction::NewToken:
    return std::format("New token for seat {}", _seat.value);
  }
  return {};
}

// The header (Game §session.overseer): the universe's folder, its server and the build it runs; the server's state, and why,
// with what came of the latest action while the server is up, and the buttons for its window and its actions; and universe
// time with its tick.
void LayHeader(Neuron::Canvas& _canvas, const Outpost::OverseerModel& _model, const Outpost::OverseerLink& _link, const HeaderState& _state,
               std::chrono::steady_clock::time_point _now)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)BeginPanel(_canvas);
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = MARGIN});

  (void)tree.Begin(Neuron::LayoutKind::Column, {.grow = 1.0f});
  Heading(_canvas, "UNIVERSE");
  const std::optional<Outpost::OverseerWelcomeMessage>& welcome = _model.Welcome();
  (void)_canvas.Label(Fitted(welcome.has_value() ? welcome->folderUtf8 : std::string("Not yet told"), Hud::Typeface::Name, NAME_LINE_UNITS),
                      theme.textColor);
  (void)_canvas.Label(Fitted(_link.server, Hud::Typeface::Figure, FIGURE_LINE_UNITS), theme.figureColor);
  if (welcome.has_value())
  {
    (void)_canvas.Label(Fitted(std::format("wire {:016x}{}protocol {}{}state {}", welcome->wireLayoutHash, DOT, welcome->protocolVersion,
                                           DOT, welcome->stateVersion),
                               Hud::Typeface::Figure, FIGURE_LINE_UNITS),
                        theme.textColor);
  }
  tree.End();

  (void)tree.Begin(Neuron::LayoutKind::Column, {.widthUnits = STATE_COLUMN_UNITS});
  Heading(_canvas, "SERVER");
  // The state, the button that opens the server window, and the server's actions beside it (owner, 2026-10-28).
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = MARGIN, .alignY = Neuron::LayoutAlign::Center});
  (void)_canvas.Label(Fitted(_state.name, Hud::Typeface::Title, TITLE_LINE_UNITS), theme.textColor, {.grow = 1.0f});
  const Neuron::LayoutStyle button{.widthUnits = HEADER_BUTTON_UNITS, .heightUnits = BUTTON_UNITS};
  (void)_canvas.Button("server button", {.label = "Server", .payload = PayloadOf(PressKind::ServerWindow, 0)}, button);
  (void)_canvas.Button("save now button", {.label = "Save now", .payload = PayloadOf(PressKind::SaveNow, 0), .enabled = _state.running},
                       button);
  (void)_canvas.Button("stop button", {.label = "Stop", .payload = PayloadOf(PressKind::Stop, 0), .enabled = _state.running}, button);
  tree.End();
  for (const std::string& line : _state.lines)
    (void)_canvas.Label(Fitted(line, Hud::Typeface::Name, NAME_LINE_UNITS), theme.dimTextColor);
  // What came of the latest action, wrapped, since the server's words may name a path, and a refusal in the warning's color as
  // a refused text is (ADR-085 decision 1).
  if (const std::optional<Outpost::HeardResult>& heard = _model.LatestResult(); _state.up && heard.has_value())
  {
    (void)_canvas.Label({.text = std::format("{}, {}: {}", ActionName(heard->result.action, heard->result.seat),
                                             AgoText(heard->heard, _now), heard->result.wordsUtf8),
                         .font = Hud::FontOf(Hud::Typeface::Name),
                         .lineUnits = NAME_LINE_UNITS,
                         .flow = Neuron::TextFlow::Wrap},
                        heard->result.done ? theme.textColor : theme.warningColor);
  }
  tree.End();

  (void)tree.Begin(Neuron::LayoutKind::Column, {.widthUnits = TIME_COLUMN_UNITS});
  Heading(_canvas, "UNIVERSE TIME");
  const Outpost::HeardHealth* latest = _model.LatestHealth();
  const bool timed = welcome.has_value() && latest != nullptr;
  (void)_canvas.Label(Fitted(timed ? Outpost::UniverseTime(latest->health.tick, welcome->ticksPerSecond) : std::string("-"),
                             Hud::Typeface::LargeFigure, LARGE_FIGURE_LINE_UNITS),
                      theme.figureColor);
  (void)_canvas.Label(Fitted(timed ? TickText(latest->health.tick) : std::string(), Hud::Typeface::Figure, FIGURE_LINE_UNITS),
                      theme.textColor);
  tree.End();

  tree.End();
  _canvas.EndPanel();
}

// One tile: its label, its figure and a detail line under it.
struct Tile
{
  std::string label;
  std::string figure;
  std::string detail;
};

// The tiles (Game §session.overseer): the tick against its budget, the ticks a second, the last save, the bytes out to the
// seats, and the process's memory, CPU and disk.
std::array<Tile, TILE_COUNT> TilesOf(const Outpost::OverseerModel& _model)
{
  const std::deque<Outpost::HeardHealth>& healths = _model.Healths();
  std::array<Tile, TILE_COUNT> tiles{{{.label = "TICK, 99TH PERCENTILE", .figure = "-", .detail = "no Health yet"},
                                      {.label = "TICKS A SECOND", .figure = "-", .detail = {}},
                                      {.label = "LAST SAVE", .figure = "-", .detail = "none in the log yet"},
                                      {.label = "BYTES OUT", .figure = "-", .detail = {}},
                                      {.label = "PROCESS", .figure = "-", .detail = {}}}};
  const Outpost::HeardHealth* latest = _model.LatestHealth();
  const std::optional<Outpost::OverseerWelcomeMessage>& welcome = _model.Welcome();
  if (latest != nullptr)
  {
    // The last minute's ticks at the nearest rank, as the universe log takes a percentile, and the hour's longest.
    std::vector<std::uint32_t> minute;
    std::uint32_t longest = 0;
    std::uint64_t dropped = 0;
    const std::size_t minuteFrom =
      healths.size() > OverseerScreen::MINUTE_OF_HEALTH ? healths.size() - OverseerScreen::MINUTE_OF_HEALTH : 0;
    for (std::size_t index = 0; index < healths.size(); ++index)
    {
      const Outpost::HealthMessage& health = healths[index].health;
      dropped += health.droppedTicks;
      for (const std::uint32_t tick : health.tickMicroseconds)
      {
        longest = std::max(longest, tick);
        if (index >= minuteFrom)
          minute.push_back(tick);
      }
    }
    if (!minute.empty())
    {
      std::ranges::sort(minute);
      const std::size_t rank = std::max<std::size_t>(1, ((minute.size() * 99) + 99) / 100);
      tiles[0].figure = MillisecondsText(minute[rank - 1]);
      tiles[0].detail =
        std::format("of {} ms; the hour's longest {}", OverseerScreen::TICK_BUDGET_MICROSECONDS / 1'000, MillisecondsText(longest));
    }
    tiles[1].figure = std::format("{}", latest->health.tickMicroseconds.size());
    tiles[1].detail = dropped == 0 ? std::string("none dropped this hour") : std::format("{} dropped this hour", dropped);

    // Each connection's bytes since the Health before, as a rate.
    if (healths.size() >= 2)
    {
      const Outpost::HeardHealth& before = healths[healths.size() - 2];
      const double seconds = std::chrono::duration<double>(latest->heard - before.heard).count();
      std::uint64_t bytes = 0;
      std::size_t connections = 0;
      for (const Outpost::SeatHealth& seat : latest->health.seats)
      {
        const auto earlier = std::ranges::find(before.health.seats, seat.player, &Outpost::SeatHealth::player);
        if (!seat.connection.has_value() || earlier == before.health.seats.end() || !earlier->connection.has_value())
          continue;
        ++connections;
        const std::uint64_t sent = seat.connection.value_or(Outpost::LinkFigures{}).bytesSent;
        const std::uint64_t sentBefore = earlier->connection.value_or(Outpost::LinkFigures{}).bytesSent;
        if (sent >= sentBefore)
          bytes += sent - sentBefore;
      }
      if (seconds > 0.0)
      {
        tiles[3].figure = std::format("{}/s", Outpost::BytesText(static_cast<std::uint64_t>(static_cast<double>(bytes) / seconds)));
        tiles[3].detail = std::format("to {} {}", connections, connections == 1 ? "seat" : "seats");
      }
    }
    // Its working set, the share of a core it took over the last second, and the disk free under the universe's folder.
    const Outpost::ProcessHealth& process = latest->health.process;
    tiles[4].figure = Outpost::BytesText(process.workingSetBytes);
    tiles[4].detail = std::format("{}% of a core{}{} free", process.cpuHundredths, DOT, Outpost::BytesText(process.diskFreeBytes));
  }
  if (const std::optional<SaveRecord> save = NewestSave(_model.Records()))
  {
    if (latest != nullptr && welcome.has_value())
      tiles[2].figure = std::format("{} ago", SpanText(save->tick, latest->health.tick, welcome->ticksPerSecond));
    tiles[2].detail = std::format("{}{}{}{}{}", TickText(save->tick), DOT, Outpost::BytesText(save->bytes), DOT,
                                  MillisecondsText(static_cast<std::uint32_t>(save->encodeMicroseconds)));
  }
  return tiles;
}

void LayTiles(Neuron::Canvas& _canvas, const Outpost::OverseerModel& _model)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)tree.Begin(Neuron::LayoutKind::Grid, {.gapUnits = MARGIN, .alignX = Neuron::LayoutAlign::Stretch, .columns = TILE_COUNT});
  for (const Tile& tile : TilesOf(_model))
  {
    (void)BeginPanel(_canvas);
    Heading(_canvas, tile.label);
    (void)_canvas.Label(Fitted(tile.figure, Hud::Typeface::LargeFigure, LARGE_FIGURE_LINE_UNITS), theme.figureColor);
    (void)_canvas.Label(Fitted(tile.detail, Hud::Typeface::Name, NAME_LINE_UNITS), theme.dimTextColor);
    _canvas.EndPanel();
  }
  tree.End();
}

// A seat's status (Game §session.overseer): online and for how long, open, its link gone and when its deputy takes it, away
// and for how long, fallen and when it restarts, or an AI empire playing; and its rank among them, which it is sorted by.
struct SeatStatus
{
  std::string name;
  std::string detail;
  std::int64_t rank = 0;
};

SeatStatus StatusOf(const Outpost::SeatHealth& _seat, std::uint64_t _tick, std::uint32_t _ticksPerSecond)
{
  if (_seat.restartTick.has_value())
  {
    const std::uint64_t restart = *_seat.restartTick;
    return {.name = "Fallen",
            .detail = restart > _tick ? std::format("restarts in {}", SpanText(_tick, restart, _ticksPerSecond))
                                      : std::format("waiting {} for its start", SpanText(restart, _tick, _ticksPerSecond)),
            .rank = 4};
  }
  switch (_seat.link)
  {
  case Outpost::SeatLink::None:
    return {.name = "Playing", .detail = "an AI empire", .rank = 5};
  case Outpost::SeatLink::Online:
    return {.name = "Online", .detail = std::format("for {}", SpanText(_seat.linkTick, _tick, _ticksPerSecond)), .rank = 0};
  case Outpost::SeatLink::Open:
    return {.name = "Open", .detail = _seat.play == Outpost::SeatPlay::Deputy ? "its deputy plays" : "never joined", .rank = 1};
  case Outpost::SeatLink::Gone:
    if (_seat.deputyTick.has_value() && *_seat.deputyTick > _tick)
      return {.name = "Link gone", .detail = std::format("deputy in {}", SpanText(_tick, *_seat.deputyTick, _ticksPerSecond)), .rank = 2};
    return {.name = "Away", .detail = std::format("for {}", SpanText(_seat.linkTick, _tick, _ticksPerSecond)), .rank = 3};
  }
  return {};
}

// Who plays a seat: its player, its deputy, or the AI at its difficulty.
std::string WhoPlays(const Outpost::SeatHealth& _seat, std::string_view _difficulty)
{
  switch (_seat.play)
  {
  case Outpost::SeatPlay::Player:
    return "player";
  case Outpost::SeatPlay::Deputy:
    return "deputy";
  case Outpost::SeatPlay::Ai:
    return _difficulty.empty() ? std::string("AI") : std::format("AI, {}", _difficulty);
  }
  return {};
}

// A seat as the table shows it, and what it is sorted and filtered by.
struct SeatLine
{
  Outpost::PlayerId player;
  bool ai = false;
  bool away = false;
  bool needsALook = false;
  Neuron::TableRow row;
  std::array<std::int64_t, OverseerScreen::SEAT_COLUMN_COUNT> keys{};
};

std::vector<SeatLine> SeatLinesOf(const Outpost::OverseerModel& _model, const OverseerScreen& _screen,
                                  std::span<const Outpost::Attention> _attention)
{
  const Outpost::HeardHealth* latest = _model.LatestHealth();
  const std::optional<Outpost::OverseerWelcomeMessage>& welcome = _model.Welcome();
  std::vector<SeatLine> lines;
  if (latest == nullptr || !welcome.has_value())
    return lines;
  const std::uint64_t tick = latest->health.tick;
  const std::uint32_t perSecond = welcome->ticksPerSecond;
  const std::deque<Outpost::HeardHealth>& healths = _model.Healths();
  const std::size_t minuteFrom = healths.size() > OverseerScreen::MINUTE_OF_HEALTH ? healths.size() - OverseerScreen::MINUTE_OF_HEALTH : 0;
  for (const Outpost::SeatHealth& seat : latest->health.seats)
  {
    const auto known = std::ranges::find(welcome->seats, seat.player, &Outpost::OverseerSeat::player);
    const std::string difficulty = known != welcome->seats.end() ? known->ai : std::string();
    // Its commands over the last minute.
    std::uint64_t orders = 0;
    std::uint64_t refused = 0;
    for (std::size_t index = minuteFrom; index < healths.size(); ++index)
    {
      const std::vector<Outpost::SeatHealth>& seats = healths[index].health.seats;
      if (const auto found = std::ranges::find(seats, seat.player, &Outpost::SeatHealth::player); found != seats.end())
      {
        orders += found->orders;
        refused += found->refused;
      }
    }
    const SeatStatus status = StatusOf(seat, tick, perSecond);
    Neuron::TableCell link{.figure = "-", .detail = "no connection", .color = {}};
    std::int64_t roundTrip = std::numeric_limits<std::int64_t>::max();
    if (seat.connection.has_value())
    {
      const Outpost::LinkFigures& figures = *seat.connection;
      const double loss =
        figures.packetsSent > 0 ? 100.0 * static_cast<double>(figures.packetsLost) / static_cast<double>(figures.packetsSent) : 0.0;
      link = {.figure = MillisecondsText(figures.roundTripMicroseconds),
              .detail = std::format("loss {:.1f}%{}backlog {}", loss, DOT, Outpost::BytesText(figures.backlogBytes)),
              .color = {}};
      roundTrip = figures.roundTripMicroseconds;
    }
    const bool ai = seat.play == Outpost::SeatPlay::Ai || !difficulty.empty();
    lines.push_back(
      {.player = seat.player,
       .ai = ai,
       .away = !ai && (seat.link != Outpost::SeatLink::Online || seat.play == Outpost::SeatPlay::Deputy),
       .needsALook = std::ranges::any_of(_attention, [&seat](const Outpost::Attention& _found)
                                         { return _found.seat == seat.player && _found.level != Outpost::AttentionLevel::Pass; }),
       .row = {.cells = {{.figure = std::format("Seat {}", seat.player.value), .detail = WhoPlays(seat, difficulty), .color = {}},
                         {.figure = status.name, .detail = status.detail, .color = {}},
                         link,
                         {.figure = std::format("{} a minute", orders), .detail = std::format("{} refused", refused), .color = {}},
                         {.figure = Outpost::WithThousands(seat.ore),
                          .detail = std::format("{:+.2f} a second", static_cast<double>(seat.incomeHundredthsPerSecond) / 100.0),
                          .color = {}},
                         {.figure = std::format("{} / {}", seat.commandPoints, seat.fleetCap), .detail = "command points", .color = {}},
                         {.figure = std::format("{}", seat.nodes), .detail = {}, .color = {}},
                         {.figure = std::format("{}", seat.researched), .detail = {}, .color = {}}},
               .payload = PayloadOf(PressKind::Seat, seat.player.value),
               .picked = _screen.Picked() == seat.player,
               .mark = _screen.SeatColor(seat.player)},
       .keys = {seat.player.value, status.rank, roundTrip, static_cast<std::int64_t>(orders), seat.ore, seat.commandPoints, seat.nodes,
                seat.researched}});
  }
  return lines;
}

constexpr std::array<std::string_view, OverseerScreen::SEAT_COLUMN_COUNT> SEAT_HEADINGS{"SEAT", "STATUS", "ROUND TRIP", "ORDERS",
                                                                                        "ORE",  "FLEET",  "NODES",      "RESEARCH"};
// Each column's width, or none for the one that takes what the others leave.
constexpr std::array<std::optional<float>, OverseerScreen::SEAT_COLUMN_COUNT> SEAT_WIDTHS{150.0f, 220.0f, std::nullopt, 150.0f,
                                                                                          160.0f, 140.0f, 100.0f,       110.0f};
constexpr std::array<std::string_view, 5> SEAT_FILTERS{"All", "Players", "AI", "Away", "Needs a look"};
constexpr std::array<std::string_view, 5> LOG_FILTERS{"All", "Seats", "Saves", "Battles", "Server"};

// A row of filter cells, _picked lit, each naming _kind and its index.
template <std::size_t Count>
void FilterCells(Neuron::Canvas& _canvas, std::string_view _id, const std::array<std::string_view, Count>& _words, std::size_t _picked,
                 PressKind _kind)
{
  std::array<Neuron::LayoutText, Count> words;
  std::array<std::uint32_t, Count> payloads{};
  for (std::size_t index = 0; index < Count; ++index)
  {
    words[index] = {.text = std::string(_words[index]), .font = Hud::FontOf(Hud::Typeface::Label), .lineUnits = DETAIL_LINE_UNITS};
    payloads[index] = PayloadOf(_kind, static_cast<std::uint32_t>(index));
  }
  _canvas.Cells(_id, words, payloads, _picked, CELL_UNITS, {.gapUnits = CELL_GAP_UNITS});
}

// What needs attention (ADR-103 decision 7): a chip for each thing to act on, a line for each to know, and the checks that
// pass under them, the list headed "All well." when nothing is to be acted on or known; scrolled when it runs past its panel.
void LayAttention(Neuron::Canvas& _canvas, std::span<const Outpost::Attention> _attention, bool _told)
{
  const Neuron::Theme& theme = Hud::Theme();
  (void)BeginPanel(_canvas, {.grow = 1.0f});
  Heading(_canvas, "NEEDS ATTENTION");
  (void)_canvas.BeginScroll("attention", {.grow = 1.0f});
  const bool quiet =
    std::ranges::all_of(_attention, [](const Outpost::Attention& _found) { return _found.level == Outpost::AttentionLevel::Pass; });
  if (!_told)
    (void)_canvas.Label(Fitted("Nothing is told until the server says how it is.", Hud::Typeface::Name, NAME_LINE_UNITS),
                        theme.dimTextColor);
  else if (quiet)
    (void)_canvas.Label(Fitted("All well.", Hud::Typeface::Name, NAME_LINE_UNITS), theme.textColor);
  for (const Outpost::Attention& found : _attention)
  {
    switch (found.level)
    {
    case Outpost::AttentionLevel::Act:
      (void)_canvas.Chip(Fitted(found.words, Hud::Typeface::Name, NAME_LINE_UNITS));
      (void)_canvas.Tree().Add({.heightUnits = CELL_GAP_UNITS});
      break;
    case Outpost::AttentionLevel::Know:
      (void)_canvas.Label(Fitted(found.words, Hud::Typeface::Name, NAME_LINE_UNITS), theme.textColor);
      break;
    case Outpost::AttentionLevel::Pass:
      (void)_canvas.Label(Fitted(found.words, Hud::Typeface::Name, NAME_LINE_UNITS), theme.dimTextColor);
      break;
    }
  }
  _canvas.EndScroll();
  _canvas.EndPanel();
}

// The map (ADR-103 decision 7): the heading says whose sight the view shows, full sight or a seat's, beside a cell that gives
// full sight back, lit while no seat is picked; under them the square the minimap's drawing goes into once the canvas has
// placed it, which this returns, none while there is no view to draw.
std::size_t LayMap(Neuron::Canvas& _canvas, const Outpost::OverseerModel& _model, const OverseerScreen& _screen)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  const std::optional<Outpost::Snapshot>& view = _model.View();
  (void)BeginPanel(_canvas, {.widthUnits = MAP_PANEL_UNITS});
  (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = CELL_UNITS, .gapUnits = CELL_GAP_UNITS, .alignY = Neuron::LayoutAlign::Center});
  const std::string sight =
    view.has_value() && view->player.IsValid() ? std::format("SEAT {}'S SIGHT", view->player.value) : std::string("FULL SIGHT");
  (void)_canvas.Label({.text = std::format("MAP{}{}", DOT, sight),
                       .font = Hud::FontOf(Hud::Typeface::Label),
                       .lineUnits = NAME_LINE_UNITS,
                       .trackingUnits = LABEL_TRACKING_UNITS,
                       .flow = Neuron::TextFlow::Fit},
                      theme.labelColor, {.grow = 1.0f});
  const std::array<Neuron::LayoutText, 1> all{
    Neuron::LayoutText{.text = "All", .font = Hud::FontOf(Hud::Typeface::Label), .lineUnits = DETAIL_LINE_UNITS}};
  const std::array<std::uint32_t, 1> payload{PayloadOf(PressKind::FullSight, 0)};
  _canvas.Cells("full sight", all, payload, _screen.Picked().has_value() ? std::nullopt : std::optional<std::size_t>{0}, CELL_UNITS,
                {.widthUnits = ALL_CELL_UNITS});
  tree.End();
  (void)tree.Add({.heightUnits = CELL_GAP_UNITS});
  std::size_t square = Neuron::LayoutNode::NO_NODE;
  if (view.has_value() && view->mapSizeMeters > 0.0f)
    square = tree.Add({.widthUnits = MAP_SQUARE_UNITS, .heightUnits = MAP_SQUARE_UNITS});
  else
    (void)_canvas.Label(Fitted("No view yet.", Hud::Typeface::Name, NAME_LINE_UNITS), theme.dimTextColor);
  _canvas.EndPanel();
  return square;
}

// What the map draws of _view: the minimap's content, every seat in the color the seat table marks it with, and no fog, since
// a seat's own snapshot holds only what the seat sees, and full sight sees through it.
Hud::Content MapContentOf(const Outpost::Snapshot& _view, const Outpost::OverseerWelcomeMessage& _welcome, const OverseerScreen& _screen)
{
  Hud::Content content = Hud::Describe(_view, _view.entities, {});
  content.sideColors.clear();
  content.sideColors.reserve(_welcome.seats.size());
  for (const Outpost::OverseerSeat& seat : _welcome.seats)
    content.sideColors.push_back({.player = seat.player, .color = _screen.SeatColor(seat.player)});
  content.fog = false;
  return content;
}

// Who played a seat in a span of its lane, from one tick to another.
struct LaneSpan
{
  std::uint64_t from = 0;
  std::uint64_t to = 0;
  Outpost::SeatPlay play = Outpost::SeatPlay::Player;
};

std::optional<Outpost::SeatPlay> PlayOf(std::string_view _word) noexcept
{
  if (_word == "player")
    return Outpost::SeatPlay::Player;
  if (_word == "deputy")
    return Outpost::SeatPlay::Deputy;
  if (_word == "ai")
    return Outpost::SeatPlay::Ai;
  return std::nullopt;
}

// _seat's lane over the day from _dayStart to _dayEnd, from its hand-overs among _records, "seat <tick> player <n> <play>",
// the last before the day saying who played it as the day began; with none, the whole day in _now's play.
std::vector<LaneSpan> LaneOf(Outpost::PlayerId _seat, const std::deque<std::string>& _records, std::uint64_t _dayStart,
                             std::uint64_t _dayEnd, std::optional<Outpost::SeatPlay> _now)
{
  std::vector<LaneSpan> spans;
  std::optional<Outpost::SeatPlay> play;
  std::uint64_t from = _dayStart;
  bool told = false;
  for (const std::string& record : _records)
  {
    const std::vector<std::string_view> words = WordsOf(record);
    if (words.size() < 5 || words[0] != "seat" || words[2] != "player" || NumberOf(words[3]) != std::uint64_t{_seat.value})
      continue;
    const std::optional<std::uint64_t> tick = NumberOf(words[1]);
    const std::optional<Outpost::SeatPlay> handedTo = PlayOf(words[4]);
    if (!tick.has_value() || !handedTo.has_value())
      continue;
    told = true;
    if (handedTo == play)
      continue;
    const std::uint64_t at = std::clamp(*tick, _dayStart, _dayEnd);
    if (play.has_value() && at > from)
      spans.push_back({.from = from, .to = at, .play = *play});
    from = std::max(from, at);
    play = handedTo;
  }
  if (!told)
    play = _now;
  if (play.has_value() && _dayEnd > from)
    spans.push_back({.from = from, .to = _dayEnd, .play = *play});
  return spans;
}

// A box of _color at _leftUnits across a lane, _widthUnits by _heightUnits, in its middle down.
void LaneBox(Neuron::Canvas& _canvas, float _leftUnits, float _widthUnits, float _heightUnits, const DirectX::XMFLOAT4& _color,
             Neuron::CanvasFill _fill = Neuron::CanvasFill::Solid)
{
  const std::size_t box = _canvas.Tree().Add(
    {.widthUnits = std::max(_widthUnits, 1.0f),
     .heightUnits = _heightUnits,
     .anchor = Neuron::LayoutAnchor::TopLeft,
     .margin = {.left = std::clamp(_leftUnits, 0.0f, LANE_WIDTH_UNITS - 1.0f), .top = (LANE_UNITS - _heightUnits) / 2.0f}});
  _canvas.Fill(box, _color, _fill);
}

// A lane's row: its label, then the lane, its track filled, which what follows is drawn into until EndLane.
void BeginLane(Neuron::Canvas& _canvas, std::string _label)
{
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)tree.Begin(Neuron::LayoutKind::Row,
                   {.heightUnits = LANE_ROW_UNITS, .gapUnits = LANE_GAP_UNITS, .alignY = Neuron::LayoutAlign::Center});
  (void)_canvas.Label(Fitted(std::move(_label), Hud::Typeface::Label, FIGURE_LINE_UNITS), Hud::Theme().labelColor,
                      {.widthUnits = LANE_LABEL_UNITS});
  (void)tree.Begin(Neuron::LayoutKind::Stack, {.widthUnits = LANE_WIDTH_UNITS, .heightUnits = LANE_UNITS});
  _canvas.Fill(tree.Add({.widthUnits = LANE_WIDTH_UNITS, .heightUnits = LANE_UNITS}), Hud::Theme().barTrackColor);
}

void EndLane(Neuron::Canvas& _canvas)
{
  _canvas.Tree().End();
  _canvas.Tree().End();
}

// Who played (ADR-103 decision 7): a lane a player's seat over the day of universe time that ends at the newest tick, solid
// in its color while its player played it and hatched while its deputy did, each recovery a mark across it; and a lane of
// battles, each mark filled when a player was present and hollow when only deputies and AI fought. AI empires have no lane.
void LayWhoPlayed(Neuron::Canvas& _canvas, const Outpost::OverseerModel& _model, const OverseerScreen& _screen)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)BeginPanel(_canvas);
  Heading(_canvas, "WHO PLAYED, THE LAST DAY");
  const std::optional<Outpost::OverseerWelcomeMessage>& welcome = _model.Welcome();
  const Outpost::HeardHealth* latest = _model.LatestHealth();
  const std::uint64_t dayEnd = _model.NewestTick();
  const std::uint64_t day = Outpost::DAY_RECORDS_SECONDS * (welcome.has_value() ? welcome->ticksPerSecond : 0);
  const std::uint64_t dayStart = dayEnd > day ? dayEnd - day : 0;
  if (!welcome.has_value() || dayEnd <= dayStart)
  {
    (void)_canvas.Label(Fitted("Nothing is told yet.", Hud::Typeface::Name, NAME_LINE_UNITS), theme.dimTextColor);
    _canvas.EndPanel();
    return;
  }
  const auto across = [dayStart, dayEnd](std::uint64_t _tick)
  { return LANE_WIDTH_UNITS * static_cast<float>(std::clamp(_tick, dayStart, dayEnd) - dayStart) / static_cast<float>(dayEnd - dayStart); };
  const std::deque<std::string>& records = _model.DayRecords();
  // The recoveries of the day: a new universe's start is where its lanes begin, not a mark across them.
  std::vector<std::uint64_t> starts;
  for (const std::string& record : records)
  {
    if (const std::optional<Outpost::RecordHead> head = Outpost::HeadOf(record);
        head.has_value() && head->kind == "start" && head->tick >= dayStart && record.contains(" recovered "))
      starts.push_back(head->tick);
  }
  for (const Outpost::OverseerSeat& seat : welcome->seats)
  {
    if (!seat.ai.empty())
      continue;
    std::optional<Outpost::SeatPlay> now;
    if (latest != nullptr)
    {
      const std::vector<Outpost::SeatHealth>& seats = latest->health.seats;
      if (const auto found = std::ranges::find(seats, seat.player, &Outpost::SeatHealth::player); found != seats.end())
        now = found->play;
    }
    BeginLane(_canvas, std::format("SEAT {}", seat.player.value));
    const DirectX::XMFLOAT4 color = _screen.SeatColor(seat.player);
    for (const LaneSpan& span : LaneOf(seat.player, records, dayStart, dayEnd, now))
    {
      if (span.play == Outpost::SeatPlay::Ai)
        continue;
      LaneBox(_canvas, across(span.from), across(span.to) - across(span.from), LANE_UNITS, color,
              span.play == Outpost::SeatPlay::Player ? Neuron::CanvasFill::Solid : Neuron::CanvasFill::Hatched);
    }
    for (const std::uint64_t start : starts)
      LaneBox(_canvas, across(start), START_MARK_UNITS, LANE_UNITS, theme.labelColor);
    EndLane(_canvas);
  }
  BeginLane(_canvas, "BATTLES");
  for (const std::string& record : records)
  {
    const std::optional<Outpost::RecordHead> head = Outpost::HeadOf(record);
    if (!head.has_value() || head->kind != "battle" || head->tick < dayStart)
      continue;
    const float left = across(head->tick) - (BATTLE_MARK_UNITS / 2.0f);
    if (record.contains(" present"))
      LaneBox(_canvas, left, BATTLE_MARK_UNITS, BATTLE_MARK_UNITS, theme.textColor);
    else
    {
      // Hollow: its four sides.
      LaneBox(_canvas, left, BATTLE_MARK_UNITS, 1.0f, theme.textColor);
      LaneBox(_canvas, left, 1.0f, BATTLE_MARK_UNITS, theme.textColor);
      LaneBox(_canvas, left + BATTLE_MARK_UNITS - 1.0f, 1.0f, BATTLE_MARK_UNITS, theme.textColor);
      const std::size_t foot = tree.Add(
        {.widthUnits = BATTLE_MARK_UNITS,
         .heightUnits = 1.0f,
         .anchor = Neuron::LayoutAnchor::TopLeft,
         .margin = {.left = std::clamp(left, 0.0f, LANE_WIDTH_UNITS - 1.0f), .top = ((LANE_UNITS + BATTLE_MARK_UNITS) / 2.0f) - 1.0f}});
      _canvas.Fill(foot, theme.textColor);
    }
  }
  EndLane(_canvas);
  // The day's two ends, under the lanes.
  (void)tree.Begin(Neuron::LayoutKind::Row, {.padding = {.left = LANE_LABEL_UNITS + LANE_GAP_UNITS}});
  (void)_canvas.Label(
    Fitted(std::format("tick {}", Outpost::WithThousands(static_cast<std::int64_t>(dayStart))), Hud::Typeface::Detail, DETAIL_LINE_UNITS),
    theme.dimTextColor);
  (void)tree.AddSpacer();
  (void)_canvas.Label(Fitted("now", Hud::Typeface::Detail, DETAIL_LINE_UNITS), theme.dimTextColor);
  tree.End();
  _canvas.EndPanel();
}
// A chart of _values, a bar each in a track of _slots, the newest at its right, each as tall as its value against _most, and a
// line across at _mark, such as a budget (ADR-103 decision 7): composites of fills, as who played's lanes are.
void BarChart(Neuron::Canvas& _canvas, std::span<const std::uint32_t> _values, std::size_t _slots, std::uint32_t _most, std::uint32_t _mark,
              float _widthUnits)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)tree.Begin(Neuron::LayoutKind::Stack, {.widthUnits = _widthUnits, .heightUnits = CHART_UNITS});
  _canvas.Fill(tree.Add({.widthUnits = _widthUnits, .heightUnits = CHART_UNITS}), theme.barTrackColor);
  const std::size_t slots = std::max(_slots, _values.size());
  const float slot = _widthUnits / static_cast<float>(std::max<std::size_t>(slots, 1));
  const auto most = static_cast<float>(std::max<std::uint32_t>(_most, 1));
  for (std::size_t index = 0; index < _values.size(); ++index)
  {
    const float height = std::max(1.0f, CHART_UNITS * std::min(1.0f, static_cast<float>(_values[index]) / most));
    const float left = slot * static_cast<float>(slots - _values.size() + index);
    _canvas.Fill(tree.Add({.widthUnits = std::max(1.0f, slot - CHART_GAP_UNITS),
                           .heightUnits = height,
                           .anchor = Neuron::LayoutAnchor::TopLeft,
                           .margin = {.left = left, .top = CHART_UNITS - height}}),
                 theme.barFillColor);
  }
  const float markTop = CHART_UNITS * (1.0f - std::min(1.0f, static_cast<float>(_mark) / most));
  _canvas.Fill(tree.Add({.widthUnits = _widthUnits,
                         .heightUnits = 1.0f,
                         .anchor = Neuron::LayoutAnchor::TopLeft,
                         .margin = {.top = std::min(markTop, CHART_UNITS - 1.0f)}}),
               theme.textColor);
  tree.End();
}

// A heading of a window's section after its first, with room above it.
void SectionHeading(Neuron::Canvas& _canvas, std::string _text)
{
  (void)_canvas.Tree().Add({.heightUnits = PADDING});
  Heading(_canvas, std::move(_text));
}

// A line of the server window, in the name face.
void WindowLine(Neuron::Canvas& _canvas, std::string _text, const DirectX::XMFLOAT4& _color, Neuron::LayoutStyle _style = {})
{
  (void)_canvas.Label(Fitted(std::move(_text), Hud::Typeface::Name, NAME_LINE_UNITS), _color, _style);
}

// One of the day's long ticks, as its `tick` record has it (ADR-082 decision 1): its tick, how long it took, how many of its
// minute's ticks took as long, and each part that took a quarter of it or more, longest first.
struct LongTick
{
  std::uint64_t tick = 0;
  std::uint64_t microseconds = 0;
  std::uint64_t over = 0;
  std::string parts;
};

std::vector<LongTick> LongTicksOf(const std::deque<std::string>& _records)
{
  std::vector<LongTick> ticks;
  for (const std::string& record : _records)
  {
    const std::vector<std::string_view> words = WordsOf(record);
    if (words.size() < 2 || words.front() != "tick")
      continue;
    LongTick tick{.tick = NumberOf(words[1]).value_or(0),
                  .microseconds = NumberAfter(words, "us").value_or(0),
                  .over = NumberAfter(words, "over").value_or(0),
                  .parts = {}};
    const auto over = std::ranges::find(words, "over");
    for (auto part = over == words.end() ? words.end() : std::min(over + 2, words.end()); part != words.end() && part + 1 != words.end();
         part += 2)
    {
      tick.parts += std::format("{}{} {}", tick.parts.empty() ? "" : DOT, *part,
                                MillisecondsText(static_cast<std::uint32_t>(NumberOf(*(part + 1)).value_or(0))));
    }
    ticks.push_back(std::move(tick));
  }
  return ticks;
}

// The server window (ADR-103 decision 7), its body _innerUnits wide: the tick by part over the hour of Health held, the
// longest tick of each minute of it against the budget, the day's long ticks and dropped ticks, the saves the log's records
// hold, the game's listener, and the process.
void LayServerWindow(Neuron::Canvas& _canvas, const Outpost::OverseerModel& _model, float _innerUnits)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  const std::deque<Outpost::HeardHealth>& healths = _model.Healths();
  const std::optional<Outpost::OverseerWelcomeMessage>& welcome = _model.Welcome();
  const std::uint32_t perSecond = welcome.has_value() ? welcome->ticksPerSecond : 0;
  (void)_canvas.BeginScroll("server window body", {.grow = 1.0f});
  (void)tree.Begin(Neuron::LayoutKind::Column, {.gapUnits = CELL_GAP_UNITS, .alignX = Neuron::LayoutAlign::Stretch});
  const float chartUnits = _innerUnits - theme.scrollLaneUnits;

  // The tick by part, summed over the hour of Health held, each part's share and its time a tick.
  Heading(_canvas, "THE TICK BY PART, THE LAST HOUR");
  std::array<std::uint64_t, Outpost::TICK_PART_COUNT> parts{};
  std::uint64_t ticks = 0;
  for (const Outpost::HeardHealth& heard : healths)
  {
    for (std::size_t part = 0; part < parts.size(); ++part)
      parts[part] += heard.health.partMicroseconds[part];
    ticks += heard.health.tickMicroseconds.size();
  }
  const std::uint64_t total = std::accumulate(parts.begin(), parts.end(), std::uint64_t{0});
  if (total == 0 || ticks == 0)
    WindowLine(_canvas, "No tick timed yet.", theme.dimTextColor);
  else
  {
    std::array<std::size_t, Outpost::TICK_PART_COUNT> order{};
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::stable_sort(order, std::ranges::greater{}, [&parts](std::size_t _part) { return parts[_part]; });
    for (const std::size_t part : order)
    {
      if (parts[part] == 0)
        continue;
      const double share = static_cast<double>(parts[part]) / static_cast<double>(total);
      (void)tree.Begin(Neuron::LayoutKind::Row,
                       {.heightUnits = NAME_LINE_UNITS, .gapUnits = MARGIN, .alignY = Neuron::LayoutAlign::Center});
      WindowLine(_canvas, std::string(Outpost::TickPartName(static_cast<Outpost::TickPart>(part))), theme.labelColor,
                 {.widthUnits = PART_NAME_UNITS});
      (void)_canvas.ProgressBar(static_cast<float>(share), {.heightUnits = PART_BAR_UNITS, .grow = 1.0f});
      WindowLine(_canvas,
                 std::format("{:.1f}%{}{} a tick", 100.0 * share, DOT, MillisecondsText(static_cast<std::uint32_t>(parts[part] / ticks))),
                 theme.textColor, {.widthUnits = PART_FIGURE_UNITS});
      tree.End();
    }
  }

  // The longest tick of each minute of the hour, the newest at the right, against the budget.
  SectionHeading(_canvas, "THE LONGEST TICK OF EACH MINUTE, THE LAST HOUR");
  std::vector<std::uint32_t> minutes;
  for (std::size_t end = healths.size(); end > 0;)
  {
    const std::size_t begin = end > HEALTHS_A_MINUTE ? end - HEALTHS_A_MINUTE : 0;
    std::uint32_t longest = 0;
    for (std::size_t index = begin; index < end; ++index)
    {
      for (const std::uint32_t tick : healths[index].health.tickMicroseconds)
        longest = std::max(longest, tick);
    }
    minutes.push_back(longest);
    end = begin;
  }
  std::ranges::reverse(minutes);
  const std::uint32_t hourLongest = minutes.empty() ? 0 : std::ranges::max(minutes);
  BarChart(_canvas, minutes, MINUTES_AN_HOUR, std::max(2 * OverseerScreen::TICK_BUDGET_MICROSECONDS, hourLongest),
           OverseerScreen::TICK_BUDGET_MICROSECONDS, chartUnits);
  WindowLine(_canvas,
             std::format("The line is the {} budget; the hour's longest took {}.",
                         MillisecondsText(OverseerScreen::TICK_BUDGET_MICROSECONDS), MillisecondsText(hourLongest)),
             theme.dimTextColor);

  // The day's long ticks, the longest first, and its dropped ticks.
  SectionHeading(_canvas, "THE DAY'S LONG TICKS");
  std::vector<LongTick> longTicks = LongTicksOf(_model.DayRecords());
  std::ranges::stable_sort(longTicks, std::ranges::greater{}, &LongTick::microseconds);
  if (longTicks.empty())
    WindowLine(_canvas, std::format("No tick took {} or more in the day.", MillisecondsText(OverseerScreen::TICK_BUDGET_MICROSECONDS)),
               theme.dimTextColor);
  for (std::size_t index = 0; index < std::min(longTicks.size(), LONG_TICKS_SHOWN); ++index)
  {
    const LongTick& tick = longTicks[index];
    WindowLine(_canvas,
               std::format("{}{}{}{}{} of its minute{}{}", Outpost::UniverseTime(tick.tick, perSecond), DOT,
                           MillisecondsText(static_cast<std::uint32_t>(tick.microseconds)), DOT, tick.over, tick.parts.empty() ? "" : DOT,
                           tick.parts),
               theme.textColor);
  }
  std::uint64_t dropped = 0;
  std::uint64_t droppedMost = 0;
  for (const std::string& record : _model.DayRecords())
  {
    const std::vector<std::string_view> words = WordsOf(record);
    if (words.empty() || words.front() != "dropped")
      continue;
    const std::uint64_t count = NumberAfter(words, "ticks").value_or(0);
    dropped += count;
    droppedMost = std::max(droppedMost, count);
  }
  WindowLine(_canvas,
             dropped == 0 ? std::string("No tick dropped in the day.")
                          : std::format("{} ticks dropped in the day, {} the most in a minute.",
                                        Outpost::WithThousands(static_cast<std::int64_t>(dropped)), droppedMost),
             dropped == 0 ? theme.dimTextColor : theme.textColor);

  // The saves the log's records hold, the newest first, each with its write, which the folder's thread times.
  SectionHeading(_canvas, "SAVES");
  const std::deque<std::string>& records = _model.Records();
  std::size_t saves = 0;
  for (auto record = records.rbegin(); record != records.rend() && saves < SAVES_SHOWN; ++record)
  {
    const std::vector<std::string_view> words = WordsOf(*record);
    if (words.size() < 2 || words.front() != "save")
      continue;
    const std::uint64_t tick = NumberOf(words[1]).value_or(0);
    std::string write = "not yet written";
    for (auto later = record.base(); later != records.end(); ++later)
    {
      const std::vector<std::string_view> laterWords = WordsOf(*later);
      if (laterWords.size() >= 2 && laterWords.front() == "written" && NumberOf(laterWords[1]) == tick)
      {
        write = std::format("written in {}", MillisecondsText(static_cast<std::uint32_t>(NumberAfter(laterWords, "write_us").value_or(0))));
        break;
      }
    }
    WindowLine(_canvas,
               std::format("{}{}{}{}encoded in {}{}{}", Outpost::UniverseTime(tick, perSecond), DOT,
                           Outpost::BytesText(NumberAfter(words, "bytes").value_or(0)), DOT,
                           MillisecondsText(static_cast<std::uint32_t>(NumberAfter(words, "encode_us").value_or(0))), DOT, write),
               theme.textColor);
    ++saves;
  }
  if (saves == 0)
    WindowLine(_canvas, "No save in the log's records yet.", theme.dimTextColor);

  // The game's listener and the process, as the newest Health says.
  const Outpost::HeardHealth* latest = _model.LatestHealth();
  SectionHeading(_canvas, "THE GAME'S LISTENER");
  if (latest == nullptr)
    WindowLine(_canvas, "No Health yet.", theme.dimTextColor);
  else
  {
    const Outpost::HealthMessage& health = latest->health;
    WindowLine(_canvas, std::format("{} connections held; the joins since the server started:", health.connections), theme.textColor);
    (void)tree.Begin(Neuron::LayoutKind::Grid, {.gapUnits = CELL_GAP_UNITS, .columns = 4});
    for (std::size_t outcome = 0; outcome < health.joins.size(); ++outcome)
      WindowLine(_canvas, std::format("{} {}", health.joins[outcome], JOIN_OUTCOME_NAMES[outcome]),
                 health.joins[outcome] == 0 ? theme.dimTextColor : theme.textColor);
    tree.End();
  }
  SectionHeading(_canvas, "THE PROCESS");
  if (latest == nullptr)
    WindowLine(_canvas, "No Health yet.", theme.dimTextColor);
  else
  {
    const Outpost::ProcessHealth& process = latest->health.process;
    WindowLine(_canvas,
               std::format("Working set {}{}private {}{}{}% of a core", Outpost::BytesText(process.workingSetBytes), DOT,
                           Outpost::BytesText(process.privateBytes), DOT, process.cpuHundredths),
               theme.textColor);
    WindowLine(_canvas,
               std::format("{} threads{}{} handles{}up {}{}{} free on its disk", process.threads, DOT, process.handles, DOT,
                           Outpost::UniverseTime(process.uptimeSeconds, 1), DOT, Outpost::BytesText(process.diskFreeBytes)),
               theme.textColor);
  }
  tree.End();
  _canvas.EndScroll();
}

// A signed number after _key among _words, as an hour's Ore and income may be below nothing.
std::optional<std::int64_t> SignedAfter(std::span<const std::string_view> _words, std::string_view _key)
{
  const auto key = std::ranges::find(_words, _key);
  if (key == _words.end() || key + 1 == _words.end())
    return std::nullopt;
  std::int64_t value = 0;
  const std::string_view word = *(key + 1);
  const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), value);
  return error == std::errc{} && end == word.data() + word.size() ? std::optional<std::int64_t>{value} : std::nullopt;
}

// Whether _words, a record's, are of _kind and name _seat's player ("player <p>").
bool IsSeatsRecord(std::span<const std::string_view> _words, std::string_view _kind, Outpost::PlayerId _seat)
{
  return _words.size() >= 2 && _words.front() == _kind && NumberAfter(_words, "player") == std::uint64_t{_seat.value};
}

// How a scheduled order fires, as the seat window writes it.
std::string TriggerText(const Outpost::ScheduledTrigger& _trigger, std::uint32_t _ticksPerSecond)
{
  switch (_trigger.kind)
  {
  case Outpost::ScheduledTriggerKind::TimeOfDay:
    return std::format("at {}", Outpost::UniverseTime(_trigger.tick, _ticksPerSecond));
  case Outpost::ScheduledTriggerKind::EnemyInSector:
    return std::format("when an enemy enters sector {}", _trigger.sector);
  case Outpost::ScheduledTriggerKind::RelayThreatened:
    return std::format("when its Relay in sector {} is threatened", _trigger.sector);
  case Outpost::ScheduledTriggerKind::RigLost:
    return std::format("when a rig of its in sector {} is lost", _trigger.sector);
  }
  return {};
}

// The sectors of _sectors, "4, 7", or none.
std::string SectorsText(std::span<const std::int32_t> _sectors)
{
  std::string text;
  for (const std::int32_t sector : _sectors)
    text += std::format("{}{}", text.empty() ? "" : ", ", sector);
  return text.empty() ? std::string("none") : text;
}

// The seat window (ADR-103 decision 7), its body _innerUnits wide: who plays _seat and how it stands; its link over the last
// four hours, from its hour records and the last hour of Health, and how long it was down, from its joins; its joins of the
// day; its hours of the day; and what waits for its player, its report of the time away and its scheduled orders.
void LaySeatWindow(Neuron::Canvas& _canvas, const Outpost::OverseerModel& _model, Outpost::PlayerId _seat, float _innerUnits)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  const std::optional<Outpost::OverseerWelcomeMessage>& welcome = _model.Welcome();
  const std::uint32_t perSecond = welcome.has_value() ? welcome->ticksPerSecond : 0;
  const Outpost::HeardHealth* latest = _model.LatestHealth();
  const Outpost::SeatHealth* seat = nullptr;
  if (latest != nullptr)
  {
    if (const auto found = std::ranges::find(latest->health.seats, _seat, &Outpost::SeatHealth::player);
        found != latest->health.seats.end())
      seat = &*found;
  }
  std::string difficulty;
  if (welcome.has_value())
  {
    if (const auto found = std::ranges::find(welcome->seats, _seat, &Outpost::OverseerSeat::player); found != welcome->seats.end())
      difficulty = found->ai;
  }
  const bool ai = seat != nullptr ? seat->play == Outpost::SeatPlay::Ai : !difficulty.empty();
  const std::uint64_t newest = _model.NewestTick();
  const std::deque<std::string>& records = _model.DayRecords();
  (void)_canvas.BeginScroll("seat window body", {.grow = 1.0f});
  (void)tree.Begin(Neuron::LayoutKind::Column, {.gapUnits = CELL_GAP_UNITS, .alignX = Neuron::LayoutAlign::Stretch});

  // Who plays it, and how it stands.
  if (seat != nullptr)
  {
    const SeatStatus status = StatusOf(*seat, latest->health.tick, perSecond);
    WindowLine(_canvas, std::format("Played by its {}{}{}, {}", WhoPlays(*seat, difficulty), DOT, status.name, status.detail),
               theme.textColor);
  }
  else
    WindowLine(_canvas, "No Health of it yet.", theme.dimTextColor);

  // Its link over the last four hours: its hours, the last hour heard a minute at a time, and how long it was down.
  Heading(_canvas, "THE LINK, THE LAST FOUR HOURS");
  const std::uint64_t linkSpan = LINK_SECONDS * perSecond;
  const std::uint64_t linkFrom = newest > linkSpan ? newest - linkSpan : 0;
  if (ai)
    WindowLine(_canvas, "An AI empire's seat has no link.", theme.dimTextColor);
  else
  {
    std::size_t hours = 0;
    for (auto record = records.rbegin(); record != records.rend(); ++record)
    {
      const std::vector<std::string_view> words = WordsOf(*record);
      if (!IsSeatsRecord(words, "hour", _seat) || NumberOf(words[1]).value_or(0) < linkFrom)
        continue;
      const std::uint64_t tick = NumberOf(words[1]).value_or(0);
      ++hours;
      if (!NumberAfter(words, "rtt_median_us").has_value())
      {
        WindowLine(_canvas, std::format("{}{}its player was not connected in the hour", Outpost::UniverseTime(tick, perSecond), DOT),
                   theme.dimTextColor);
        continue;
      }
      const std::uint64_t sent = NumberAfter(words, "sent").value_or(0);
      const std::uint64_t lost = NumberAfter(words, "lost").value_or(0);
      WindowLine(_canvas,
                 std::format("{}{}round trip {} median, {} at the 95th", Outpost::UniverseTime(tick, perSecond), DOT,
                             MillisecondsText(static_cast<std::uint32_t>(NumberAfter(words, "rtt_median_us").value_or(0))),
                             MillisecondsText(static_cast<std::uint32_t>(NumberAfter(words, "rtt_p95_us").value_or(0)))),
                 theme.textColor);
      WindowLine(_canvas,
                 std::format("loss {:.1f}%{}backlog at most {}{}{} sent",
                             sent > 0 ? 100.0 * static_cast<double>(lost) / static_cast<double>(sent) : 0.0, DOT,
                             Outpost::BytesText(NumberAfter(words, "backlog_most").value_or(0)), DOT,
                             Outpost::BytesText(NumberAfter(words, "sent_bytes").value_or(0))),
                 theme.dimTextColor);
    }
    if (hours == 0)
      WindowLine(_canvas, "No hour of its link in the four hours.", theme.dimTextColor);

    // The last hour heard, a minute a bar: the minute's round trip at the 95th percentile, against what needs attention.
    const std::deque<Outpost::HeardHealth>& healths = _model.Healths();
    std::vector<std::uint32_t> minutes;
    for (std::size_t end = healths.size(); end > 0;)
    {
      const std::size_t begin = end > HEALTHS_A_MINUTE ? end - HEALTHS_A_MINUTE : 0;
      std::vector<std::uint32_t> trips;
      for (std::size_t index = begin; index < end; ++index)
      {
        const std::vector<Outpost::SeatHealth>& seats = healths[index].health.seats;
        if (const auto found = std::ranges::find(seats, _seat, &Outpost::SeatHealth::player);
            found != seats.end() && found->connection.has_value())
          trips.push_back(found->connection.value_or(Outpost::LinkFigures{}).roundTripMicroseconds);
      }
      std::ranges::sort(trips);
      minutes.push_back(
        trips.empty() ? 0 : trips[std::max<std::size_t>(1, ((trips.size() * Outpost::ATTENTION_ROUND_TRIP_PERCENTILE) + 99) / 100) - 1]);
      end = begin;
    }
    std::ranges::reverse(minutes);
    if (std::ranges::all_of(minutes, [](std::uint32_t _trip) { return _trip == 0; }))
      WindowLine(_canvas, "No link heard in the last hour.", theme.dimTextColor);
    else
    {
      const std::uint32_t slowest = std::ranges::max(minutes);
      BarChart(_canvas, minutes, MINUTES_AN_HOUR, std::max(2 * Outpost::ATTENTION_ROUND_TRIP_MICROSECONDS, slowest),
               Outpost::ATTENTION_ROUND_TRIP_MICROSECONDS, _innerUnits - theme.scrollLaneUnits);
      WindowLine(_canvas,
                 std::format("The line is {}; the hour's slowest minute ran {} at the {}th percentile.",
                             MillisecondsText(Outpost::ATTENTION_ROUND_TRIP_MICROSECONDS), MillisecondsText(slowest),
                             Outpost::ATTENTION_ROUND_TRIP_PERCENTILE),
                 theme.dimTextColor);
    }

    // How long its link was down in the four hours: from each connection gone to the next welcomed.
    std::uint64_t down = 0;
    std::uint64_t times = 0;
    std::optional<std::uint64_t> goneAt;
    for (const std::string& record : records)
    {
      const std::vector<std::string_view> words = WordsOf(record);
      if (!IsSeatsRecord(words, "join", _seat))
        continue;
      const std::uint64_t tick = std::max(NumberOf(words[1]).value_or(0), linkFrom);
      if (words.size() > 2 && words[2] == "gone" && !goneAt.has_value())
      {
        goneAt = tick;
        ++times;
      }
      else if (words.size() > 2 && words[2] == "welcomed" && goneAt.has_value())
      {
        down += tick - std::min(tick, goneAt.value_or(tick));
        goneAt.reset();
      }
    }
    if (goneAt.has_value())
      down += newest - std::min(newest, goneAt.value_or(newest));
    WindowLine(_canvas,
               times == 0
                 ? std::string("Its link was not down in the four hours.")
                 : std::format("Its link was down {} of the four hours, {} {}.",
                               Outpost::MinutesAndSeconds(perSecond > 0 ? down / perSecond : 0), times, times == 1 ? "time" : "times"),
               times == 0 ? theme.dimTextColor : theme.textColor);
  }

  // Its joins of the day, the newest first: what came of each, and the address it came from.
  SectionHeading(_canvas, "JOINS, THE LAST DAY");
  std::size_t joins = 0;
  for (auto record = records.rbegin(); record != records.rend(); ++record)
  {
    const std::vector<std::string_view> words = WordsOf(*record);
    if (!IsSeatsRecord(words, "join", _seat))
      continue;
    std::string what;
    for (std::size_t word = 2; word < words.size(); ++word)
    {
      if (words[word] == "player" && word + 1 < words.size())
      {
        ++word;
        continue;
      }
      what += std::format("{}{}", what.empty() ? "" : " ", words[word]);
    }
    WindowLine(_canvas, std::format("{}{}{}", Outpost::UniverseTime(NumberOf(words[1]).value_or(0), perSecond), DOT, what),
               theme.textColor);
    ++joins;
  }
  if (joins == 0)
    WindowLine(_canvas, "No join in the day.", theme.dimTextColor);

  // Its hours of the day, the newest first.
  SectionHeading(_canvas, "ITS HOURS, THE DAY");
  std::size_t hours = 0;
  for (auto record = records.rbegin(); record != records.rend(); ++record)
  {
    const std::vector<std::string_view> words = WordsOf(*record);
    if (!IsSeatsRecord(words, "hour", _seat))
      continue;
    WindowLine(_canvas,
               std::format("{}{}Ore {}{}{:+.2f} a second{}fleet {} / {}{}{} nodes{}{} researched",
                           Outpost::UniverseTime(NumberOf(words[1]).value_or(0), perSecond), DOT,
                           Outpost::WithThousands(SignedAfter(words, "ore").value_or(0)), DOT,
                           static_cast<double>(SignedAfter(words, "income").value_or(0)) / 100.0, DOT,
                           NumberAfter(words, "fleet").value_or(0), NumberAfter(words, "cap").value_or(0), DOT,
                           NumberAfter(words, "nodes").value_or(0), DOT, NumberAfter(words, "researched").value_or(0)),
               theme.textColor);
    WindowLine(_canvas,
               std::format("its player {}, its deputy {}{}{} orders, {} refused",
                           Outpost::MinutesAndSeconds(NumberAfter(words, "present").value_or(0)),
                           Outpost::MinutesAndSeconds(NumberAfter(words, "deputy").value_or(0)), DOT,
                           Outpost::WithThousands(static_cast<std::int64_t>(NumberAfter(words, "orders").value_or(0))),
                           NumberAfter(words, "refused").value_or(0)),
               theme.dimTextColor);
    ++hours;
  }
  if (hours == 0)
    WindowLine(_canvas, "No hour of it in the day yet.", theme.dimTextColor);

  // What waits for its player: the report the server keeps while its deputy plays it, and its scheduled orders.
  SectionHeading(_canvas, "WHAT WAITS FOR ITS PLAYER");
  if (seat != nullptr && seat->away.has_value())
  {
    const Outpost::AwayReport& away = *seat->away;
    WindowLine(_canvas,
               std::format("Its deputy has played it since {}, for {}.", Outpost::UniverseTime(away.sinceTick, perSecond),
                           SpanText(away.sinceTick, latest->health.tick, perSecond)),
               theme.textColor);
    WindowLine(_canvas,
               std::format("Built {} ships and {} structures; lost {} ships and {} structures.", away.shipsBuilt, away.structuresBuilt,
                           away.shipsLost, away.structuresLost),
               theme.textColor);
    WindowLine(_canvas, std::format("Sectors gained: {}; lost: {}.", SectorsText(away.sectorsGained), SectorsText(away.sectorsLost)),
               theme.textColor);
    WindowLine(_canvas,
               away.ordersFired.empty() ? std::string("No scheduled order of its fired.")
                                        : std::format("{} of its scheduled orders fired.", away.ordersFired.size()),
               theme.textColor);
  }
  else if (ai)
    WindowLine(_canvas, "An AI empire's seat has no player.", theme.dimTextColor);
  else if (seat != nullptr && seat->play == Outpost::SeatPlay::Player)
    WindowLine(_canvas, "Nothing waits: its player plays it.", theme.dimTextColor);
  else
    WindowLine(_canvas, "Nothing waits: its player has not held it since its deputy took it.", theme.dimTextColor);
  if (!ai)
  {
    const std::optional<Outpost::Snapshot>& view = _model.View();
    if (!view.has_value() || view->player != _seat)
      WindowLine(_canvas, "Its scheduled orders show once its sight comes.", theme.dimTextColor);
    else if (view->scheduled.empty())
      WindowLine(_canvas, "No scheduled order waits.", theme.dimTextColor);
    else
    {
      for (const Outpost::ScheduledOrderView& order : view->scheduled)
      {
        const auto action = static_cast<std::size_t>(order.action.kind);
        WindowLine(_canvas,
                   std::format("Order {}{}{}{}{}{}{} ships", order.id, DOT,
                               action < ACTION_NAMES.size() ? ACTION_NAMES[action] : "an order", DOT, TriggerText(order.trigger, perSecond),
                               DOT, order.ships.size()),
                   theme.textColor);
      }
    }
  }
  tree.End();
  _canvas.EndScroll();
}

} // namespace

// The confirm window's title for _action, and what it says the action does.
std::string ConfirmTitle(const Outpost::ActionMessage& _action)
{
  switch (_action.action)
  {
  case Outpost::OverseerAction::Stop:
    return "STOP THE SERVER";
  case Outpost::OverseerAction::SaveNow:
  case Outpost::OverseerAction::DropLink:
  case Outpost::OverseerAction::NewToken:
    break;
  }
  return {};
}

std::vector<std::string> ConfirmWords(const Outpost::ActionMessage& _action)
{
  switch (_action.action)
  {
  case Outpost::OverseerAction::Stop:
    return {"Stopping ends the server as Ctrl+C does. No tick runs until it is started again, on its own machine.",
            "It comes back from its folder: from its last save, with the commands logged since.",
            "Its players lose their connection until then, and their deputies do not play."};
  case Outpost::OverseerAction::SaveNow:
  case Outpost::OverseerAction::DropLink:
  case Outpost::OverseerAction::NewToken:
    break;
  }
  return {};
}

// The confirm window (ADR-104): what the action does, and the buttons that ask for it, only of a running server, or leave it.
void LayConfirmWindow(Neuron::Canvas& _canvas, const Outpost::ActionMessage& _action, bool _running)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  for (std::string& words : ConfirmWords(_action))
  {
    (void)_canvas.Label(
      {.text = std::move(words), .font = Hud::FontOf(Hud::Typeface::Name), .lineUnits = NAME_LINE_UNITS, .flow = Neuron::TextFlow::Wrap},
      theme.textColor);
  }
  if (!_running)
    WindowLine(_canvas, "The server is not running, so nothing can be asked of it.", theme.dimTextColor);
  (void)tree.Add({.heightUnits = PADDING});
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = MARGIN});
  (void)tree.Add({.grow = 1.0f});
  const Neuron::LayoutStyle button{.widthUnits = CONFIRM_BUTTON_UNITS, .heightUnits = BUTTON_UNITS};
  (void)_canvas.Button("confirm button", {.label = "Stop", .payload = PayloadOf(PressKind::Confirm, 0), .enabled = _running}, button);
  (void)_canvas.Button("cancel button", {.label = "Keep running", .payload = PayloadOf(PressKind::Cancel, 0)}, button);
  tree.End();
}

Outpost::OverseerScreen::OverseerScreen(std::vector<Hud::SideColor> _seatColors)
  : m_seatColors(std::move(_seatColors))
{
}

const Neuron::DrawList& Outpost::OverseerScreen::Lay(const OverseerModel& _model, const OverseerLink& _link,
                                                     std::chrono::steady_clock::time_point _now, const Hud::TextMetrics& _metrics,
                                                     std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const float scale = Hud::Scale(_widthPixels, _heightPixels);
  const float widthUnits = static_cast<float>(_widthPixels) / scale;
  const float heightUnits = static_cast<float>(_heightPixels) / scale;
  m_interaction.BeginBuild();
  Neuron::Canvas canvas(Hud::Theme(), _metrics, m_interaction, widthUnits, heightUnits);
  Neuron::LayoutTree& tree = canvas.Tree();
  (void)tree.Begin(Neuron::LayoutKind::Column, {.widthUnits = widthUnits,
                                                .heightUnits = heightUnits,
                                                .padding = {.left = MARGIN, .top = MARGIN, .right = MARGIN, .bottom = MARGIN},
                                                .gapUnits = MARGIN,
                                                .alignX = Neuron::LayoutAlign::Stretch});
  const HeaderState state = StateOf(_model, _link, _now);
  m_running = state.running;
  LayHeader(canvas, _model, _link, state, _now);
  LayTiles(canvas, _model);
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = MARGIN, .alignY = Neuron::LayoutAlign::Stretch, .grow = 1.0f});
  (void)tree.Begin(Neuron::LayoutKind::Column, {.gapUnits = MARGIN, .alignX = Neuron::LayoutAlign::Stretch, .grow = 1.0f});

  // Over the seat table, the map and what needs attention.
  const std::vector<Attention> attention = AttentionOf(_model);
  (void)tree.Begin(Neuron::LayoutKind::Row, {.heightUnits = TOP_ROW_UNITS, .gapUnits = MARGIN, .alignY = Neuron::LayoutAlign::Stretch});
  const std::size_t mapSquare = LayMap(canvas, _model, *this);
  LayAttention(canvas, attention, _model.LatestHealth() != nullptr);
  tree.End();

  // The seat table: its filters, then its rows, sorted, the picked one lit.
  std::vector<SeatLine> lines = SeatLinesOf(_model, *this, attention);
  const auto sortColumn = static_cast<std::size_t>(m_sortColumn);
  std::ranges::stable_sort(lines,
                           [this, sortColumn](const SeatLine& _first, const SeatLine& _second)
                           {
                             const std::int64_t first = _first.keys[sortColumn];
                             const std::int64_t second = _second.keys[sortColumn];
                             if (first != second)
                               return m_sortDescending ? first > second : first < second;
                             return _first.player.value < _second.player.value;
                           });
  std::vector<Neuron::TableRow> rows;
  for (SeatLine& line : lines)
  {
    const bool shown = m_filter == SeatFilter::All || (m_filter == SeatFilter::Players && !line.ai) ||
                       (m_filter == SeatFilter::Ai && line.ai) || (m_filter == SeatFilter::Away && line.away) ||
                       (m_filter == SeatFilter::NeedsALook && line.needsALook);
    if (shown)
      rows.push_back(std::move(line.row));
  }
  std::array<Neuron::TableColumn, SEAT_COLUMN_COUNT> columns;
  for (std::size_t column = 0; column < SEAT_COLUMN_COUNT; ++column)
  {
    columns[column] = {.heading = std::string(SEAT_HEADINGS[column]),
                       .widthUnits = SEAT_WIDTHS[column],
                       .grow = 1.0f,
                       .payload = PayloadOf(PressKind::Sort, static_cast<std::uint32_t>(column))};
  }
  (void)BeginPanel(canvas, {.grow = 1.0f});
  Heading(canvas, std::format("SEATS{}{} SHOWN", DOT, rows.size()));
  FilterCells(canvas, "seat filters", SEAT_FILTERS, static_cast<std::size_t>(m_filter), PressKind::Filter);
  (void)tree.Add({.heightUnits = CELL_GAP_UNITS});
  (void)canvas.Table(
    "seats", columns,
    Neuron::TableSorted{.column = sortColumn, .sort = m_sortDescending ? Neuron::TableSort::Descending : Neuron::TableSort::Ascending},
    rows, {.grow = 1.0f});
  // The picked seat's action bar, at the table's foot (owner, 2026-10-28): its window, and the seat's actions to come.
  if (m_picked.has_value())
  {
    (void)tree.Add({.heightUnits = CELL_GAP_UNITS});
    (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = MARGIN, .alignY = Neuron::LayoutAlign::Center});
    (void)canvas.Label(Fitted(std::format("Seat {} picked", m_picked->value), Hud::Typeface::Name, NAME_LINE_UNITS), Hud::Theme().textColor,
                       {.grow = 1.0f});
    (void)canvas.Button("seat button", {.label = "Seat", .payload = PayloadOf(PressKind::SeatWindow, 0)},
                        {.widthUnits = HEADER_BUTTON_UNITS, .heightUnits = BUTTON_UNITS});
    tree.End();
  }
  canvas.EndPanel();
  tree.End();

  // At the right, who played over the last day, then the universe log: its filters, then its records, the newest first.
  (void)tree.Begin(Neuron::LayoutKind::Column,
                   {.widthUnits = RIGHT_COLUMN_UNITS, .gapUnits = MARGIN, .alignX = Neuron::LayoutAlign::Stretch});
  LayWhoPlayed(canvas, _model, *this);
  (void)BeginPanel(canvas, {.grow = 1.0f});
  Heading(canvas, "UNIVERSE LOG");
  FilterCells(canvas, "log filters", LOG_FILTERS, static_cast<std::size_t>(m_logFilter), PressKind::LogFilter);
  (void)tree.Add({.heightUnits = CELL_GAP_UNITS});
  (void)canvas.BeginScroll("records", {.grow = 1.0f});
  std::size_t shown = 0;
  const std::deque<std::string>& records = _model.Records();
  for (auto record = records.rbegin(); record != records.rend() && shown < RECORDS_SHOWN; ++record)
  {
    if (!Keeps(m_logFilter, std::string_view(*record).substr(0, record->find(' '))))
      continue;
    (void)canvas.Label(
      {.text = *record, .font = Hud::FontOf(Hud::Typeface::Detail), .lineUnits = DETAIL_LINE_UNITS, .flow = Neuron::TextFlow::Wrap},
      Hud::Theme().textColor);
    ++shown;
  }
  canvas.EndScroll();
  canvas.EndPanel();
  tree.End();

  tree.End();
  tree.End();
  m_layout = {};
  canvas.Finish(m_layout);
  // The map's drawing, into its square as the canvas placed it, as the HUD draws its minimap.
  const std::optional<Snapshot>& view = _model.View();
  const std::optional<OverseerWelcomeMessage>& welcome = _model.Welcome();
  if (mapSquare != Neuron::LayoutNode::NO_NODE && view.has_value() && welcome.has_value())
  {
    const Neuron::LayoutNode& square = tree.Node(mapSquare);
    Hud::DrawMinimap(m_layout, {square.leftUnits * scale, square.topUnits * scale, square.widthUnits * scale, square.heightUnits * scale},
                     MapContentOf(*view, *welcome, *this), scale);
  }
  // The windows, back to front, each on a canvas of its own over the overview and the map, as the game's are (ADR-100).
  m_scale = scale;
  const std::vector<OverseerWindow> open(m_windows.FrontToBack().begin(), m_windows.FrontToBack().end());
  for (auto window = open.rbegin(); window != open.rend(); ++window)
    LayWindow(*window, _model, _metrics, widthUnits, heightUnits);
  return m_layout;
}

void Outpost::OverseerScreen::LayWindow(OverseerWindow _window, const OverseerModel& _model, const Hud::TextMetrics& _metrics,
                                        float _screenWidthUnits, float _screenHeightUnits)
{
  const auto index = static_cast<std::size_t>(_window);
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::Canvas canvas(theme, _metrics, m_interaction, _screenWidthUnits, _screenHeightUnits);
  const ActionMessage confirming = m_confirming.value_or(ActionMessage{});
  Neuron::WidgetId widget = m_interaction.IdOf("server window");
  std::string title = "SERVER";
  float width = SERVER_WINDOW_UNITS;
  if (_window == OverseerWindow::Seat)
  {
    widget = m_interaction.IdOf("seat window");
    title = std::format("SEAT {}", m_seatShown.value);
    width = SEAT_WINDOW_UNITS;
  }
  else if (_window == OverseerWindow::Confirm)
  {
    widget = m_interaction.IdOf("confirm window");
    title = ConfirmTitle(confirming);
    width = CONFIRM_WINDOW_UNITS;
  }
  m_titleBars[index] = canvas.TitleBarOf(widget);
  m_closeBoxes[index] = canvas.CloseBoxOf(widget);
  const Neuron::LayoutStyle body{.padding = {.left = PADDING, .top = PADDING, .right = PADDING, .bottom = PADDING},
                                 .alignX = Neuron::LayoutAlign::Stretch};
  // No taller than the screen leaves it; a window moved stays where it was left, kept on the screen.
  const float most = _screenHeightUnits - (2.0f * MARGIN);
  std::size_t frame = Neuron::LayoutNode::NO_NODE;
  if (const std::optional<Neuron::CanvasPoint> left = m_windows.PositionOf(_window))
  {
    const Neuron::CanvasPoint corner =
      Neuron::KeepOnScreen(*left, width, _screenWidthUnits, _screenHeightUnits, Hud::WINDOW_KEPT_ON_SCREEN_UNITS, theme.titleBarUnits);
    m_windows.Settle(_window, corner);
    frame = canvas.BeginWindow(widget, title, corner, width, body, most);
  }
  else if (_window == OverseerWindow::Server)
    frame =
      canvas.BeginWindow(widget, title, Neuron::LayoutAnchor::TopRight, {.top = WINDOW_TOP_UNITS, .right = MARGIN}, width, body, most);
  else if (_window == OverseerWindow::Seat)
    frame = canvas.BeginWindow(widget, title, Neuron::LayoutAnchor::TopLeft, {.left = MARGIN, .top = WINDOW_TOP_UNITS}, width, body, most);
  else
    frame = canvas.BeginWindow(widget, title, Neuron::LayoutAnchor::Middle, {}, width, body, most);
  switch (_window)
  {
  case OverseerWindow::Server:
    LayServerWindow(canvas, _model, width - (2.0f * PADDING));
    break;
  case OverseerWindow::Seat:
    LaySeatWindow(canvas, _model, m_seatShown, width - (2.0f * PADDING));
    break;
  case OverseerWindow::Confirm:
    LayConfirmWindow(canvas, confirming, m_running);
    break;
  }
  canvas.EndWindow();
  m_windowLayers[index] = m_layout.LayerCount();
  canvas.Finish(m_layout);
  const Neuron::LayoutNode& placed = canvas.Tree().Node(frame);
  m_windowCorners[index] = {.xUnits = placed.leftUnits, .yUnits = placed.topUnits};
}

void Outpost::OverseerScreen::Input(const Neuron::InputState& _input)
{
  if (!_input.active)
    return;
  const auto x = static_cast<float>(_input.cursorXPixels);
  const auto y = static_cast<float>(_input.cursorYPixels);
  m_interaction.Move(m_layout, x, y);
  // A notch away from the owner scrolls back up.
  if (_input.wheelNotches != 0.0f)
    (void)m_interaction.Wheel(m_layout, x, y, -_input.wheelNotches * WHEEL_NOTCH_UNITS);
  for (const Neuron::InputEvent& event : _input.events)
  {
    if (event.key != VK_LBUTTON)
      continue;
    const auto eventX = static_cast<float>(event.xPixels);
    const auto eventY = static_cast<float>(event.yPixels);
    if (event.kind == Neuron::InputEventKind::ButtonDown)
    {
      (void)m_interaction.Press(m_layout, eventX, eventY);
      const Neuron::CanvasRegion* hit = m_layout.HitAt(eventX, eventY);
      // A window's title bar drags it, its close box closes it, and a press anywhere in it brings it to the front.
      const std::size_t layer = m_layout.LayerAt(eventX, eventY);
      bool taken = false;
      for (std::size_t index = 0; index < OVERSEER_WINDOW_COUNT; ++index)
      {
        const auto window = static_cast<OverseerWindow>(index);
        if (!m_windows.IsOpen(window) || m_windowLayers[index] != layer)
          continue;
        if (hit != nullptr && hit->widget == m_closeBoxes[index])
          m_windows.Close(window);
        else if (hit != nullptr && hit->widget == m_titleBars[index])
          m_windows.Grab(window, {.xUnits = eventX / m_scale, .yUnits = eventY / m_scale}, m_windowCorners[index]);
        else
        {
          m_windows.Open(window);
          continue;
        }
        taken = true;
      }
      if (!taken && hit != nullptr && hit->payload != Neuron::NO_PAYLOAD)
        Press(hit->payload);
    }
    else if (event.kind == Neuron::InputEventKind::ButtonUp)
    {
      m_windows.Release();
      (void)m_interaction.Release(m_layout, eventX, eventY);
    }
  }
  if (m_windows.IsDragging() && _input.IsDown(VK_LBUTTON))
    m_windows.Drag({.xUnits = x / m_scale, .yUnits = y / m_scale});
}

void Outpost::OverseerScreen::Press(std::uint32_t _payload)
{
  const std::uint32_t which = _payload & 0xFFFF;
  switch (static_cast<PressKind>(_payload >> 16))
  {
  case PressKind::Sort:
    if (which < SEAT_COLUMN_COUNT)
      SortBy(static_cast<SeatColumn>(which));
    break;
  case PressKind::Filter:
    if (which < SEAT_FILTERS.size())
      Filter(static_cast<SeatFilter>(which));
    break;
  case PressKind::Seat:
    Pick(PlayerId{which});
    break;
  case PressKind::LogFilter:
    if (which < LOG_FILTERS.size())
      FilterLog(static_cast<LogFilter>(which));
    break;
  case PressKind::FullSight:
    WatchFullSight();
    break;
  case PressKind::ServerWindow:
    OpenServerWindow();
    break;
  case PressKind::SeatWindow:
    if (m_picked.has_value())
      OpenSeatWindow(*m_picked);
    break;
  case PressKind::SaveNow:
    SaveNow();
    break;
  case PressKind::Stop:
    AskToStop();
    break;
  case PressKind::Confirm:
    Confirm();
    break;
  case PressKind::Cancel:
    CloseWindow(OverseerWindow::Confirm);
    break;
  }
}

void Outpost::OverseerScreen::SortBy(SeatColumn _column)
{
  m_sortDescending = m_sortColumn == _column && !m_sortDescending;
  m_sortColumn = _column;
}

void Outpost::OverseerScreen::Filter(SeatFilter _filter)
{
  m_filter = _filter;
}

void Outpost::OverseerScreen::Pick(PlayerId _seat)
{
  if (m_picked == _seat)
  {
    m_picked.reset();
    m_sightPicked = PlayerId{};
    return;
  }
  m_picked = _seat;
  m_sightPicked = _seat;
}

void Outpost::OverseerScreen::WatchFullSight()
{
  if (m_picked.has_value())
    Pick(*m_picked);
}

void Outpost::OverseerScreen::OpenServerWindow()
{
  m_windows.Open(OverseerWindow::Server);
}

void Outpost::OverseerScreen::OpenSeatWindow(PlayerId _seat)
{
  m_seatShown = _seat;
  m_windows.Open(OverseerWindow::Seat);
}

void Outpost::OverseerScreen::CloseWindow(OverseerWindow _window) noexcept
{
  m_windows.Close(_window);
}

void Outpost::OverseerScreen::SaveNow()
{
  // No confirm: a save changes nothing of the universe (owner, 2026-10-28).
  if (m_running)
    m_actions.push_back({.action = OverseerAction::SaveNow});
}

void Outpost::OverseerScreen::AskToStop()
{
  if (!m_running)
    return;
  m_confirming = ActionMessage{.action = OverseerAction::Stop};
  m_windows.Open(OverseerWindow::Confirm);
}

void Outpost::OverseerScreen::Confirm()
{
  if (!m_running || !m_confirming.has_value() || !m_windows.IsOpen(OverseerWindow::Confirm))
    return;
  m_actions.push_back(*m_confirming);
  m_windows.Close(OverseerWindow::Confirm);
}

std::vector<Outpost::ActionMessage> Outpost::OverseerScreen::TakeActions()
{
  return std::exchange(m_actions, {});
}

void Outpost::OverseerScreen::FilterLog(LogFilter _filter)
{
  m_logFilter = _filter;
}

std::optional<Outpost::PlayerId> Outpost::OverseerScreen::TakeSight()
{
  return std::exchange(m_sightPicked, std::nullopt);
}

DirectX::XMFLOAT4 Outpost::OverseerScreen::SeatColor(PlayerId _seat) const
{
  if (const auto found = std::ranges::find(m_seatColors, _seat, &Hud::SideColor::player); found != m_seatColors.end())
    return found->color;
  const std::uint32_t past = _seat.value >= 3 ? _seat.value - 3 : _seat.value;
  return OVERSEER_SEAT_PALETTE[past % OVERSEER_SEAT_PALETTE.size()];
}

std::string Outpost::UniverseTime(std::uint64_t _tick, std::uint32_t _ticksPerSecond)
{
  const std::uint64_t seconds = _ticksPerSecond > 0 ? _tick / _ticksPerSecond : 0;
  const std::uint64_t days = seconds / 86'400;
  const std::uint64_t hours = (seconds / 3'600) % 24;
  const std::uint64_t minutes = (seconds / 60) % 60;
  return days > 0 ? std::format("{} d {:02}:{:02}:{:02}", days, hours, minutes, seconds % 60)
                  : std::format("{}:{:02}:{:02}", hours, minutes, seconds % 60);
}

std::string Outpost::BytesText(std::uint64_t _bytes)
{
  constexpr std::array<std::string_view, 4> UNITS{"KB", "MB", "GB", "TB"};
  if (_bytes < 1'024)
    return std::format("{} B", _bytes);
  auto value = static_cast<double>(_bytes) / 1'024.0;
  std::size_t unit = 0;
  while (value >= 1'024.0 && unit + 1 < UNITS.size())
  {
    value /= 1'024.0;
    ++unit;
  }
  return value < 100.0 ? std::format("{:.1f} {}", value, UNITS[unit]) : std::format("{:.0f} {}", value, UNITS[unit]);
}
