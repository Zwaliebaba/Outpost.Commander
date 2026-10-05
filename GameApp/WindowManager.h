#pragma once

namespace Outpost
{
// The game's floating windows (Phase 1 design §12, ADR-031). The HUD stays where it is; these float over the battlefield
// and over the HUD.
enum class WindowKind : std::uint8_t
{
  Designer,
  Research,
  Production,
  // Every key and mouse action the game reads (task 16.4).
  Controls
};

inline constexpr std::size_t WINDOW_KINDS = 4;

// Which windows are open, in what order front to back, where each was left and which is being dragged, all in reference
// units (ADR-006). It keeps no geometry of its own: the HUD lays each open window out where this says, clamped to the
// screen, and tells it what a press hit. Positions last for as long as the manager does, which is as long as the game
// runs (owner, 2026-10-02): nothing is written to disk.
class WindowManager
{
public:
  // A point in reference units.
  struct Point
  {
    float xUnits = 0.0f;
    float yUnits = 0.0f;

    friend bool operator==(const Point&, const Point&) = default;
  };

  // How many places the windows that stand under the Ore have, production's and research's (task 15.4).
  static constexpr std::size_t SLOTS = 2;

  // Opens _window at the front, where it was left, or where the HUD puts it first; an open window comes to the front. A
  // production or research window not yet moved takes the first slot no other open window holds (task 15.4).
  void Open(WindowKind _window);
  void Close(WindowKind _window) noexcept;
  [[nodiscard]] bool IsOpen(WindowKind _window) const noexcept;

  // The open windows, the front one first.
  [[nodiscard]] std::span<const WindowKind> FrontToBack() const noexcept
  {
    return m_order;
  }

  // Closes the front window, as Esc does; false when none is open.
  bool CloseFront() noexcept;
  // Closes every window, keeping where each was left, as leaving a match does.
  void CloseAll() noexcept;

  // Where _window's top-left corner was left; nothing until it has been moved, when the HUD places it.
  [[nodiscard]] std::optional<Point> PositionOf(WindowKind _window) const noexcept;
  // Which of the SLOTS an open production or research window not yet moved stands in, the first being production's place
  // (task 15.4); nothing for any other window.
  [[nodiscard]] std::optional<std::size_t> SlotOf(WindowKind _window) const noexcept;

  // A press on _window's title bar at _pointer, while the window's top-left corner stands at _corner: the window comes to
  // the front, and moves with the pointer until Release.
  void Grab(WindowKind _window, Point _pointer, Point _corner);
  // The pointer moved to _pointer: a grabbed window follows it, keeping where it was taken hold of.
  void Drag(Point _pointer) noexcept;
  void Release() noexcept;
  // Where the HUD could lay a moved window out, kept on the screen: it stays there, so that a window dragged past the
  // edge moves again as soon as the pointer comes back, and keeps its place on a smaller screen.
  void Settle(WindowKind _window, Point _corner) noexcept;
  [[nodiscard]] bool IsDragging() const noexcept
  {
    return m_grabbed.has_value();
  }

private:
  [[nodiscard]] static std::size_t IndexOf(WindowKind _window) noexcept
  {
    return static_cast<std::size_t>(_window);
  }

  void BringToFront(WindowKind _window);

  std::vector<WindowKind> m_order;
  std::array<std::optional<Point>, WINDOW_KINDS> m_positions{};
  std::array<std::optional<std::size_t>, WINDOW_KINDS> m_slots{};
  std::optional<WindowKind> m_grabbed;
  // From the grabbed window's top-left corner to where it was taken hold of.
  Point m_grabOffset;
};
} // namespace Outpost
