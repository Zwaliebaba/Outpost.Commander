#include "pch.h"
#include "OverseerScreen.h"

#include <algorithm>
#include <charconv>
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
  FullSight
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

// The server's state as the header names it (Game §session.overseer), and the lines under it that say why and since when.
struct HeaderState
{
  std::string name;
  std::vector<std::string> lines;
};

// What the server said as it ended.
std::string EndingOf(const Outpost::ClosingMessage& _closing)
{
  switch (_closing.ending)
  {
  case Outpost::ServerEnding::StoppedByOwner:
    return "The server was stopped by its owner";
  case Outpost::ServerEnding::StoppedByWindows:
    return "The server was stopped by Windows";
  case Outpost::ServerEnding::Failed:
    return std::format("The server failed: {}", _closing.messageUtf8);
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
  if (_link.connected && welcome.has_value() && latest != nullptr && _now - latest->heard < OverseerScreen::NO_WORD_AFTER)
  {
    std::string up = std::format("Up {}", Outpost::MinutesAndSeconds(latest->health.process.uptimeSeconds));
    if (_link.roundTrip.has_value())
      up += std::format("; the overseer's round trip {}", MillisecondsText(static_cast<std::uint32_t>(_link.roundTrip->count())));
    return {.name = "Running", .lines = {up + ".", CameBackFrom(*welcome)}};
  }
  HeaderState state{.name = "No word", .lines = {}};
  if (const std::optional<Outpost::ClosingMessage>& closing = _model.Closing())
    state.lines.push_back(std::format("{}; its last save is of tick {}.", EndingOf(*closing), closing->lastSaveTick));
  else if (const std::optional<std::string>& why = _model.LostWhy())
    state.lines.push_back(std::format("The connection went: {}", *why));
  else if (_link.connected)
    state.lines.push_back(std::format("Connected; no Health for {} s.", OverseerScreen::NO_WORD_AFTER.count()));
  if (!_link.connected)
    state.lines.push_back(std::format("Joining {} every {} s.", _link.server, Outpost::OverseerClient::REJOIN_INTERVAL.count()));
  if (latest != nullptr)
  {
    const auto since = std::chrono::duration_cast<std::chrono::seconds>(_now - latest->heard).count();
    state.lines.push_back(std::format("The last word was {} ago, at tick {}.",
                                      Outpost::MinutesAndSeconds(static_cast<std::uint64_t>(since)), latest->health.tick));
  }
  return state;
}

// The header (Game §session.overseer): the universe's folder, its server and the build it runs; the server's state, and why;
// and universe time with its tick.
void LayHeader(Neuron::Canvas& _canvas, const Outpost::OverseerModel& _model, const Outpost::OverseerLink& _link,
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

  const HeaderState state = StateOf(_model, _link, _now);
  (void)tree.Begin(Neuron::LayoutKind::Column, {.widthUnits = STATE_COLUMN_UNITS});
  Heading(_canvas, "SERVER");
  (void)_canvas.Label(Fitted(state.name, Hud::Typeface::Title, TITLE_LINE_UNITS), theme.textColor);
  for (const std::string& line : state.lines)
    (void)_canvas.Label(Fitted(line, Hud::Typeface::Name, NAME_LINE_UNITS), theme.dimTextColor);
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
} // namespace

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
  LayHeader(canvas, _model, _link, _now);
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
  return m_layout;
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
      if (const Neuron::CanvasRegion* hit = m_layout.HitAt(eventX, eventY); hit != nullptr && hit->payload != Neuron::NO_PAYLOAD)
        Press(hit->payload);
    }
    else if (event.kind == Neuron::InputEventKind::ButtonUp)
      (void)m_interaction.Release(m_layout, eventX, eventY);
  }
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
