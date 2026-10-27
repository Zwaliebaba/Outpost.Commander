#pragma once

namespace Neuron
{
// A point of the canvas in reference units (ADR-006).
struct CanvasPoint
{
  float xUnits = 0.0f;
  float yUnits = 0.0f;

  friend bool operator==(const CanvasPoint&, const CanvasPoint&) = default;
};

// Where a window _widthUnits wide whose top-left corner is asked to stand at _corner may stand on a screen of
// _screenWidthUnits by _screenHeightUnits: moved only as far as keeps its title bar, _titleBarUnits tall, on the screen,
// and _keptUnits of its width, or all of it when it is narrower (ADR-031 decision 5; ADR-100).
[[nodiscard]] CanvasPoint KeepOnScreen(CanvasPoint _corner, float _widthUnits, float _screenWidthUnits, float _screenHeightUnits,
                                       float _keptUnits, float _titleBarUnits) noexcept;

// The canvas's floating windows, each named by an Id below Kinds, such as an enumeration's value (ADR-100): which are
// open, in what order front to back, where each was left and which is being dragged, in reference units. It keeps no
// geometry of its own: the layout lays each open window out where this says, kept on the screen, and tells it what a
// press hit. A place lasts as long as the stack does.
template <class Id, std::size_t Kinds> class WindowStack
{
public:
  // Opens _window at the front, where it was left; an open window comes to the front.
  void Open(Id _window)
  {
    std::erase(m_order, _window);
    m_order.insert(m_order.begin(), _window);
  }

  void Close(Id _window) noexcept
  {
    std::erase(m_order, _window);
    if (m_grabbed == _window)
      m_grabbed.reset();
  }

  [[nodiscard]] bool IsOpen(Id _window) const noexcept
  {
    return std::ranges::find(m_order, _window) != m_order.end();
  }

  // The open windows, the front one first.
  [[nodiscard]] std::span<const Id> FrontToBack() const noexcept
  {
    return m_order;
  }

  // Closes the front window, as Esc does, and names it; nothing when none is open.
  std::optional<Id> CloseFront() noexcept
  {
    if (m_order.empty())
      return std::nullopt;
    const Id front = m_order.front();
    Close(front);
    return front;
  }

  // Closes every window, keeping where each was left.
  void CloseAll() noexcept
  {
    m_order.clear();
    m_grabbed.reset();
  }

  // Where _window's top-left corner was left; nothing until it has been moved or placed, when the layout places it.
  [[nodiscard]] std::optional<CanvasPoint> PositionOf(Id _window) const noexcept
  {
    return m_positions[IndexOf(_window)];
  }

  // A press on _window's title bar at _pointer, while its top-left corner stands at _corner: the window comes to the
  // front, and moves with the pointer until Release.
  void Grab(Id _window, CanvasPoint _pointer, CanvasPoint _corner)
  {
    Open(_window);
    m_positions[IndexOf(_window)] = _corner;
    m_grabbed = _window;
    m_grabOffset = {.xUnits = _pointer.xUnits - _corner.xUnits, .yUnits = _pointer.yUnits - _corner.yUnits};
  }

  // The pointer moved to _pointer: a grabbed window follows it, keeping where it was taken hold of.
  void Drag(CanvasPoint _pointer) noexcept
  {
    if (m_grabbed.has_value())
      m_positions[IndexOf(*m_grabbed)] = {.xUnits = _pointer.xUnits - m_grabOffset.xUnits, .yUnits = _pointer.yUnits - m_grabOffset.yUnits};
  }

  void Release() noexcept
  {
    m_grabbed.reset();
  }

  // Where the layout could lay a moved window out, kept on the screen: it stays there, so that a window dragged past the
  // edge moves again as soon as the pointer comes back, and keeps its place on a smaller screen.
  void Settle(Id _window, CanvasPoint _corner) noexcept
  {
    if (m_positions[IndexOf(_window)].has_value())
      m_positions[IndexOf(_window)] = _corner;
  }

  [[nodiscard]] bool IsDragging() const noexcept
  {
    return m_grabbed.has_value();
  }

  // The window being dragged, if any.
  [[nodiscard]] std::optional<Id> Grabbed() const noexcept
  {
    return m_grabbed;
  }

  // Stands _window at _corner, as if it had been left there, without opening it or bringing it to the front.
  void Place(Id _window, CanvasPoint _corner) noexcept
  {
    m_positions[IndexOf(_window)] = _corner;
  }

private:
  [[nodiscard]] static std::size_t IndexOf(Id _window) noexcept
  {
    return static_cast<std::size_t>(_window);
  }

  std::vector<Id> m_order;
  std::array<std::optional<CanvasPoint>, Kinds> m_positions{};
  std::optional<Id> m_grabbed;
  // From the grabbed window's top-left corner to where it was taken hold of.
  CanvasPoint m_grabOffset;
};
} // namespace Neuron
