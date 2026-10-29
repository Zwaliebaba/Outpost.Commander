#pragma once

namespace Outpost
{
// What the overseer shows of its link beside what its model knows (ADR-103): the server it reaches, as Hud::ServerName writes
// it, whether a connection is open, and its round trip as last read.
struct OverseerLink
{
  std::string server;
  bool connected = false;
  std::optional<std::chrono::microseconds> roundTrip;
};

// The overseer's own colors for the seats past the two whose sets have one (owner, 2026-10-28), clear of the warning's color
// (ADR-085 decision 1), of the pirates' violet and of the two sets', linear: teal, green, yellow, sky, sand and rose.
inline constexpr std::array<DirectX::XMFLOAT4, 6> OVERSEER_SEAT_PALETTE{{{0.1f, 0.72f, 0.66f, 1.0f},
                                                                         {0.38f, 0.78f, 0.2f, 1.0f},
                                                                         {0.92f, 0.82f, 0.18f, 1.0f},
                                                                         {0.5f, 0.78f, 1.0f, 1.0f},
                                                                         {0.78f, 0.66f, 0.44f, 1.0f},
                                                                         {0.92f, 0.4f, 0.62f, 1.0f}}};

// The overseer's windows, which stand over its overview (ADR-103 decision 7): the server's; the seat window, which shows the
// seat last opened; and the window that asks the owner to confirm an action (ADR-104), the one last asked for.
enum class OverseerWindow : std::uint8_t
{
  Server,
  Seat,
  Confirm
};

inline constexpr std::size_t OVERSEER_WINDOW_COUNT = 3;

// The overseer's overview (ADR-103; Game §session.overseer), built on the canvas in the game's look (ADR-100) and laid out on
// the 1920×1080 reference frame scaled to the window, as the HUD is (ADR-006 decision 5), from what its model knows: the
// header, the health tiles, the map, what needs attention, the seat table, who played over the last day, and the universe log;
// and its windows over it. It keeps what its widgets share, what the owner picked on it and where its windows stand, and no
// GPU state: the shell draws its display list.
class OverseerScreen
{
public:
  // How long the server may go without a Health before the header says No word.
  static constexpr std::chrono::seconds NO_WORD_AFTER{3};
  // How long the tick may stand still while Health comes before the header says Stalled.
  static constexpr std::chrono::seconds STALLED_AFTER{2};
  // The tick's budget, which its 99th percentile over the last minute is told against (the budgets).
  static constexpr std::uint32_t TICK_BUDGET_MICROSECONDS = 5'000;
  // How many Health a minute is, one a second, which the tick's percentile and a seat's orders a minute are taken over.
  static constexpr std::size_t MINUTE_OF_HEALTH = 60;
  // How many of the log's records the log shows at most, the newest first, of those its filter keeps.
  static constexpr std::size_t RECORDS_SHOWN = 400;

  // The seat table's columns, in the order they stand.
  enum class SeatColumn : std::uint8_t
  {
    Seat,
    Status,
    Link,
    Orders,
    Ore,
    Fleet,
    Nodes,
    Research
  };
  static constexpr std::size_t SEAT_COLUMN_COUNT = 8;

  // Which seats the table shows: every one, the players' seats, the AI empires', the players' seats whose player is away, or
  // those that something needing attention names (AttentionOf).
  enum class SeatFilter : std::uint8_t
  {
    All,
    Players,
    Ai,
    Away,
    NeedsALook
  };

  // Which of the log's records it shows, by their kind: every one; the seats' (seat, join, lost, restart, waiting, hour and
  // order); the saves (start, save and written); the battles; and the server's own (tick, dropped and overseer).
  enum class LogFilter : std::uint8_t
  {
    All,
    Seats,
    Saves,
    Battles,
    Server
  };

  // Each seat that has a set drawn in its set's color (Hud::SideColorsOf), the others in OVERSEER_SEAT_PALETTE.
  explicit OverseerScreen(std::vector<Hud::SideColor> _seatColors);

  // Lays the overview out on a back buffer of this size, its text measured with _metrics, the fonts at Hud::Scale's scale for
  // it, as _model knows the universe at _now and _link says of the connection.
  const Neuron::DrawList& Lay(const OverseerModel& _model, const OverseerLink& _link, std::chrono::steady_clock::time_point _now,
                              const Hud::TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  // The frame's input, against the overview as it was last laid out: where the pointer is, its presses and the wheel. A press
  // on a heading, a filter's cell, a row, the map's All cell or a button does what the matching call below does; a press on a
  // window's title bar drags it until the button is let go, on its close box closes it, and anywhere in it brings it to the
  // front.
  void Input(const Neuron::InputState& _input);

  // The seat table sorted by _column, ascending, or the other way when it sorts by it already.
  void SortBy(SeatColumn _column);
  void Filter(SeatFilter _filter);
  // _seat picked, whose sight the server sends from here on; or, picked already, full sight again.
  void Pick(PlayerId _seat);
  // Full sight again, and no seat picked, as the map's All cell gives it.
  void WatchFullSight();

  // Opens the server window at the front, as the header's Server button does; the seat window on _seat, as the Seat button
  // of the picked row's action bar does; or closes a window, as its close box does.
  void OpenServerWindow();
  void OpenSeatWindow(PlayerId _seat);
  void CloseWindow(OverseerWindow _window) noexcept;
  [[nodiscard]] bool IsOpen(OverseerWindow _window) const noexcept
  {
    return m_windows.IsOpen(_window);
  }

  // The seat the seat window shows.
  [[nodiscard]] PlayerId SeatShown() const noexcept
  {
    return m_seatShown;
  }

  // The server's actions (ADR-104), asked only of a running server, as the overview was last laid out: Save now, as the
  // header's button asks for it; Stop, which the header's button opens the confirm window on; Drop link, for a player's seat
  // whose link is online, and New token, for a player's seat, which the picked seat's action bar opens the window on; the
  // deputy's choice in Drop link's window, at once or after DEPUTY_DELAY_SECONDS; and the action the window shows, asked for as
  // its first button does, which closes it. Closing the window asks for nothing.
  void SaveNow();
  void AskToStop();
  void AskToDropLink(PlayerId _seat);
  void AskForNewToken(PlayerId _seat);
  void PickDeputyAtOnce(bool _atOnce) noexcept;
  void Confirm();
  [[nodiscard]] const std::optional<ActionMessage>& Confirming() const noexcept
  {
    return m_confirming;
  }

  // The actions the owner asked for since the last call, in order, which the link asks of the server.
  [[nodiscard]] std::vector<ActionMessage> TakeActions();
  void FilterLog(LogFilter _filter);

  // The sight the owner picked since the last call, which the link tells the server: a seat's, or full sight with no player;
  // none when the pick has not changed.
  [[nodiscard]] std::optional<PlayerId> TakeSight();

  [[nodiscard]] std::optional<PlayerId> Picked() const noexcept
  {
    return m_picked;
  }

  // The color _seat is drawn in: its set's, or the overseer's palette's.
  [[nodiscard]] DirectX::XMFLOAT4 SeatColor(PlayerId _seat) const;

  [[nodiscard]] const Neuron::DrawList& Layout() const noexcept
  {
    return m_layout;
  }

private:
  // What a press named, by the payload of the region it fell in.
  void Press(std::uint32_t _payload);
  // Lays _window out on a canvas of its own over what is laid out already, where it was left or where it first opens, with
  // what it shows of _model, and keeps its widgets, its layer and its corner for the next input.
  void LayWindow(OverseerWindow _window, const OverseerModel& _model, const Hud::TextMetrics& _metrics, float _screenWidthUnits,
                 float _screenHeightUnits);

  std::vector<Hud::SideColor> m_seatColors;
  Neuron::Interaction m_interaction;
  // The windows open, front to back, and where each was left; and, as each was last laid out, its title bar's and close box's
  // widgets, its layer of the display list and its top-left corner in reference units, which a drag starts from; and the
  // pixels a reference unit took.
  Neuron::WindowStack<OverseerWindow, OVERSEER_WINDOW_COUNT> m_windows;
  std::array<Neuron::WidgetId, OVERSEER_WINDOW_COUNT> m_titleBars{};
  std::array<Neuron::WidgetId, OVERSEER_WINDOW_COUNT> m_closeBoxes{};
  std::array<std::size_t, OVERSEER_WINDOW_COUNT> m_windowLayers{};
  std::array<Neuron::CanvasPoint, OVERSEER_WINDOW_COUNT> m_windowCorners{};
  float m_scale = 1.0f;
  // Whether the server ran as the overview was last laid out, which the actions are asked of only then, and its players'
  // seats and those whose link was online then; the action the confirm window shows; and the actions asked for that
  // TakeActions has yet to take.
  bool m_running = false;
  std::vector<PlayerId> m_playerSeats;
  std::vector<PlayerId> m_onlineSeats;
  std::optional<ActionMessage> m_confirming;
  std::vector<ActionMessage> m_actions;
  // A HUD's layout, since the map is the minimap's drawing (Hud::DrawMinimap).
  Hud::Layout m_layout;
  SeatColumn m_sortColumn = SeatColumn::Seat;
  bool m_sortDescending = false;
  SeatFilter m_filter = SeatFilter::All;
  LogFilter m_logFilter = LogFilter::All;
  std::optional<PlayerId> m_picked;
  PlayerId m_seatShown;
  std::optional<PlayerId> m_sightPicked;
};

// A universe's time, from its tick count at _ticksPerSecond: hours, minutes and seconds, "5:02:03", with the days in front once
// there are any, "3 d 04:05:06".
[[nodiscard]] std::string UniverseTime(std::uint64_t _tick, std::uint32_t _ticksPerSecond);

// A count of bytes as the overseer writes it: "512 B", "12.4 KB", "182 MB", "1.2 GB", in units of 1,024.
[[nodiscard]] std::string BytesText(std::uint64_t _bytes);
} // namespace Outpost
