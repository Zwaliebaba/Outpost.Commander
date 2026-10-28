#pragma once

namespace Outpost
{
// The game's floating windows (ADR-031). The HUD stays where it is; these float over the battlefield
// and over the HUD.
enum class WindowKind : std::uint8_t
{
  Designer,
  Research,
  Production,
  // Every key and mouse action the game reads (ADR-068 decision 4).
  Controls,
  // The seat's scheduled orders and the form that gives one, and what happened while the player was away (ADR-080
  // decision 9).
  Orders,
  Away,
  // In a universe, whether to leave it for the menu (ADR-078 decision 11).
  Leave
};

inline constexpr std::size_t WINDOW_KINDS = 7;

// The game's windows on the engine's window stack (ADR-100), which keeps which are open, in what order front to back,
// where each was left and which is being dragged, all in reference units (ADR-006); and which of the places under the
// Ore an unmoved production or research window stands in, which is the game's. It keeps no geometry of its own: the HUD
// lays each open window out where this says, clamped to the screen, and tells it what a press hit. Positions last for as
// long as the manager does, which is as long as the game runs; GameClient keeps the places the player leaves from one
// session to the next, and puts them back with Place (ADR-099).
class WindowManager
{
public:
  // A point in reference units.
  using Point = Neuron::CanvasPoint;

  // How many places the windows that stand under the Ore have, production's and research's (ADR-031 decision 2).
  static constexpr std::size_t SLOTS = 2;

  // Opens _window at the front, where it was left, or where the HUD puts it first; an open window comes to the front. A
  // production or research window not yet moved takes the first slot no other open window holds (ADR-031 decision 2).
  void Open(WindowKind _window);
  void Close(WindowKind _window) noexcept;
  [[nodiscard]] bool IsOpen(WindowKind _window) const noexcept;

  // The open windows, the front one first.
  [[nodiscard]] std::span<const WindowKind> FrontToBack() const noexcept
  {
    return m_stack.FrontToBack();
  }

  // Closes the front window, as Esc does; false when none is open.
  bool CloseFront() noexcept;
  // Closes every window, keeping where each was left, as leaving a match does.
  void CloseAll() noexcept;

  // Where _window's top-left corner was left; nothing until it has been moved, when the HUD places it.
  [[nodiscard]] std::optional<Point> PositionOf(WindowKind _window) const noexcept;
  // Which of the SLOTS an open production or research window not yet moved stands in, the first being production's place
  // (ADR-031 decision 2); nothing for any other window.
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
    return m_stack.IsDragging();
  }
  // The window being dragged, if any.
  [[nodiscard]] std::optional<WindowKind> Grabbed() const noexcept
  {
    return m_stack.Grabbed();
  }
  // Stands _window at _corner, as if the player had left it there, without opening it or bringing it to the front: a place
  // the player left it in is put back so, whatever the screen and the scale (ADR-099). It then takes no slot.
  void Place(WindowKind _window, Point _corner) noexcept;

private:
  [[nodiscard]] static std::size_t IndexOf(WindowKind _window) noexcept
  {
    return static_cast<std::size_t>(_window);
  }

  Neuron::WindowStack<WindowKind, WINDOW_KINDS> m_stack;
  std::array<std::optional<std::size_t>, WINDOW_KINDS> m_slots{};
};
} // namespace Outpost
