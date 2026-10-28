#include "pch.h"
#include "OverseerScreen.h"

namespace
{
using Outpost::Hud;

// Everything below in reference units. The screen's margin and the gap between its regions, and a panel's padding.
constexpr float MARGIN = 16.0f;
constexpr float PADDING = 12.0f;
// A line in the title face, in the name face and of a figure, as the HUD's panels set them (ADR-062); a large figure's.
constexpr float TITLE_LINE_UNITS = 30.0f;
constexpr float NAME_LINE_UNITS = 22.0f;
constexpr float FIGURE_LINE_UNITS = 17.0f;
constexpr float LARGE_FIGURE_LINE_UNITS = 36.0f;
constexpr float LABEL_TRACKING_UNITS = 2.0f;

// The header's columns: the universe and its server at its left, taking what is left; the server's state; and the universe's
// time at its right.
constexpr float STATE_COLUMN_UNITS = 620.0f;
constexpr float TIME_COLUMN_UNITS = 320.0f;

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

HeaderState StateOf(const Outpost::OverseerModel& _model, const Outpost::OverseerLink& _link, std::chrono::steady_clock::time_point _now)
{
  using Outpost::OverseerScreen;
  const Outpost::HeardHealth* latest = _model.LatestHealth();
  if (_link.connected && _model.Welcome().has_value() && latest != nullptr && _now - latest->heard < OverseerScreen::NO_WORD_AFTER)
    return {.name = "Running", .lines = {}};
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

// The header (Game §session.overseer): the universe's folder and its server; the server's state, and why; and universe
// time with its tick.
void LayHeader(Neuron::Canvas& _canvas, const Outpost::OverseerModel& _model, const Outpost::OverseerLink& _link,
               std::chrono::steady_clock::time_point _now)
{
  const Neuron::Theme& theme = Hud::Theme();
  Neuron::LayoutTree& tree = _canvas.Tree();
  (void)_canvas.BeginPanel(
    {.padding = {.left = PADDING, .top = PADDING, .right = PADDING, .bottom = PADDING}, .alignX = Neuron::LayoutAlign::Stretch});
  (void)tree.Begin(Neuron::LayoutKind::Row, {.gapUnits = MARGIN});

  (void)tree.Begin(Neuron::LayoutKind::Column, {.grow = 1.0f});
  Heading(_canvas, "UNIVERSE");
  const std::optional<Outpost::OverseerWelcomeMessage>& welcome = _model.Welcome();
  (void)_canvas.Label(Fitted(welcome.has_value() ? welcome->folderUtf8 : std::string("Not yet told"), Hud::Typeface::Name, NAME_LINE_UNITS),
                      theme.textColor);
  (void)_canvas.Label(Fitted(_link.server, Hud::Typeface::Figure, FIGURE_LINE_UNITS), theme.figureColor);
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
  (void)_canvas.Label(
    Fitted(timed ? std::format("tick {}", Outpost::WithThousands(static_cast<std::int64_t>(latest->health.tick))) : std::string(),
           Hud::Typeface::Figure, FIGURE_LINE_UNITS),
    theme.textColor);
  tree.End();

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
  m_interaction.BeginBuild();
  Neuron::Canvas canvas(Hud::Theme(), _metrics, m_interaction, static_cast<float>(_widthPixels) / scale,
                        static_cast<float>(_heightPixels) / scale);
  Neuron::LayoutTree& tree = canvas.Tree();
  (void)tree.Begin(Neuron::LayoutKind::Column, {.widthUnits = static_cast<float>(_widthPixels) / scale,
                                                .heightUnits = static_cast<float>(_heightPixels) / scale,
                                                .padding = {.left = MARGIN, .top = MARGIN, .right = MARGIN, .bottom = MARGIN},
                                                .gapUnits = MARGIN,
                                                .alignX = Neuron::LayoutAlign::Stretch});
  LayHeader(canvas, _model, _link, _now);
  tree.End();
  m_layout = {};
  canvas.Finish(m_layout);
  return m_layout;
}

void Outpost::OverseerScreen::Input(const Neuron::InputState& _input)
{
  if (!_input.active)
    return;
  const auto x = static_cast<float>(_input.cursorXPixels);
  const auto y = static_cast<float>(_input.cursorYPixels);
  m_interaction.Move(m_layout, x, y);
  if (_input.wheelNotches != 0.0f)
    (void)m_interaction.Wheel(m_layout, x, y, _input.wheelNotches);
  for (const Neuron::InputEvent& event : _input.events)
  {
    if (event.key != VK_LBUTTON)
      continue;
    const auto eventX = static_cast<float>(event.xPixels);
    const auto eventY = static_cast<float>(event.yPixels);
    if (event.kind == Neuron::InputEventKind::ButtonDown)
      (void)m_interaction.Press(m_layout, eventX, eventY);
    else if (event.kind == Neuron::InputEventKind::ButtonUp)
      (void)m_interaction.Release(m_layout, eventX, eventY);
  }
}

std::optional<Outpost::PlayerId> Outpost::OverseerScreen::TakeSight()
{
  return std::exchange(m_sightPicked, std::nullopt);
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
