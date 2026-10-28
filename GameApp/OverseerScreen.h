#pragma once

namespace Outpost
{
// What the overseer shows of its link beside what its model knows (ADR-102): the server it reaches, as Hud::ServerName writes
// it, whether a connection is open, and its round trip as last read.
struct OverseerLink
{
  std::string server;
  bool connected = false;
  std::optional<std::chrono::microseconds> roundTrip;
};

// The overseer's overview (ADR-102; Game §session.overseer), built on the canvas in the game's look (ADR-100) and laid out on
// the 1920×1080 reference frame scaled to the window, as the HUD is (ADR-006 decision 5), from what its model knows. It keeps
// what its widgets share and what the owner picked on it, and no GPU state: the shell draws its display list.
class OverseerScreen
{
public:
  // How long the server may go without a Health before the header says No word.
  static constexpr std::chrono::seconds NO_WORD_AFTER{3};

  // Each seat that has a set drawn in its set's color (Hud::SideColorsOf), the others in the overseer's own palette.
  explicit OverseerScreen(std::vector<Hud::SideColor> _seatColors);

  // Lays the overview out on a back buffer of this size, its text measured with _metrics, the fonts at Hud::Scale's scale for
  // it, as _model knows the universe at _now and _link says of the connection.
  const Neuron::DrawList& Lay(const OverseerModel& _model, const OverseerLink& _link, std::chrono::steady_clock::time_point _now,
                              const Hud::TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  // The frame's input, against the overview as it was last laid out: where the pointer is, its presses and the wheel.
  void Input(const Neuron::InputState& _input);

  // The sight the owner picked since the last call, which the link tells the server: a seat's, or full sight with no player;
  // none when the pick has not changed.
  [[nodiscard]] std::optional<PlayerId> TakeSight();

  [[nodiscard]] const Neuron::DrawList& Layout() const noexcept
  {
    return m_layout;
  }

private:
  std::vector<Hud::SideColor> m_seatColors;
  Neuron::Interaction m_interaction;
  Neuron::DrawList m_layout;
  std::optional<PlayerId> m_sightPicked;
};

// A universe's time, from its tick count at _ticksPerSecond: hours, minutes and seconds, "5:02:03", with the days in front once
// there are any, "3 d 04:05:06".
[[nodiscard]] std::string UniverseTime(std::uint64_t _tick, std::uint32_t _ticksPerSecond);
} // namespace Outpost
